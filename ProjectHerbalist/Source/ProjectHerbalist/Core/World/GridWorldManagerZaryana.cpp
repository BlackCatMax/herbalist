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
        // Вычитаем интервал, не обнуляем (аудит 2026-09-05) -- обнуление
        // выбрасывает остаток накопленного времени, реальный интервал опроса
        // становится >= CheckInterval и плывёт с кадровой частотой (окна
        // "выдержано N секунд" внутри TrySpawnStateBasedFragment кормятся
        // номинальным CheckInterval на КАЖДЫЙ вызов -- при обнулении вызовов
        // становится меньше, чем должно быть за реальное игровое время,
        // требуемая выдержка растянулась бы дольше заявленных N секунд).
        FragmentStateCheckAccumulator -= CheckInterval;
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
    const float RawResponse = FMath::Clamp(AvgRestorationRespect - AvgMorokHistory, -ResponseRange, ResponseRange);

    // Сглаженная сходимость (§20.3, 2026-09-02 — см. обоснование ставки у
    // ClarityResponseLerpRate, HerbalistSettings.h): без этого одиночный
    // резкий обвал/взлёт Restoration/Respect по вложенным клеткам между
    // двумя опросами мгновенно снимал/добавлял бы всю ClarityResponseRange
    // разом — заметный скачок за один пятисекундный тик. Экспоненциальный
    // Lerp к сырому Response — тот же приём, что уже Memory.HistoryPurity
    // ("медленная скользящая средняя"), но на два порядка более медленной
    // ставке (HistoryPurity лерпится каждый тик симуляции, ~20 раз в
    // секунду; отклик Clarity — раз в MemoryFragmentStateCheckInterval, 5с).
    const float LerpRate = Settings ? Settings->ClarityResponseLerpRate : 0.002f;
    ClarityResponseSmoothed = FMath::Lerp(ClarityResponseSmoothed, RawResponse, LerpRate);

    // Clarity = max(Якорь, Якорь + Отклик) — отклик не может утащить
    // Clarity ниже уже заработанного якоря, только поднять её выше (§20.3).
    // Гарантия структурная (Max()), не зависит от диапазона/сглаживания
    // выше — даже мгновенно отрицательный ClarityResponseSmoothed не
    // проваливает Clarity ниже Anchor.
    GlobalPerceptionClarity = FMath::Clamp(FMath::Max(ClarityAnchor, ClarityAnchor + ClarityResponseSmoothed), 0.0f, 1.0f);
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

    // Шаг опроса для "выдержано N секунд" ниже (TickSustainedCondition) — эта
    // функция сама не знает, сколько реального/игрового времени прошло с
    // прошлого вызова (её зовут и из UpdateMemoryFragments раз в
    // MemoryFragmentStateCheckInterval, и напрямую из тестов) — считаем один
    // вызов = один шаг опроса номинальной длины настройки, тот же принцип,
    // что уже применяют капищные/энтити-триггеры к "разу в N секунд".
    const float CheckInterval = Settings ? Settings->MemoryFragmentStateCheckInterval : 5.0f;

    // HighCommunityTrust (§17.6, "устойчиво высокая Молва") — проверяем
    // первым, дешевле даже капищ (одно поле, не цикл). "Устойчиво" — теперь
    // выдержано (HerbalistCore::Math::TickSustainedCondition, 2026-09-02),
    // не мгновенный порог: Molva должна оставаться высокой
    // KhlebSolSustainedSeconds подряд (в шагах опроса), один провал условия
    // между опросами сбрасывает накопление целиком. Спавнится у порога
    // Заряны (ZaryanaCell) в буквальном соответствии с текстом фрагмента
    // ("оставили... на пороге, пока я спала") — если её клетка ещё не
    // размещена, фрагмент ждёт следующего опроса вместо спавна в
    // произвольном месте.
    const float HighMolvaThreshold = Settings ? Settings->MemoryFragmentHighMolvaThreshold : 0.5f;
    const float KhlebSolSustained = Settings ? Settings->KhlebSolSustainedSeconds : 30.0f;
    const bool bKhlebSolSustainedLongEnough = HerbalistCore::Math::TickSustainedCondition(
        KhlebSolSustainedMolvaSeconds, Molva >= HighMolvaThreshold, CheckInterval, KhlebSolSustained);
    if (bKhlebSolSustainedLongEnough && ZaryanaCell != FIntPoint(-1, -1))
    {
        const FMemoryFragmentDefinition* BreadSaltDef = HerbalistCore::Zaryana::FindMemoryFragmentDefinition(FName(TEXT("KHLEB_SOL")));
        if (BreadSaltDef && !CollectedFragmentIDs.Contains(BreadSaltDef->ID))
        {
            SpawnMemoryFragmentAt(BreadSaltDef->ID, ZaryanaCell, /*bIsFalse=*/false);
            return;
        }
    }

    // NE_POKHVALILA (17_Hero_And_Community.md §17.7, Смешанный лес) —
    // проверяем следующей, тоже дёшево (один якорь, не цикл). См. комментарий
    // у EMemoryFragmentTrigger::LegendaryZoneResolved (MemoryFragmentTypes.h)
    // — приближение открытого вопроса главы, не точная спецификация.
    {
        const FMemoryFragmentDefinition* Def = HerbalistCore::Zaryana::FindMemoryFragmentDefinition(FName(TEXT("NE_POKHVALILA")));
        if (Def && !CollectedFragmentIDs.Contains(Def->ID) && IsLegendaryManifested(FName(TEXT("Баба-Яга"))))
        {
            const FIntPoint* Anchor = LegendaryAnchors.Find(FName(TEXT("Баба-Яга")));
            if (Anchor)
            {
                SpawnMemoryFragmentAt(Def->ID, *Anchor, /*bIsFalse=*/false);
                return;
            }
        }
    }

    // ShrineRestored — проверяем следующим, капищ мало, дёшево.
    // NEUDOBNAYA_PRAVDA (§17.7, Широколиственный лес) — тот же триггер,
    // скопирован на биом капища; проверяется в том же проходе по Shrines,
    // не отдельным циклом.
    {
        const FMemoryFragmentDefinition* PodnoshenieDef = HerbalistCore::Zaryana::FindMemoryFragmentDefinition(FName(TEXT("PODNOSHENIE")));
        const FMemoryFragmentDefinition* PravdaDef = HerbalistCore::Zaryana::FindMemoryFragmentDefinition(FName(TEXT("NEUDOBNAYA_PRAVDA")));
        for (const FShrine& S : Shrines)
        {
            if (S.Restoration < ShrineThreshold) continue;

            if (PodnoshenieDef && !CollectedFragmentIDs.Contains(PodnoshenieDef->ID))
            {
                SpawnMemoryFragmentAt(PodnoshenieDef->ID, S.Cell, /*bIsFalse=*/false);
                return;
            }

            const FGridCell* ShrineCell = GetCellConst(S.Cell.X, S.Cell.Y);
            if (PravdaDef && ShrineCell && ShrineCell->Biome == EBiomeType::BroadleafForest
                && !CollectedFragmentIDs.Contains(PravdaDef->ID))
            {
                SpawnMemoryFragmentAt(PravdaDef->ID, S.Cell, /*bIsFalse=*/false);
                return;
            }
        }
    }

    // LowLocalDistortion — линейный обход клеток; сетка небольшая (сейчас
    // 20x20), раз в MemoryFragmentStateCheckInterval секунд — не проблема.
    // Собираем ВСЕ подходящие клетки и берём случайную (WorldRNG), не первую
    // встречную — иначе фрагмент почти всегда рождался бы в одном и том же
    // "первом по обходу" углу сетки (найдено при аудите 2026-08-24).
    // TISHINA_LESA (§17.7, Тайга) и OJIDANIE_BURI (§17.7, Тундра, Stability
    // вместо Distortion) — та же линейная развёртка клеток, тот же приём
    // "собрать все подходящие, взять случайную", скопировано на биом.
    const FMemoryFragmentDefinition* QuietDef = HerbalistCore::Zaryana::FindMemoryFragmentDefinition(FName(TEXT("TIKHOE_MESTO")));
    const FMemoryFragmentDefinition* TishinaDef = HerbalistCore::Zaryana::FindMemoryFragmentDefinition(FName(TEXT("TISHINA_LESA")));
    const FMemoryFragmentDefinition* BuriDef = HerbalistCore::Zaryana::FindMemoryFragmentDefinition(FName(TEXT("OJIDANIE_BURI")));
    const float StabilityThreshold = Settings ? Settings->MemoryFragmentHighStabilityThreshold : 0.7f;
    const float TishinaLesaSustained = Settings ? Settings->TishinaLesaSustainedSeconds : 60.0f;
    const float OjidanieBuriSustained = Settings ? Settings->OjidanieBuriSustainedSeconds : 120.0f;
    const bool bNeedTishina = TishinaDef && !CollectedFragmentIDs.Contains(TishinaDef->ID);
    const bool bNeedBuri = BuriDef && !CollectedFragmentIDs.Contains(BuriDef->ID);

    // Утечка по аудиту 2026-09-05: пока bNeedTishina/bNeedBuri истинны, эти
    // per-клеточные карты пополняются FindOrAdd на каждую подходящую по биому
    // клетку на каждом опросе и никогда не уменьшаются -- как только
    // соответствующий фрагмент собран, они больше не нужны НИКОГДА (флаг
    // навсегда false), но без явной очистки продолжают занимать память до
    // конца сессии. Очищаем один раз в момент перехода need->collected.
    if (!bNeedTishina && TishinaLesaHoldSeconds.Num() > 0)
    {
        TishinaLesaHoldSeconds.Empty();
    }
    if (!bNeedBuri && OjidanieBuriHoldSeconds.Num() > 0)
    {
        OjidanieBuriHoldSeconds.Empty();
    }

    const bool bNeedQuiet = QuietDef && !CollectedFragmentIDs.Contains(QuietDef->ID);
    if (bNeedQuiet || bNeedTishina || bNeedBuri)
    {
        TArray<FIntPoint> EligibleCellsQuiet;
        TArray<FIntPoint> EligibleCellsTishina;
        TArray<FIntPoint> EligibleCellsBuri;
        for (const FGridCell& Cell : Cells)
        {
            if (Cell.bIsWater) continue;
            const FIntPoint CellCoord(Cell.X, Cell.Y);
            const bool bLowDistortion = Cell.State.Meta.Distortion < DistortionThreshold;

            // TIKHOE_MESTO остаётся мгновенным порогом намеренно — её
            // собственный текст (§21.1) не требует длительности, только
            // TISHINA_LESA (ниже) делит с ней тот же State-порог, но со
            // своим требованием "выдержано".
            if (bNeedQuiet && bLowDistortion)
            {
                EligibleCellsQuiet.Add(CellCoord);
            }

            // TISHINA_LESA (§17.7: "длительная низкая Distortion в Тайге") —
            // тикаем аккумулятор для КАЖДОЙ клетки Тайги, не только уже
            // подходящих сейчас: провал условия должен сбросить накопление
            // этой конкретной клетки, а не просто не увеличить его.
            if (bNeedTishina && Cell.Biome == EBiomeType::Taiga)
            {
                float& Hold = TishinaLesaHoldSeconds.FindOrAdd(CellCoord);
                if (HerbalistCore::Math::TickSustainedCondition(Hold, bLowDistortion, CheckInterval, TishinaLesaSustained))
                {
                    EligibleCellsTishina.Add(CellCoord);
                }
            }

            // OJIDANIE_BURI (§17.7: "клетка с высокой Stability, удержанной
            // долго") — тот же приём per-клеточного аккумулятора.
            if (bNeedBuri && Cell.Biome == EBiomeType::Tundra)
            {
                const bool bHighStability = Cell.State.Meta.Stability >= StabilityThreshold;
                float& Hold = OjidanieBuriHoldSeconds.FindOrAdd(CellCoord);
                if (HerbalistCore::Math::TickSustainedCondition(Hold, bHighStability, CheckInterval, OjidanieBuriSustained))
                {
                    EligibleCellsBuri.Add(CellCoord);
                }
            }
        }
        if (bNeedTishina && EligibleCellsTishina.Num() > 0)
        {
            const FIntPoint Chosen = EligibleCellsTishina[WorldRNG.RandRange(0, EligibleCellsTishina.Num() - 1)];
            SpawnMemoryFragmentAt(TishinaDef->ID, Chosen, /*bIsFalse=*/false);
            return;
        }
        if (bNeedBuri && EligibleCellsBuri.Num() > 0)
        {
            const FIntPoint Chosen = EligibleCellsBuri[WorldRNG.RandRange(0, EligibleCellsBuri.Num() - 1)];
            SpawnMemoryFragmentAt(BuriDef->ID, Chosen, /*bIsFalse=*/false);
            return;
        }
        if (bNeedQuiet && EligibleCellsQuiet.Num() > 0)
        {
            const FIntPoint Chosen = EligibleCellsQuiet[WorldRNG.RandRange(0, EligibleCellsQuiet.Num() - 1)];
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
    // Класс из карточки (2026-09-02) — пусто = базовый, тот же приём, что у
    // трёх рангов бестиария (SyncManifestedEntityActor).
    TSubclassOf<AMemoryFragmentActor> ClassToSpawn = Def->ActorClass;
    if (!ClassToSpawn) ClassToSpawn = AMemoryFragmentActor::StaticClass();
    AMemoryFragmentActor* Fragment = GetWorld()->SpawnActor<AMemoryFragmentActor>(ClassToSpawn, SpawnPos, FRotator::ZeroRotator);
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

    // Метрика мира с историей (15_Cycles_And_Shrines.md §15.5.1, 2026-09-06)
    // — Буян измеряет Distance_итог (с историей Coherence), не голый
    // снимок: мир, добравшийся до тех же чисел через согласованные варки,
    // должен быть заметно ближе к порогу, чем тот же мир через хаос.
    float SumDistance = 0.0f;
    for (const FGridCell& Cell : Cells)
    {
        SumDistance += HerbalistCore::Math::DistanceWithHistory(Cell.State, Cell.Memory.AverageCoherence);
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

bool AGridWorldManager::TryChooseBuyanPath(EBuyanPath Path)
{
    if (!bBuyanReached || ChosenBuyanPath != EBuyanPath::None || Path == EBuyanPath::None)
    {
        return false;
    }

    if (Path == EBuyanPath::Guardian)
    {
        // Путь 1 (страж) — 18_Ending.md §18.2: высокая GlobalPerceptionClarity
        // И высокая Молва, мгновенный порог. Пути 2/3 — без порога (ниже,
        // в SetChosenBuyanPath не попадают сюда вовсе).
        const UHerbalistSettings* Settings = GetHerbalistSettings();
        const float ClarityThreshold = Settings ? Settings->BuyanGuardianClarityThreshold : 0.7f;
        const float MolvaThreshold = Settings ? Settings->BuyanGuardianMolvaThreshold : 0.5f;
        if (GlobalPerceptionClarity < ClarityThreshold || Molva < MolvaThreshold)
        {
            return false;
        }
    }

    ChosenBuyanPath = Path;
    UE_LOG(LogHerbalistZaryana, Log, TEXT("[Zaryana] === ПУТЬ У БУЯНА ВЫБРАН: %d ==="), (int32)Path);

    // Гарантированный финальный фрагмент, три вариации (18_Ending.md §18.1,
    // 21_Journey_And_Artifacts.md §21.1, 2026-09-01) — доставляется
    // напрямую, тем же событийным приёмом, что уже CoherentBrew
    // (TryTriggerCoherentBrewFragment), не через случайный спавн/false-ролл.
    static const TMap<EBuyanPath, FName> PathToFragmentID = {
        { EBuyanPath::Guardian, FName(TEXT("BUYAN_GUARDIAN")) },
        { EBuyanPath::TradePlaces, FName(TEXT("BUYAN_TRADE_PLACES")) },
        { EBuyanPath::AcceptReality, FName(TEXT("BUYAN_ACCEPT_REALITY")) },
    };
    if (const FName* FragmentID = PathToFragmentID.Find(Path))
    {
        AHerbalistPlayerController* PC = Cast<AHerbalistPlayerController>(GetWorld()->GetFirstPlayerController());
        CollectMemoryFragment(*FragmentID, /*bIsFalse=*/false, PC);
    }

    return true;
}

FName AGridWorldManager::GetActiveFragmentDefinitionID() const
{
    return ActiveFragment.IsValid() ? ActiveFragment->GetDefinitionID() : NAME_None;
}

void AGridWorldManager::SetZaryanaCellIfUnset(const FIntPoint& Cell)
{
    if (ZaryanaCell == FIntPoint(-1, -1))
    {
        ZaryanaCell = Cell;
        SeedRosaCorruptedCircle(Cell);
    }
}

void AGridWorldManager::SeedRosaCorruptedCircle(const FIntPoint& Center)
{
    // §19.4a: "трава полегла неестественно ровным кругом, цвет земли на пару
    // тонов темнее... из этого круга, в чащу, уползает низкое облако
    // Морока" — вертикальный срез: реальная State-порча (не TargetState),
    // спадающая от центра к краю. Намеренно НЕ трогаем TargetState — это
    // остаточный след одного уже случившегося события (облако уже ушло,
    // §19.4a: "оно уже сделало то, зачем приходило"), не активный источник
    // порчи вроде Гнильников; клетка вправе естественно зарасти со временем
    // через RegenerateCellParameters, как и любой другой шрам мира.
    //
    // ЯВНОЕ ИСКЛЮЧЕНИЕ из Single-Writer (найдено аудитом 2026-09-05,
    // задокументировано, не тихо оставлено): единственная точка записи в
    // мир — ApplyStateDelta() — не умеет "только State, не TargetState".
    // FStateDelta::WorldChanges бьёт по обоим сразу (см. ApplyStateDelta,
    // GridWorldManagerCore.cpp — Cell->TargetState = NewCellData.State той
    // же строкой, что и Cell->State), а TargetStateNudges бьёт только по
    // TargetState — ни один существующий канал не даёт нужной здесь
    // семантики "разовый прямой удар по State, цель релаксации не трогать".
    // Заводить новый канал FStateDelta ради ЕДИНСТВЕННОГО вызова редкого
    // нарративного события (круг вокруг Заряны, случается не чаще раза за
    // прохождение — SetZaryanaCellIfUnset ставит клетку один раз и
    // защищена собственной проверкой) — непропорциональная цена; прямая
    // запись здесь остаётся, но как признанное, а не молчаливое отступление.
    // Следствие: это изменение НЕ видно TraceReplay/трассировке — если
    // когда-нибудь понадобится воспроизводимость именно этого события,
    // придётся либо расширять FStateDelta, либо переносить сюда отдельный
    // прогон трассировки.
    const UHerbalistSettings* Settings = GetHerbalistSettings();
    const int32 Radius = Settings ? Settings->RosaCorruptedCircleRadius : 3;
    const float PeakDistortion = Settings ? Settings->RosaCorruptedCirclePeakDistortion : 0.5f;
    const float PeakCorruption = Settings ? Settings->RosaCorruptedCirclePeakCorruption : 0.4f;
    if (Radius <= 0) return;

    for (int32 DY = -Radius; DY <= Radius; ++DY)
    {
        for (int32 DX = -Radius; DX <= Radius; ++DX)
        {
            // Chebyshev-расстояние — тот же "круг" по клеткам, что уже
            // определяет радиус влияния хозяев места в GetZaryanaPerceivedState.
            const int32 Dist = FMath::Max(FMath::Abs(DX), FMath::Abs(DY));
            if (Dist > Radius) continue;

            FGridCell* Cell = GetCell(Center.X + DX, Center.Y + DY);
            if (!Cell) continue;

            // Спадает линейно к краю, полная сила в центре (Dist=0).
            const float Factor = 1.0f - static_cast<float>(Dist) / static_cast<float>(Radius > 0 ? Radius : 1);
            Cell->State.Meta.Distortion = FMath::Clamp(Cell->State.Meta.Distortion + PeakDistortion * Factor, 0.0f, 1.0f);
            Cell->State.Meta.Corruption = FMath::Clamp(Cell->State.Meta.Corruption + PeakCorruption * Factor, 0.0f, 1.0f);
            MarkCellDirty(Cell->X, Cell->Y);
        }
    }

    // Заглушка на постановку (§19.4a: "камера, тайминг, конкретная
    // анимация облака всё ещё открытые вопросы", §19.5) — в проекте нет
    // системы катсцен/Sequencer/Niagara-партиклов для нарратива; облако,
    // уползающее в чащу, остаётся визуальной/партикл-задачей контента,
    // не кода. Текстовая версия сцены — тот же канал, что уже несёт
    // остальной непроизнесённый нарратив Заряны (ShowMemoryRevealText).
    if (AHerbalistPlayerController* PC = GetWorld() ? Cast<AHerbalistPlayerController>(GetWorld()->GetFirstPlayerController()) : nullptr)
    {
        PC->ShowMemoryRevealText(FText::FromString(TEXT(
            "Трава вокруг вас полегла ровным кругом, земля темнее, чем должна быть. "
            "Низкое облако -- не туман, что-то плотнее -- беззвучно уползает в чащу и не оборачивается.")));
    }
}

FRealState AGridWorldManager::ComputeZaryanaBlendedState() const
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

    return Blended;
}

FRealState AGridWorldManager::GetZaryanaPerceivedState(FRandomStream& Rng) const
{
    const FRealState Blended = ComputeZaryanaBlendedState();

    // Честный шум — та же формула/сид-паттерн, что уже AlchemySlotWidget.cpp
    // (свой FRandomStream у вызывающего, не WorldRNG). §19.4: "снижает шум в
    // самом важном показании игры" — роса не привилегированное окно истины.
    //
    // Молодильное яблоко (§21.3, 2026-09-01) — расходуемое временное окно
    // полного гашения шума (эффективная Clarity = 1.0, NoiseScale=0), не
    // трогает саму GlobalPerceptionClarity (постоянная прогрессия) —
    // "короткая версия того, что Clarity даёт навсегда".
    const float EffectiveClarity = (GameClockSeconds < YouthAppleClarityBoostExpiryGameSeconds)
        ? 1.0f : GlobalPerceptionClarity;
    return Simulation::FPerceptionService::PerceiveRealState(Blended, Rng, EffectiveClarity);
}

FRealState AGridWorldManager::GetZaryanaTrueState() const
{
    // Перо Гамаюна / прогретое Зеркальце (§16.4/§21.4) — честное чтение,
    // без шума PerceiveRealState вовсе. Та же прямая честность, что уже
    // UseHornOnCell/UseLanternDisclosureOnCell применяют к клеткам, здесь —
    // к Заряне.
    return ComputeZaryanaBlendedState();
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
    UE_LOG(LogHerbalistZaryana, Log, TEXT("Buyan reached: %s (path: %d)"), bBuyanReached ? TEXT("true") : TEXT("false"), (int32)ChosenBuyanPath);
    UE_LOG(LogHerbalistZaryana, Log, TEXT("Collected fragments (%d):"), CollectedFragmentIDs.Num());
    for (FName ID : CollectedFragmentIDs)
    {
        UE_LOG(LogHerbalistZaryana, Log, TEXT("  - %s"), *ID.ToString());
    }
    UE_LOG(LogHerbalistZaryana, Log, TEXT("Active fragment in world: %s"),
        ActiveFragment.IsValid() ? *ActiveFragment->GetDefinitionID().ToString() : TEXT("none"));
}
