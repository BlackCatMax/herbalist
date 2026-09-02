// Core/World/GridWorldManagerTick.cpp
// Полный файл с трассировкой, реплеем и восстановлением экологии

#include "Core/World/GridWorldManager.h"
#include "ProjectHerbalist.h"
#include "HerbalistLogChannels.h"
#include "DrawDebugHelpers.h"
#include "Core/Simulation/Public/CommandTypes.h"
#include "Core/Simulation/Public/DeltaTypes.h"
#include "Core/Simulation/Public/SnapshotService.h"
#include "Core/Simulation/Public/TraceTypes.h"
#include "Core/Simulation/Private/TraceReplay.h"
#include "Core/BiomeGraph/BiomeGraphSubsystem.h"
#include "Core/Journal/HerbalistJournalComponent.h"
#include "Core/Simulation/Private/PerceptionService.h"
#include "Player/HerbalistPlayerController.h"
#include "Core/Config/HerbalistSettings.h"
#include "Core/Subsystems/IngredientRegistrySubsystem.h"

namespace
{
    // Тематика зелья по доминирующей черте существа (21_Journey_And_Artifacts.md
    // §21.4, 2026-09-01, ревизия "Update docs"/"Update artifacts") — черновые
    // эвристики на существующих осях FRealState, не точная спецификация: глава
    // называет только направление ("зелье чистоты источника" и т.п.), не
    // формулу. Рог/Гребень читаются одинаково (оба про "чистоту") — глава не
    // даёт способа различить их механически, не изобретаю разницу сам.
    bool MatchesArtifactPotionType(FName ArtifactID, const FRealState& PotionState)
    {
        if (ArtifactID == FName(TEXT("Рог")) || ArtifactID == FName(TEXT("Гребень")))
        {
            return PotionState.Meta.Purity >= 0.7f;   // "чистота источника" / "чистая вода"
        }
        if (ArtifactID == FName(TEXT("Молодильное яблоко")) || ArtifactID == FName(TEXT("Зеркальце")))
        {
            return PotionState.Meta.Purity >= 0.7f && PotionState.Meta.Distortion <= 0.2f;   // "истина/ясность"
        }
        if (ArtifactID == FName(TEXT("Камень-оберег")))
        {
            return PotionState.Meta.Stability >= 0.7f;
        }
        if (ArtifactID == FName(TEXT("Шапка-невидимка")))
        {
            // "Незаметность/тишина" — ближайшее честное чтение: не громкое
            // (низкий Magnitude), не бросающееся в глаза (низкий Resonance,
            // ось уже читается в проекте как "прозорливость/заметность").
            return PotionState.Magnitude <= 0.5f && PotionState.Meta.Resonance <= 0.3f;
        }
        return false;   // Клубочек — отдельная, не осевая проверка (см. ниже); Фонарь не через Warmth вовсе
    }

    // Клубочек — "зелье, требующее ингредиентов из нескольких биомов, не
    // одного — путешествие как рецепт" (§21.4). Нет per-предметного учёта,
    // откуда конкретно собран ИМЕННО этот экземпляр — приближено через
    // статический AllowedBiomes каждого использованного IngredientID
    // (UIngredientRegistrySubsystem), не точная провенанс-система.
    bool OfferingSpansMultipleBiomes(const TArray<FInventoryItem>& Ingredients, UIngredientRegistrySubsystem* IngredientSubsystem)
    {
        if (!IngredientSubsystem) return false;
        TSet<EBiomeType> Biomes;
        for (const FInventoryItem& Item : Ingredients)
        {
            const FIngredientTableRow* Row = IngredientSubsystem->GetRow(Item.IngredientID);
            if (!Row) continue;
            for (EBiomeType Biome : Row->AllowedBiomes)
            {
                Biomes.Add(Biome);
            }
        }
        return Biomes.Num() >= 2;
    }
}

