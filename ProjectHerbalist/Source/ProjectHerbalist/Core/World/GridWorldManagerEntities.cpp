// Core/World/GridWorldManagerEntities.cpp
//
// Вертикальный срез "Проявление сущностей" (02_GDD/16_Entity_Manifestation.md).
// Реализует ровно 4 карточки бестиария, по одной на механизм:
//   - Гнильники   (Низший,     Болото)        -> амбиентная зона (§16.2)
//   - Полевик     (Основной,   Лесостепь)      -> "хозяин" с Respect (§16.3)
//   - Берегиня    (Легендарный, Речная пойма)  -> порог мирового состояния (§16.4)
//   - Морочники   (Опасная нечисть, повсеместно) -> искажение восприятия (§16.5)
//
// Всё, что мутирует State/TargetState, идёт через Delta.TargetStateNudges ->
// ApplyStateDelta — тот же внепайплайновый, но Single-Writer-совместимый канал,
// что уже использует ApplyBiomeInfluences (GridWorldManagerCore.cpp). Ничего
// здесь не проходит через Command/Delta детерминированный пайплайн (PipelineV2) —
// это осознанно, по той же логике, что и у RegenerateCellParameters: это
// презентационно-атмосферный слой, а не игровая причинность.

#include "Core/World/GridWorldManager.h"
#include "Core/Config/HerbalistSettings.h"
#include "Core/Types/BiomeTypes.h"
#include "Core/Simulation/Public/DeltaTypes.h"
#include "ProjectHerbalist.h"
#include "HerbalistLogChannels.h"

namespace
{
    const FName EntityID_Gnilniki(TEXT("Гнильники"));
    const FName EntityID_Polevik(TEXT("Полевик"));
    const FName EntityID_Bereginya(TEXT("Берегиня"));
}

// ============================================================================
// СУТОЧНЫЙ ЦИКЛ (минимальная версия для среза — только фаза, без луны/сезона)
// ============================================================================

float AGridWorldManager::GetTimeOfDay01() const
{
    const UHerbalistSettings* Settings = GetHerbalistSettings();
    const float DayLengthSeconds = FMath::Max(1.0f, (Settings ? Settings->GameDayMinutes : 32.0f) * 60.0f);
    return FMath::Fmod(WorldTimeSeconds, DayLengthSeconds) / DayLengthSeconds;
}

bool AGridWorldManager::IsNight() const
{
    const UHerbalistSettings* Settings = GetHerbalistSettings();
    const float DayMinutes = FMath::Max(1.0f, Settings ? Settings->GameDayMinutes : 32.0f);
    // Ночь — последние 6 игровых минут суток (15_Cycles_And_Shrines §15.2:
    // Рассвет 6 / День 14 / Закат 6 / Ночь 6, итого 32).
    const float NightDurationMinutes = 6.0f;
    const float NightStartFraction = 1.0f - FMath::Clamp(NightDurationMinutes / DayMinutes, 0.0f, 1.0f);
    return GetTimeOfDay01() >= NightStartFraction;
}

float AGridWorldManager::ComputePerceptionDistortion(int32 X, int32 Y) const
{
    const FGridCell* Cell = GetCellConst(X, Y);
    if (!Cell) return 0.0f;

    const UHerbalistSettings* Settings = GetHerbalistSettings();
    float Perceived = Cell->Memory.AccumulatedDistortion;

    // Морочники: ночью восприятие лжёт сильнее, сам мир (S_real) не меняется —
    // это надбавка только для UI (тултипы через PerceiveValue), не для Cell.State.
    if (IsNight())
    {
        Perceived += Settings ? Settings->NightPerceptionDistortionBonus : 0.25f;
    }

    // Любая проявленная сущность в клетке — сигнал "здесь не всё как кажется",
    // независимо от того, какая именно (для среза это в первую очередь Гнильники).
    if (!Cell->ManifestedEntityID.IsNone())
    {
        Perceived += 0.1f;
    }

    return FMath::Clamp(Perceived, 0.0f, 1.0f);
}

// ============================================================================
// РАССТАНОВКА ТЕСТОВЫХ "ОБИТАЛИЩ" (только для среза — в продакшене вручную)
// ============================================================================

void AGridWorldManager::SeedTestLandmarks()
{
    EntityLandmarks.Empty();

    // Полевик: первая найденная клетка Лесостепи, не водная.
    for (const FGridCell& Cell : Cells)
    {
        if (Cell.Biome == EBiomeType::ForestSteppe && !Cell.bIsWater)
        {
            FEntityLandmark Landmark;
            Landmark.EntityID = EntityID_Polevik;
            Landmark.Cell = FIntPoint(Cell.X, Cell.Y);
            Landmark.Respect = 0.0f;
            EntityLandmarks.Add(Landmark);
            UE_LOG(LogHerbalistWorld, Log, TEXT("[Entities] Seeded landmark %s at (%d,%d)"),
                *EntityID_Polevik.ToString(), Cell.X, Cell.Y);
            break;
        }
    }
}

// ============================================================================
// ГЛАВНЫЙ ТИК ПРОЯВЛЕНИЙ
// ============================================================================

