// Core/World/GridWorldManagerZaryana.cpp
//
// Заряна: фрагменты памяти и Буян (обсуждение в сессии 2026-08-24,
// 06_Progression.md "Прогрессия через Заряну", 15_Cycles_And_Shrines.md §15.5
// "Буян как глобальное состояние"). Прототип пользователя описывал полный
// авторский конвейер (EventOutbox/CommandBus/AssetCatalog/RuleSet) —
// адаптировано на уже существующие каналы этого проекта: внепайплайновый тик
// (как UpdateEntityManifestations/UpdateShrines), мировые акторы (как
// AHerbalistResourceActor), запись текста воспоминания — через UE_LOG (v1,
// полноценный экран "читать воспоминание" — следующий шаг, не в этом проходе).

#include "Core/World/GridWorldManager.h"
#include "Core/Zaryana/MemoryFragmentActor.h"
#include "Core/Zaryana/MemoryFragmentDefinitions.h"
#include "Core/Config/HerbalistSettings.h"
#include "Core/Types/HerbalistCoreMath.h"
#include "Core/Journal/HerbalistJournalComponent.h"
#include "Core/Journal/JournalTypes.h"
#include "Core/BiomeGraph/BiomeGraphSubsystem.h"
#include "Core/Simulation/Private/PerceptionService.h"
#include "Player/HerbalistPlayerController.h"
#include "ProjectHerbalist.h"
#include "HerbalistLogChannels.h"

void AGridWorldManager::UpdateMemoryFragments(float DeltaTime)
{
    if (FragmentSpawnCooldownRemaining > 0.0f)
    {
        FragmentSpawnCooldownRemaining -= DeltaTime;
    }

    // Событийный триггер (CoherentBrew) живёт в RunSimulationStep, здесь —
    // только State-триггеры (LowLocalDistortion/ShrineRestored), throttled:
    // сканировать всю сетку каждый кадр незачем.
    FragmentStateCheckAccumulator += DeltaTime;
    const UHerbalistSettings* Settings = GetHerbalistSettings();
    const float CheckInterval = Settings ? Settings->MemoryFragmentStateCheckInterval : 5.0f;
    if (FragmentStateCheckAccumulator >= CheckInterval)
    {
        FragmentStateCheckAccumulator = 0.0f;
        RecomputeGlobalPerceptionClarity();
        UpdateRosaSignal();
        TrySpawnStateBasedFragment();
        CheckBuyanCondition();
    }
}

void AGridWorldManager::RecomputeGlobalPerceptionClarity()
{
    // Отклик = среднее Restoration (капища) + Respect (хозяева) минус
    // средний MorokHistory по узлам биом-графа (§20.3: "минус недавние
    // Catastrophe-исходы" — отдельного счётчика Catastrophe в проекте нет,
    // BiomeGraphSubsystem::RecordFootprint уже копит эквивалентный сигнал в
    // FBiomeMemory::MorokHistory, экспоненциально затухающий сам по себе).
    // Толкование "вложенных клеток" как ВСЕХ Shrines+EntityLandmarks, не
    // только тех, куда игрок реально вкладывался — вертикальный срез, не
    // полная семантика главы (в коде нет понятия "вложенная клетка").
    float SumRestorationRespect = 0.0f;
    int32 CountRestorationRespect = 0;
    for (const FShrine& S : Shrines)
    {
        SumRestorationRespect += S.Restoration;
        ++CountRestorationRespect;
    }
    for (const FEntityLandmark& L : EntityLandmarks)
    {
        SumRestorationRespect += L.Respect;
        ++CountRestorationRespect;
    }
    const float AvgRestorationRespect = CountRestorationRespect > 0 ? SumRestorationRespect / CountRestorationRespect : 0.0f;

    float AvgMorokHistory = 0.0f;
    if (UBiomeGraphSubsystem* Graph = GetWorld() ? GetWorld()->GetSubsystem<UBiomeGraphSubsystem>() : nullptr)
    {
        const TMap<FName, FBiomeGraphNode>& Nodes = Graph->GetNodes();
        if (Nodes.Num() > 0)
        {
            float SumMorokHistory = 0.0f;
            for (const auto& NodePair : Nodes)
            {
                SumMorokHistory += NodePair.Value.Memory.MorokHistory;
            }
            AvgMorokHistory = SumMorokHistory / Nodes.Num();
        }
    }

    const UHerbalistSettings* Settings = GetHerbalistSettings();
    const float ResponseRange = Settings ? Settings->ClarityResponseRange : 0.2f;
    const float Response = FMath::Clamp(AvgRestorationRespect - AvgMorokHistory, -ResponseRange, ResponseRange);

    // Clarity = max(Якорь, Якорь + Отклик) — отклик не может утащить
    // Clarity ниже уже заработанного якоря, только поднять её выше (§20.3).
    GlobalPerceptionClarity = FMath::Clamp(FMath::Max(ClarityAnchor, ClarityAnchor + Response), 0.0f, 1.0f);
}