void AGridWorldManager::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    // Игровые часы — не GetWorld()->GetTimeSeconds() (см. GridWorldManager.h):
    // должны пережить сохранение/загрузку, а движковое время level-relative.
    GameClockSeconds += DeltaTime;

    if (UBiomeGraphSubsystem* Graph = GetWorld()->GetSubsystem<UBiomeGraphSubsystem>())
    {
        Graph->StepSimulation(DeltaTime);
    }

    // ========================================================================
    // ПАЙПЛАЙН НА ФИКСИРОВАННОМ ШАГЕ
    // Command Intake -> Snapshot -> Pipeline -> Delta -> World Apply выполняются
    // с шагом SimulationFixedTimeStep, а не на каждом рендер-кадре — количество
    // симуляционных тиков в секунду больше не зависит от FPS (см. Tick Execution
    // Model в архитектурном документе). При просадке FPS ниже шага может пройти
    // несколько шагов симуляции за один Tick() — цикл ниже их все отработает.
    // ========================================================================
    SimulationTimeAccumulator += DeltaTime;
    while (SimulationTimeAccumulator >= SimulationFixedTimeStep)
    {
        SimulationTimeAccumulator -= SimulationFixedTimeStep;
        RunSimulationStep();
    }

    // ========================================================================
    // ВОССТАНОВЛЕНИЕ ПАРАМЕТРОВ КЛЕТОК (ЭКОЛОГИЯ)
    // Непрерывный процесс релаксации к TargetState, не часть пайплайна —
    // идёт каждый кадр с реальным DeltaTime, как и раньше.
    // ========================================================================
    RegenerateCellParameters(DeltaTime);

    // ========================================================================
    // ПРОЯВЛЕНИЕ СУЩНОСТЕЙ (16_Entity_Manifestation, вертикальный срез)
    // Тот же внепайплайновый принцип, что и у RegenerateCellParameters выше.
    // Суточная фаза для Морочников берётся из GetWorld()->GetTimeSeconds()
    // напрямую (GetTimeOfDay01), отдельных часов больше нет.
    // ========================================================================
    UpdateEntityManifestations(DeltaTime);

    // ========================================================================
    // КАПИЩА (15_Cycles_And_Shrines §15.5) — спад Restoration при небрежении.
    // Рост — в RunSimulationStep ниже, там же, где Травник сопоставляет
    // команды с результатом варки.
    // ========================================================================
    UpdateShrines(DeltaTime);

    // ========================================================================
    // ЗАРЯНА: ФРАГМЕНТЫ ПАМЯТИ И БУЯН (обсуждение в сессии 2026-08-24)
    // ========================================================================
    UpdateMemoryFragments(DeltaTime);

    // Тик всегда активен для вызова нового пайплайна каждый кадр
    SetActorTickEnabled(true);

#if WITH_EDITOR
    if (bEnableDebugDraw)
    {
        DrawGridDebug();
        DrawBiomeGraphDebug();
    }
#endif
}

