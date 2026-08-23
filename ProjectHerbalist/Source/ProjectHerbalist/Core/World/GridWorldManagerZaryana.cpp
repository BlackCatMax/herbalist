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
        TrySpawnStateBasedFragment();
        CheckBuyanCondition();
    }
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

    UE_LOG(LogHerbalistWorld, Log, TEXT("[Zaryana] Fragment %s%s spawned at (%d,%d)"),
        *DefinitionID.ToString(), bActuallyFalse ? TEXT(" (FALSE)") : TEXT(""), Cell.X, Cell.Y);
}

void AGridWorldManager::CollectMemoryFragment(FName DefinitionID, bool bIsFalse, AHerbalistPlayerController* PC)
{
    const FMemoryFragmentDefinition* Def = HerbalistCore::Zaryana::FindMemoryFragmentDefinition(DefinitionID);
    if (!Def) return;

    const UHerbalistSettings* Settings = GetHerbalistSettings();

    if (bIsFalse)
    {
        // Ложное воспоминание — не блокирует будущий подлинный спавн того же
        // ID (CollectedFragmentIDs не трогаем) и слегка портит уже набранную
        // ясность, а не даёт её.
        GlobalPerceptionClarity = FMath::Max(GlobalPerceptionClarity - Def->ClarityGain * 0.5f, 0.0f);
        UE_LOG(LogHerbalistWorld, Warning, TEXT("[Zaryana] Ложное воспоминание (%s): \"%s\""),
            *DefinitionID.ToString(), *Def->FalseText.ToString());
    }
    else
    {
        CollectedFragmentIDs.Add(DefinitionID);
        GlobalPerceptionClarity = FMath::Clamp(GlobalPerceptionClarity + Def->ClarityGain, 0.0f, 1.0f);
        UE_LOG(LogHerbalistWorld, Log, TEXT("[Zaryana] Подлинное воспоминание (%s): \"%s\" (Clarity=%.2f)"),
            *DefinitionID.ToString(), *Def->TrueText.ToString(), GlobalPerceptionClarity);
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
    UE_LOG(LogHerbalistWorld, Log, TEXT("[Zaryana] === БУЯН ДОСТИГНУТ === (AvgDistance=%.3f)"), AvgDistance);
    // Открытие скрытой локации с живой/мёртвой водой — контентная задача
    // (level design), не код: см. DESIGN_World_State.md, раздел про Буян.
    // Здесь — только флаг, который такой контент сможет прочитать.
}

void AGridWorldManager::ShowZaryanaStatus()
{
    UE_LOG(LogHerbalistWorld, Log, TEXT("=== ZARYANA STATUS ==="));
    UE_LOG(LogHerbalistWorld, Log, TEXT("GlobalPerceptionClarity: %.2f"), GlobalPerceptionClarity);
    UE_LOG(LogHerbalistWorld, Log, TEXT("Buyan reached: %s"), bBuyanReached ? TEXT("true") : TEXT("false"));
    UE_LOG(LogHerbalistWorld, Log, TEXT("Collected fragments (%d):"), CollectedFragmentIDs.Num());
    for (FName ID : CollectedFragmentIDs)
    {
        UE_LOG(LogHerbalistWorld, Log, TEXT("  - %s"), *ID.ToString());
    }
    UE_LOG(LogHerbalistWorld, Log, TEXT("Active fragment in world: %s"),
        ActiveFragment.IsValid() ? *ActiveFragment->GetDefinitionID().ToString() : TEXT("none"));
}