void AGridWorldManager::TrySpawnStateBasedFragment()
{
    // v1: не больше одного активного фрагмента и общий кулдаун между спавнами —
    // редкое, особое событие, не постоянный источник дохода (§15.5: Буян —
    // "приближение, а не разовое действие", тот же принцип задаёт темп и здесь).
    if (ActiveFragment.IsValid() || FragmentSpawnCooldownRemaining > 0.0f) return;

    const UHerbalistSettings* Settings = GetHerbalistSettings();
    const float DistortionThreshold = Settings ? Settings->MemoryFragmentLowDistortionThreshold : 0.15f;
    const float ShrineThreshold = Settings ? Settings->MemoryFragmentShrineRestorationThreshold : 0.7f;

    // ShrineRestored — проверяем первым, капищ мало, дёшево.
    for (const FShrine& S : Shrines)
    {
        if (S.Restoration < ShrineThreshold) continue;
        const FMemoryFragmentDefinition* Def = HerbalistCore::Zaryana::FindMemoryFragmentDefinition(FName(TEXT("PODNOSHENIE")));
        if (Def && !CollectedFragmentIDs.Contains(Def->ID))
        {
            SpawnMemoryFragmentAt(Def->ID, S.Cell, /*bIsFalse=*/false);
            return;
        }
    }

    // LowLocalDistortion — линейный обход клеток; сетка небольшая (сейчас
    // 20x20), раз в MemoryFragmentStateCheckInterval секунд — не проблема.
    // Собираем ВСЕ подходящие клетки и берём случайную (WorldRNG), не первую
    // встречную — иначе фрагмент почти всегда рождался бы в одном и том же
    // "первом по обходу" углу сетки (найдено при аудите 2026-08-24).
    const FMemoryFragmentDefinition* QuietDef = HerbalistCore::Zaryana::FindMemoryFragmentDefinition(FName(TEXT("TIKHOE_MESTO")));
    if (QuietDef && !CollectedFragmentIDs.Contains(QuietDef->ID))
    {
        TArray<FIntPoint> EligibleCells;
        for (const FGridCell& Cell : Cells)
        {
            if (Cell.bIsWater) continue;
            if (Cell.State.Meta.Distortion < DistortionThreshold)
            {
                EligibleCells.Add(FIntPoint(Cell.X, Cell.Y));
            }
        }
        if (EligibleCells.Num() > 0)
        {
            const FIntPoint Chosen = EligibleCells[WorldRNG.RandRange(0, EligibleCells.Num() - 1)];
            SpawnMemoryFragmentAt(QuietDef->ID, Chosen, /*bIsFalse=*/false);
            return;
        }
    }
}