void AGridWorldManager::RunSimulationStep()
{
    // Снапшот мира и биом-графа для трассировки (если включено)
    FWorldSnapshot PreTickSnapshot;
    FBiomeSnapshot PreBiomeSnapshot;
    if (bEnableTrace)
    {
        PreTickSnapshot = CaptureState();
        if (UBiomeGraphSubsystem* Graph = GetWorld()->GetSubsystem<UBiomeGraphSubsystem>())
        {
            PreBiomeSnapshot = Graph->CaptureState();
        }
    }

    // Построение и выполнение пакета команд нового пайплайна
    const FCommandBatch CmdBatch = Simulation::FSnapshotService::BuildCommandBatch(PendingCommands);
    TArray<FCommandEntry> CommandsCopy = PendingCommands;   // копируем для трейса
    PendingCommands.Empty();
    FStateDelta Delta = Simulation::FSnapshotService::ExecuteTick(CmdBatch);

    // Footprint (14_Biome_Graph.md): Pipeline — чистая функция, сам в
    // UBiomeGraphSubsystem не пишет, только формирует Delta.Footprints.
    // Применение — здесь, вне Pipeline, где обращение к UObject-подсистеме допустимо.
    if (Delta.Footprints.Num() > 0)
    {
        if (UBiomeGraphSubsystem* Graph = GetWorld()->GetSubsystem<UBiomeGraphSubsystem>())
        {
            for (const FBiomeFootprintEntry& Footprint : Delta.Footprints)
            {
                Graph->RecordFootprint(Footprint.BiomeID, Footprint.MorokImpact, Footprint.ZaryanaImpact,
                    Footprint.AxisDelta, SimulationFixedTimeStep);
            }
        }
    }

    // Травник (07_UX §7.2.4, CHANGELOG.md 2026-08-23/24) — вне детерминированного
    // пайплайна, как и Footprint выше: презентационная фиксация, не часть
    // Command/Delta цикла. Сопоставляем команды Harvest/Apply(крафт) из
    // CommandsCopy с добавленными предметами из Delta.InventoryOps по порядку —
    // Pipeline формирует их последовательно 1:1 для одиночного сбора/варки,
    // этого достаточно для v1. Полная привязка результата к исходной команде
    // потребовала бы прокидывать ID команды через весь Pipeline — излишне
    // для журнала, который и так презентационный слой.
    if (Delta.InventoryOps.Num() > 0)
    {
        if (AHerbalistPlayerController* PC = Cast<AHerbalistPlayerController>(GetWorld()->GetFirstPlayerController()))
        {
            if (PC->JournalComponent)
            {
                int32 OpIndex = 0;
                for (const FCommandEntry& Cmd : CommandsCopy)
                {
                    const bool bIsHarvest = Cmd.Primitive == ECommandPrimitive::Harvest;
                    const bool bIsCraft = Cmd.Primitive == ECommandPrimitive::Apply && Cmd.Apply.bIsCrafting;
                    if (!bIsHarvest && !bIsCraft) continue;

                    // Ищем следующий Add-оп в инвентарь игрока (ContainerID 0) —
                    // Ash/BoiledWater из провальной варки тоже сюда попадают,
                    // это осознанно: неудача — тоже опыт, который стоит записать.
                    while (OpIndex < Delta.InventoryOps.Num()
                        && !(Delta.InventoryOps[OpIndex].OpType == EInventoryOpType::Add
                            && Delta.InventoryOps[OpIndex].ContainerID == 0))
                    {
                        ++OpIndex;
                    }
                    if (OpIndex >= Delta.InventoryOps.Num()) break;

                    const FInventoryItem& Produced = Delta.InventoryOps[OpIndex].Ingredient;
                    const float ProducedCoherence = Delta.InventoryOps[OpIndex].Coherence;
                    ++OpIndex;

                    // Заряна, фрагмент CoherentBrew (обсуждение в сессии 2026-08-24) —
                    // тот же момент, где уже читается результат варки для Травника,
                    // не отдельный проход по CommandsCopy.
                    if (bIsCraft && Produced.IngredientID == FName(TEXT("Potion")))
                    {
                        const FIntPoint BrewCell = Cmd.Apply.TargetCell;
                        TryTriggerCoherentBrewFragment(BrewCell, ProducedCoherence,
                            Produced.State.Meta.Distortion, Produced.State.Meta.Purity);
                    }

                    FJournalEntry Entry;
                    Entry.Type = bIsHarvest ? EJournalEntryType::Harvest : EJournalEntryType::Brew;
                    Entry.IngredientID = Produced.IngredientID;
                    Entry.Count = Produced.Count;
                    // Искажённое состояние, замороженное сейчас — см. предупреждение
                    // в JournalTypes.h. WorldRNG, не FMath:: — тот же класс бага
                    // с недетерминированным ГПСЧ уже дважды находился и чинился
                    // в этой сессии (спавн ресурсов, порча инвентаря).
                    Entry.PerceivedState = Simulation::FPerceptionService::PerceiveRealState(Produced.State, WorldRNG, GlobalPerceptionClarity);
                    Entry.BrewOutcome = Produced.BrewOutcome;
                    const FIntPoint TargetCell = bIsHarvest ? Cmd.Harvest.TargetCell : Cmd.Apply.TargetCell;
                    Entry.Cell = TargetCell;
                    if (const FGridCell* Cell = GetCellConst(TargetCell.X, TargetCell.Y))
                    {
                        Entry.Biome = Cell->Biome;
                    }
                    Entry.bWasNight = IsNight();
                    Entry.GameTimeSeconds = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;

                    PC->JournalComponent->AddEntry(Entry);
                }
            }
        }
    }

    // Подношение капищу (15_Cycles_And_Shrines §15.5) — правка по итогам
    // обсуждения в сессии 2026-08-24: варка сама по себе больше не считается
    // подношением (варят всегда в одной точке — котле, у самого факта крафта
    // нет смысловой связи с конкретной клеткой мира). Влиять на клетки могут
    // только применённые зелья (Apply, не крафт) и капища/Гнильники —
    // подношение теперь тоже идёт через Apply: игрок должен физически
    // поднести (применить) зелье на клетку капища, как полил бы им сохнущее
    // дерево. Delta.WorldChanges, не InventoryOps — Apply-на-клетку не кладёт
    // предмет в инвентарь, только меняет Cell.State напрямую.
    if (Delta.WorldChanges.Num() > 0 && Shrines.Num() > 0)
    {
        for (const FCommandEntry& Cmd : CommandsCopy)
        {
            const bool bIsApplyToCell = Cmd.Primitive == ECommandPrimitive::Apply && !Cmd.Apply.bIsCrafting;
            if (!bIsApplyToCell) continue;

            FShrine* Shrine = FindShrineAt(Cmd.Apply.TargetCell);
            if (!Shrine) continue;

            const FGridCell* Modified = Delta.WorldChanges.Find(Cmd.Apply.TargetCell);
            if (!Modified) continue;

            // Чистое зелье (высокая Purity, низкая Corruption) — благословение,
            // скверное — осквернение: тот же знак, что уже определяет "хороший"/
            // "плохой" результат везде в проекте, без отдельной оси качества.
            const UHerbalistSettings* Settings = GetHerbalistSettings();
            const float OfferingGain = Settings ? Settings->ShrineOfferingGain : 0.05f;
            const float DeltaRestoration = OfferingGain * (Modified->State.Meta.Purity - Modified->State.Meta.Corruption);
            Shrine->Restoration = FMath::Clamp(Shrine->Restoration + DeltaRestoration, -1.0f, 1.0f);
        }
    }

    // Подношение "хозяину" места (16_Entity_Manifestation.md §16.3) —
    // 2026-08-29, по прямому запросу пользователя ("давай поработаем над
    // формулой"): тот же канал и тот же знаковый принцип, что подношение
    // капищу выше (Apply-на-клетку-обиталище, не крафт), сознательно БЕЗ
    // капищного спада при небрежении — обсуждалось явно: капище "забывается"
    // как структура со сроком годности, хозяин места — нет, у Respect нет
    // течения времени, только рост/порча от того, что туда принесли.
    // Раньше здесь у Полевика стояла пассивная проверка HarvestStress/Purity
    // клетки без единого явного жеста подношения — расходилась со
    // спецификацией §16.3 ("подношение/уважение"), мигрирована на этот
    // канал тем же решением, не оставлена особым случаем.
    if (Delta.WorldChanges.Num() > 0 && EntityLandmarks.Num() > 0)
    {
        for (const FCommandEntry& Cmd : CommandsCopy)
        {
            const bool bIsApplyToCell = Cmd.Primitive == ECommandPrimitive::Apply && !Cmd.Apply.bIsCrafting;
            if (!bIsApplyToCell) continue;

            FEntityLandmark* Landmark = FindLandmarkAt(Cmd.Apply.TargetCell);
            if (!Landmark) continue;

            const FGridCell* Modified = Delta.WorldChanges.Find(Cmd.Apply.TargetCell);
            if (!Modified) continue;

            const UHerbalistSettings* Settings = GetHerbalistSettings();
            const float OfferingGain = Settings ? Settings->LandmarkOfferingGain : 0.05f;
            const float DeltaRespect = OfferingGain * (Modified->State.Meta.Purity - Modified->State.Meta.Corruption);
            Landmark->Respect = FMath::Clamp(Landmark->Respect + DeltaRespect, -1.0f, 1.0f);
        }
    }

    // Роса Заряны (19_Rosa_Signal.md §19.2, Слой 2) — фиксируем прямое
    // применение зелья на её клетку в этом тике, отдельно от подношений
    // капищам/хозяевам выше: UpdateRosaSignal (GridWorldManagerZaryana.cpp)
    // на следующем опросе должен отличить "игрок сам полил" от "роса
    // дрогнула сама", а не решать это здесь.
    if (Delta.WorldChanges.Num() > 0 && ZaryanaCell != FIntPoint(-1, -1))
    {
        for (const FCommandEntry& Cmd : CommandsCopy)
        {
            if (Cmd.Primitive == ECommandPrimitive::Apply && !Cmd.Apply.bIsCrafting && Cmd.Apply.TargetCell == ZaryanaCell)
            {
                bZaryanaCellTouchedSinceLastPoll = true;
                break;
            }
        }
    }

    // Камень-оберег (21_Journey_And_Artifacts.md §21.3, 2026-09-01) —
    // списывается после ЛЮБОЙ команды Apply (крафт ИЛИ применение на
    // клетку — Bifurcation в ComputeApplyResult не различает их), с
    // которой заряд был активен, независимо от того, спас ли он реально
    // ("не гарантирует успех" — расходуется самим фактом держания во
    // время варки).
    for (const FCommandEntry& Cmd : CommandsCopy)
    {
        if (Cmd.Primitive != ECommandPrimitive::Apply || !Cmd.Apply.bBifurcationCharmActive) continue;

        for (FAcquiredArtifact& Artifact : AcquiredArtifacts)
        {
            if (Artifact.ArtifactID == FName(TEXT("Камень-оберег")) && !Artifact.bBifurcationChargeSpent)
            {
                Artifact.bBifurcationChargeSpent = true;
                UE_LOG(LogHerbalistWorld, Log, TEXT("[Artifact] Камень-оберег charge spent"));
                break;
            }
        }
    }

    // Прогрев артефактов, вариант C (21_Journey_And_Artifacts.md §21.4,
    // 2026-09-01, ревизия "Update docs"/"Update artifacts") — зелье нужного
    // типа, сваренное в родном регионе артефакта при уже проявленной
    // сущности, поднимает Warmth. Тот же гейт (IsLegendaryManifested), что
    // уже определяет проявление сущности и доступность артефакта честным
    // путём — переиспользован, не изобретён новый порог. Фонарь исключён
    // (bWarmsFromGlobalClarity — свой путь через GlobalPerceptionClarity,
    // IsArtifactWarmed).
    if (Delta.InventoryOps.Num() > 0 && AcquiredArtifacts.Num() > 0)
    {
        int32 WarmthOpIndex = 0;
        for (const FCommandEntry& Cmd : CommandsCopy)
        {
            if (Cmd.Primitive != ECommandPrimitive::Apply || !Cmd.Apply.bIsCrafting) continue;

            while (WarmthOpIndex < Delta.InventoryOps.Num()
                && !(Delta.InventoryOps[WarmthOpIndex].OpType == EInventoryOpType::Add
                    && Delta.InventoryOps[WarmthOpIndex].ContainerID == 0))
            {
                ++WarmthOpIndex;
            }
            if (WarmthOpIndex >= Delta.InventoryOps.Num()) break;

            const FInventoryItem& Produced = Delta.InventoryOps[WarmthOpIndex].Ingredient;
            ++WarmthOpIndex;
            if (Produced.IngredientID != FName(TEXT("Potion"))) continue;

            const FGridCell* BrewCell = GetCellConst(Cmd.Apply.TargetCell.X, Cmd.Apply.TargetCell.Y);
            if (!BrewCell) continue;

            for (FAcquiredArtifact& Artifact : AcquiredArtifacts)
            {
                const FArtifactDefinition* Def = FindArtifactDefinition(Artifact.ArtifactID);
                if (!Def || Def->bWarmsFromGlobalClarity || Def->Biome != BrewCell->Biome) continue;

                if (!IsLegendaryManifested(Def->LegendaryEntityID)) continue;

                bool bTypeMatches = false;
                if (Artifact.ArtifactID == FName(TEXT("Клубочек")))
                {
                    UGameInstance* GameInstanceRef = GetGameInstance();
                    UIngredientRegistrySubsystem* IngredientSubsystem = GameInstanceRef ? GameInstanceRef->GetSubsystem<UIngredientRegistrySubsystem>() : nullptr;
                    bTypeMatches = OfferingSpansMultipleBiomes(Cmd.Apply.Ingredients, IngredientSubsystem);
                }
                else
                {
                    bTypeMatches = MatchesArtifactPotionType(Artifact.ArtifactID, Produced.State);
                }
                if (!bTypeMatches) continue;

                const UHerbalistSettings* WarmthSettings = GetHerbalistSettings();
                const float Gain = WarmthSettings ? WarmthSettings->ArtifactWarmthGainPerBrew : 0.2f;
                const float Threshold = WarmthSettings ? WarmthSettings->ArtifactWarmthThreshold : 1.0f;
                Artifact.Warmth = FMath::Min(Artifact.Warmth + Gain, Threshold);
                UE_LOG(LogHerbalistWorld, Log, TEXT("[Artifact] %s Warmth += %.2f (now %.2f)"),
                    *Artifact.ArtifactID.ToString(), Gain, Artifact.Warmth);
            }
        }
    }

    // Запись кадра трассировки
    if (bEnableTrace)
    {
        TraceBuffer.Record(CurrentTickID, PreTickSnapshot, PreBiomeSnapshot, CommandsCopy, Delta);
    }

    CurrentTickID++;
}

void AGridWorldManager::DumpTrace()
{
    TraceBuffer.DumpToLog();
}

void AGridWorldManager::ReplayLastTick()
{
    const FTraceFrame* Frame = TraceBuffer.GetLastFrame();
    if (!Frame)
    {
        UE_LOG(LogHerbalistWorld, Warning, TEXT("ReplayLastTick: No trace frames recorded"));
        return;
    }

    FRandomStream Rng(Frame->WorldSnapshot.WorldSeed);
    Simulation::ReplayAndCompare(*Frame, Rng);
}