void AGridWorldManager::UpdateEntityManifestations(float DeltaTime)
{
    WorldTimeSeconds += DeltaTime;

    const UHerbalistSettings* Settings = GetHerbalistSettings();
    const float GnilnikiThreshold   = Settings ? Settings->GnilnikiCorruptionThreshold      : 0.6f;
    const float GnilnikiNudgeRate   = Settings ? Settings->GnilnikiNudgeRate                : 0.01f;
    const float HistoryPurityRate   = Settings ? Settings->HistoryPurityLerpRate            : 0.02f;
    const float BereginyaThreshold  = Settings ? Settings->BereginyaHistoryPurityThreshold  : 0.75f;
    const float RespectGainRate     = Settings ? Settings->LandmarkRespectGainRate          : 0.01f;
    const float RespectDecayRate    = Settings ? Settings->LandmarkRespectDecayRate         : 0.02f;
    const float StressAngerThreshold= Settings ? Settings->LandmarkStressAngerThreshold     : 0.6f;

    FStateDelta Delta;

    // ---- Низший (Гнильники) и Легендарный (Берегиня) — проход по всем клеткам ----
    for (FGridCell& Cell : Cells)
    {
        // Memory.HistoryPurity — медленная скользящая средняя от текущей Purity.
        // Поле уже существовало в FMemoryState, но нигде не обновлялось (см.
        // 16_Entity_Manifestation §16.4, упрощённый аналог Restoration капищ).
        Cell.Memory.HistoryPurity = FMath::Lerp(Cell.Memory.HistoryPurity, Cell.State.Meta.Purity, HistoryPurityRate);

        bool bChanged = false;
        FRealState NewTarget = Cell.TargetState;

        // --- Гнильники: Низший, амбиентная зона, Болото ---
        if (Cell.Biome == EBiomeType::Bog && !Cell.bIsWater)
        {
            if (Cell.State.Meta.Corruption > GnilnikiThreshold)
            {
                Cell.ManifestedEntityID = EntityID_Gnilniki;
                // Самоусиливающаяся порча места — тянем TargetState дальше, чем
                // клетка уже есть, а не жёстко фиксируем; ApplyStateDelta/
                // RegenerateCellParameters сами доведут State до цели.
                NewTarget.Meta.Corruption = FMath::Clamp(NewTarget.Meta.Corruption + GnilnikiNudgeRate, 0.0f, 1.0f);
                NewTarget.Meta.Purity     = FMath::Clamp(NewTarget.Meta.Purity - GnilnikiNudgeRate * 0.5f, 0.0f, 1.0f);
                bChanged = true;
            }
            else if (Cell.ManifestedEntityID == EntityID_Gnilniki)
            {
                // Порог больше не пройден — зона рассеивается.
                Cell.ManifestedEntityID = NAME_None;
            }
        }

        // --- Берегиня: Легендарный, порог мирового состояния, Речная пойма ---
        if (Cell.Biome == EBiomeType::Floodplain && Cell.bIsWater)
        {
            if (Cell.Memory.HistoryPurity > BereginyaThreshold)
            {
                Cell.ManifestedEntityID = EntityID_Bereginya;
                // Благословлённая вода — Purity подтягивается к максимуму, но не
                // мгновенно и с потолком (не 1.0 — оставляет пространство Zaryana
                // в самой варке, а не обнуляет её роль).
                NewTarget.Meta.Purity = FMath::Clamp(FMath::Max(NewTarget.Meta.Purity, 0.9f), 0.0f, 0.97f);
                bChanged = true;
            }
            else if (Cell.ManifestedEntityID == EntityID_Bereginya)
            {
                Cell.ManifestedEntityID = NAME_None;
            }
        }

        if (bChanged)
        {
            Delta.TargetStateNudges.Add(FIntPoint(Cell.X, Cell.Y), NewTarget);
        }
    }

    // ---- Основной (Полевик) — проход по клеткам-обиталищам ----
    for (FEntityLandmark& Landmark : EntityLandmarks)
    {
        FGridCell* Cell = GetCell(Landmark.Cell.X, Landmark.Cell.Y);
        if (!Cell) continue;

        // Бережное обращение (низкий стресс, высокая чистота места) поднимает
        // Respect; истощение клетки — опускает. Та же формула-дух, что и у
        // Restoration капищ (15_Cycles_And_Shrines §15.5), но без завязки на
        // конкретную варку — Полевик реагирует на состояние поля в целом.
        if (Cell->HarvestStress > StressAngerThreshold)
        {
            Landmark.Respect = FMath::Clamp(Landmark.Respect - RespectDecayRate * DeltaTime, -1.0f, 1.0f);
        }
        else if (Cell->State.Meta.Purity > 0.6f)
        {
            Landmark.Respect = FMath::Clamp(Landmark.Respect + RespectGainRate * DeltaTime, -1.0f, 1.0f);
        }

        FRealState NewTarget = Cell->TargetState;
        bool bChanged = false;

        if (Landmark.Respect > 0.5f)
        {
            // Благословлённое зерно (08_Content/бестиарий: "повышенная Potency и Purity").
            NewTarget.Meta.Potency = FMath::Clamp(NewTarget.Meta.Potency + RespectGainRate * DeltaTime, 0.0f, 1.0f);
            NewTarget.Meta.Purity  = FMath::Clamp(NewTarget.Meta.Purity + RespectGainRate * DeltaTime * 0.5f, 0.0f, 1.0f);
            Cell->ManifestedEntityID = Landmark.EntityID;
            bChanged = true;
        }
        else if (Landmark.Respect < -0.3f)
        {
            // "Порча посевов, наведение усталости" — переведено в Stability, а не
            // в новую придуманную ось "усталости".
            NewTarget.Meta.Stability = FMath::Clamp(NewTarget.Meta.Stability - RespectDecayRate * DeltaTime, 0.0f, 1.0f);
            Cell->ManifestedEntityID = Landmark.EntityID;
            bChanged = true;
        }
        else if (Cell->ManifestedEntityID == Landmark.EntityID)
        {
            Cell->ManifestedEntityID = NAME_None;
        }

        if (bChanged)
        {
            Delta.TargetStateNudges.Add(Landmark.Cell, NewTarget);
        }
    }

    if (Delta.TargetStateNudges.Num() > 0)
    {
        ApplyStateDelta(Delta);
    }
}