void AGridWorldManager::TryTriggerCoherentBrewFragment(const FIntPoint& Cell, float Coherence, float Distortion, float Purity)
{
    if (ActiveFragment.IsValid() || FragmentSpawnCooldownRemaining > 0.0f) return;

    const UHerbalistSettings* Settings = GetHerbalistSettings();
    const float CoherenceThreshold = Settings ? Settings->MemoryFragmentBrewCoherenceThreshold : 0.8f;
    const float DistortionCeiling = Settings ? Settings->MemoryFragmentBrewDistortionCeiling : 0.2f;
    if (Coherence < CoherenceThreshold || Distortion > DistortionCeiling) return;

    const FMemoryFragmentDefinition* Def = HerbalistCore::Zaryana::FindMemoryFragmentDefinition(FName(TEXT("PERVAYA_VARKA")));
    if (!Def || CollectedFragmentIDs.Contains(Def->ID)) return;

    SpawnMemoryFragmentAt(Def->ID, Cell, /*bIsFalse=*/false);
}

void AGridWorldManager::SpawnMemoryFragmentAt(FName DefinitionID, const FIntPoint& Cell, bool bIsFalse)
{
    const FMemoryFragmentDefinition* Def = HerbalistCore::Zaryana::FindMemoryFragmentDefinition(DefinitionID);
    if (!Def) return;

    // "При высоком глобальном Morok фрагмент может проявиться как искажённый" —
    // подлинный триггер всё равно рискует стать ложным версией того же ID.
    // Считаем средний Distortion по не-водным клеткам как грубую оценку
    // "глобального Морока" — отдельного агрегата в проекте для этого нет,
    // заводить его ради одной проверки раз в 5 секунд не стоило.
    const UHerbalistSettings* Settings = GetHerbalistSettings();
    bool bActuallyFalse = bIsFalse;
    if (!bActuallyFalse && Def->Trigger != EMemoryFragmentTrigger::ShrineRestored)
    {
        float SumDistortion = 0.0f;
        int32 Count = 0;
        for (const FGridCell& C : Cells)
        {
            if (C.bIsWater) continue;
            SumDistortion += C.State.Meta.Distortion;
            ++Count;
        }
        const float AvgDistortion = Count > 0 ? SumDistortion / Count : 0.0f;
        const float FalseRisk = Settings ? Settings->MemoryFragmentFalseRiskGlobalDistortion : 0.5f;
        if (AvgDistortion > FalseRisk && WorldRNG.FRand() < (AvgDistortion - FalseRisk))
        {
            bActuallyFalse = true;
        }
    }

    if (!GetWorld()) return;
    const FVector SpawnPos = GetCellWorldPosition(Cell.X, Cell.Y) + FVector(0, 0, 30.0f);
    AMemoryFragmentActor* Fragment = GetWorld()->SpawnActor<AMemoryFragmentActor>(AMemoryFragmentActor::StaticClass(), SpawnPos, FRotator::ZeroRotator);
    if (!Fragment) return;

    const float Lifetime = Settings ? Settings->MemoryFragmentLifetimeSeconds : 120.0f;
    Fragment->Init(DefinitionID, bActuallyFalse, Lifetime, this, Cell.X, Cell.Y);
    ActiveFragment = Fragment;
    FragmentSpawnCooldownRemaining = Settings ? Settings->MemoryFragmentSpawnCooldownSeconds : 300.0f;

    UE_LOG(LogHerbalistZaryana, Log, TEXT("[Zaryana] Fragment %s%s spawned at (%d,%d)"),
        *DefinitionID.ToString(), bActuallyFalse ? TEXT(" (FALSE)") : TEXT(""), Cell.X, Cell.Y);
}

void AGridWorldManager::CollectMemoryFragment(FName DefinitionID, bool bIsFalse, AHerbalistPlayerController* PC, const FIntPoint& Cell)
{
    const FMemoryFragmentDefinition* Def = HerbalistCore::Zaryana::FindMemoryFragmentDefinition(DefinitionID);
    if (!Def) return;

    const UHerbalistSettings* Settings = GetHerbalistSettings();
    const FText& RevealText = bIsFalse ? Def->FalseText : Def->TrueText;

    if (bIsFalse)
    {
        // Ложное воспоминание — не блокирует будущий подлинный спавн того же
        // ID (CollectedFragmentIDs не трогаем). Изменение поведения
        // (2026-09-01, §20.3 "якорь + отклик"): прямой штраф Clarity убран
        // сознательно, не потерян — якорь монотонен по спецификации главы,
        // штрафовать в нём больше нечего, а волатильный отклик и так честно
        // отражает состояние мира на следующем пересчёте. Ложный фрагмент
        // остаётся вредным иначе — он лжёт текстом, не крадёт прогресс.
        UE_LOG(LogHerbalistZaryana, Warning, TEXT("[Zaryana] Ложное воспоминание (%s): \"%s\""),
            *DefinitionID.ToString(), *Def->FalseText.ToString());
    }
    else
    {
        CollectedFragmentIDs.Add(DefinitionID);
        ClarityAnchor = FMath::Clamp(ClarityAnchor + Def->ClarityGain, 0.0f, 1.0f);
        RecomputeGlobalPerceptionClarity();
        UE_LOG(LogHerbalistZaryana, Log, TEXT("[Zaryana] Подлинное воспоминание (%s): \"%s\" (Anchor=%.2f, Clarity=%.2f)"),
            *DefinitionID.ToString(), *Def->TrueText.ToString(), ClarityAnchor, GlobalPerceptionClarity);
    }

    // Экран + Травник, "Прогрессия/Заряна" 2026-08-29 — раньше сбор был виден
    // только через UE_LOG, текст воспоминания игрок никогда не читал. PC
    // может быть nullptr (Herbalist.Zaryana.* тесты вызывают этот метод
    // напрямую на голом AGridWorldManager, без контроллера) — оба шага
    // защищены проверкой.
    if (PC)
    {
        PC->ShowMemoryRevealText(RevealText);

        if (PC->JournalComponent)
        {
            FJournalEntry Entry;
            Entry.Type = EJournalEntryType::MemoryFragment;
            Entry.FragmentText = RevealText;
            Entry.bFragmentWasTrue = !bIsFalse;
            Entry.Cell = Cell;
            if (const FGridCell* FoundCell = GetCellConst(Cell.X, Cell.Y))
            {
                Entry.Biome = FoundCell->Biome;
            }
            Entry.bWasNight = IsNight();
            Entry.GameTimeSeconds = GameClockSeconds;
            PC->JournalComponent->AddEntry(Entry);
        }
    }

    ActiveFragment.Reset();
}

void AGridWorldManager::CheckBuyanCondition()
{
    if (bBuyanReached) return;   // не переоцениваем — Буян не мигает туда-обратно

    if (Cells.Num() == 0) return;

    float SumDistance = 0.0f;
    for (const FGridCell& Cell : Cells)
    {
        SumDistance += HerbalistCore::Math::Distance(Cell.State, FAlatyr::S0);
    }
    const float AvgDistance = SumDistance / Cells.Num();

    const UHerbalistSettings* Settings = GetHerbalistSettings();
    const float DistanceThreshold = Settings ? Settings->BuyanAverageDistanceThreshold : 0.5f;
    const float ShrineThreshold = Settings ? Settings->BuyanShrineRestorationThreshold : 0.7f;

    if (AvgDistance > DistanceThreshold) return;

    // "Капища восстановлены" — все зарегистрированные, не только часть. Пока
    // капище одно (жилище игрока) это тривиально по конструкции; заложено
    // под будущие мастерские (пользователь: "пока заложим под будущие", число
    // ещё не решено).
    for (const FShrine& S : Shrines)
    {
        if (S.Restoration < ShrineThreshold) return;
    }

    bBuyanReached = true;
    UE_LOG(LogHerbalistZaryana, Log, TEXT("[Zaryana] === БУЯН ДОСТИГНУТ === (AvgDistance=%.3f)"), AvgDistance);
    // Открытие скрытой локации с живой/мёртвой водой — контентная задача
    // (level design), не код: см. DESIGN_World_State.md, раздел про Буян.
    // Здесь — только флаг, который такой контент сможет прочитать.

    // Текстовое объявление на экране, "Прогрессия/Заряна" 2026-08-29 — тот же
    // разрыв, что у фрагментов: сам факт раньше был виден только через
    // UE_LOG/ShowZaryanaStatus (Exec-команда, не часть обычной игры). Не через
    // CollectMemoryFragment (это состояние мира, не действие игрока) — читаем
    // PC тем же приёмом, что уже применён в GridWorldManagerAlchemy.cpp/
    // GridWorldManagerDebug.cpp/GridWorldManagerTick.cpp.
    if (AHerbalistPlayerController* PC = Cast<AHerbalistPlayerController>(GetWorld()->GetFirstPlayerController()))
    {
        PC->ShowMemoryRevealText(FText::FromString(TEXT(
            "Морок стих. Впервые за долгий срок мир вокруг ровен, как гладь непотревоженной воды -- "
            "будто где-то там, за пределами видимого, лежит Буян.")));
    }
}

void AGridWorldManager::SetZaryanaCellIfUnset(const FIntPoint& Cell)
{
    if (ZaryanaCell == FIntPoint(-1, -1))
    {
        ZaryanaCell = Cell;
    }
}

FRealState AGridWorldManager::GetZaryanaPerceivedState(FRandomStream& Rng) const
{
    const FGridCell* Cell = GetCellConst(ZaryanaCell.X, ZaryanaCell.Y);
    if (!Cell)
    {
        // ZaryanaCell ещё не размещена (ни левел-дизайнером, ни
        // AAlchemyTableActor::BeginPlay) — нейтральный дефолт, не крэш.
        return FAlatyr::S0;
    }

    // Слой 1 (§19.2): её собственная клетка, честный FRealState.
    FRealState Blended = Cell->State;

    // Слой 3 (§19.2): радиус чувствительности растёт с Clarity —
    // "восстановленное капище на другом краю карты чуть светлит её кожу".
    // Тот же HerbalistCore::Shrine::GetInfluenceAt, что уже использует
    // остальной проект (GridWorldManagerEntities.cpp/PipelineV2.cpp), только
    // с радиусом, растущим по Clarity, а не фиксированным ShrineInfluenceRadius.
    const UHerbalistSettings* Settings = GetHerbalistSettings();
    const float BaseRadius = Settings ? Settings->RosaBaseRadius : 3.0f;
    const float RadiusPerClarity = Settings ? Settings->RosaRadiusPerClarity : 15.0f;
    const int32 Radius = FMath::RoundToInt(BaseRadius + GlobalPerceptionClarity * RadiusPerClarity);

    const float ShrineInfluence = HerbalistCore::Shrine::GetInfluenceAt(ZaryanaCell, Shrines, Radius);

    // Хозяева места в радиусе — тот же знаковый принцип (Respect), простое
    // среднее по найденным: вертикальный срез, не полноценная модель веса
    // по расстоянию.
    float SumLandmarkRespect = 0.0f;
    int32 LandmarkCount = 0;
    for (const FEntityLandmark& L : EntityLandmarks)
    {
        const int32 Dist = FMath::Max(FMath::Abs(ZaryanaCell.X - L.Cell.X), FMath::Abs(ZaryanaCell.Y - L.Cell.Y));
        if (Dist > Radius) continue;
        SumLandmarkRespect += L.Respect;
        ++LandmarkCount;
    }
    const float LandmarkInfluence = LandmarkCount > 0 ? SumLandmarkRespect / LandmarkCount : 0.0f;
    const float DistantInfluence = (ShrineInfluence + LandmarkInfluence) * 0.5f;

    // Тот же знаковый принцип, что подношение капищу/хозяину
    // (GridWorldManagerTick.cpp): благое — светлит (Purity), скверное —
    // темнит (Corruption). Небольшой множитель — это фоновый подмес
    // отдалённого влияния, не замена Слоя 1.
    Blended.Meta.Purity = FMath::Clamp(Blended.Meta.Purity + FMath::Max(DistantInfluence, 0.0f) * 0.1f, 0.0f, 1.0f);
    Blended.Meta.Corruption = FMath::Clamp(Blended.Meta.Corruption + FMath::Max(-DistantInfluence, 0.0f) * 0.1f, 0.0f, 1.0f);

    // Честный шум — та же формула/сид-паттерн, что уже AlchemySlotWidget.cpp
    // (свой FRandomStream у вызывающего, не WorldRNG). §19.4: "снижает шум в
    // самом важном показании игры" — роса не привилегированное окно истины.
    return Simulation::FPerceptionService::PerceiveRealState(Blended, Rng, GlobalPerceptionClarity);
}

void AGridWorldManager::UpdateRosaSignal()
{
    const FGridCell* Cell = GetCellConst(ZaryanaCell.X, ZaryanaCell.Y);
    if (!Cell)
    {
        bZaryanaCellTouchedSinceLastPoll = false;
        return;
    }

    const float CurrentMagnitude = HerbalistCore::Math::Distance(Cell->State, FAlatyr::S0);

    if (!bRosaBaselineCaptured)
    {
        // Первый опрос после размещения клетки — нет предыдущего значения
        // для сравнения, только фиксируем точку отсчёта.
        LastRosaRealMagnitude = CurrentMagnitude;
        bRosaBaselineCaptured = true;
        bZaryanaCellTouchedSinceLastPoll = false;
        return;
    }

    // Слой 2 (§19.2, "не специфицирован главой жёстко" — решение по месту,
    // см. план): роса поменялась сама, без прямого применения зелья на
    // ZaryanaCell с прошлого опроса (флаг выставляется в
    // GridWorldManagerTick.cpp::RunSimulationStep) — реальный мир вокруг
    // неё дрейфует непрерывно и без игрока (релаксация к TargetState,
    // биом-граф, эффекты проявленных сущностей), это и создаёт совпадение.
    // Разовая метка на партию, не постоянная механика.
    const bool bChanged = !FMath::IsNearlyEqual(CurrentMagnitude, LastRosaRealMagnitude, KINDA_SMALL_NUMBER);
    if (bChanged && !bZaryanaCellTouchedSinceLastPoll && !bRosaFirstFalseSignalShown)
    {
        bRosaFirstFalseSignalShown = true;
        UE_LOG(LogHerbalistZaryana, Log, TEXT("[Zaryana] Роса дрогнула сама собой -- первое совпадение (Слой 2, §19.2)"));
        if (AHerbalistPlayerController* PC = Cast<AHerbalistPlayerController>(GetWorld()->GetFirstPlayerController()))
        {
            PC->ShowMemoryRevealText(FText::FromString(TEXT(
                "Роса на её коже дрогнула сама собой -- ты в этом не при чём. "
                "Совпадение? Или мир шире, чем кажется отсюда?")));
        }
    }

    LastRosaRealMagnitude = CurrentMagnitude;
    bZaryanaCellTouchedSinceLastPoll = false;
}

void AGridWorldManager::ShowZaryanaStatus()
{
    UE_LOG(LogHerbalistZaryana, Log, TEXT("=== ZARYANA STATUS ==="));
    UE_LOG(LogHerbalistZaryana, Log, TEXT("GlobalPerceptionClarity: %.2f (Anchor: %.2f)"), GlobalPerceptionClarity, ClarityAnchor);
    UE_LOG(LogHerbalistZaryana, Log, TEXT("ZaryanaCell: %s, first false Rosa signal shown: %s"),
        *ZaryanaCell.ToString(), bRosaFirstFalseSignalShown ? TEXT("true") : TEXT("false"));
    UE_LOG(LogHerbalistZaryana, Log, TEXT("Buyan reached: %s"), bBuyanReached ? TEXT("true") : TEXT("false"));
    UE_LOG(LogHerbalistZaryana, Log, TEXT("Collected fragments (%d):"), CollectedFragmentIDs.Num());
    for (FName ID : CollectedFragmentIDs)
    {
        UE_LOG(LogHerbalistZaryana, Log, TEXT("  - %s"), *ID.ToString());
    }
    UE_LOG(LogHerbalistZaryana, Log, TEXT("Active fragment in world: %s"),
        ActiveFragment.IsValid() ? *ActiveFragment->GetDefinitionID().ToString() : TEXT("none"));
}
