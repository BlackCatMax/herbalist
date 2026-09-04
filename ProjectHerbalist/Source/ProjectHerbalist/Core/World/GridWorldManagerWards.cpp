// Core/World/GridWorldManagerWards.cpp
//
// Обереги (кристаллы Пещеры, DESIGN_Community_And_Homestead.md §2.4,
// 2026-09-04) — "работа оберегов слабая, но постоянная, есть временные
// лимиты, не постоянное действие" (прямая формулировка запроса). Тот же
// GameClockSeconds-экспаери, что уже InvisibilityCap/YouthApple/Alkonost
// (GridWorldManagerArtifactEffects.cpp/GridWorldManagerProphetFeathers.cpp)
// — выбор таймера обоснован у EWardEffectType (HerbalistCoreTypes.h).
//
// Владение (есть ли нужный кристалл в инвентаре игрока) НЕ проверяется
// здесь — тот же принцип разделения обязанностей, что уже
// OfferToCommunity/RegisterGardenPlot: инвентарные поиски делает
// AHerbalistPlayerController::ActivateWard (резолвит FIngredientTableRow
// через IngredientRegistrySubsystem, дальше зовёт одну из функций ниже),
// GridWorldManager хранит только мировое состояние уже активированного
// эффекта. Это тот же класс границы, что уже отделяет GenerateHarvestResult
// от Landmark/Respect (ROADMAP.md) — и та же причина, по которой резолв
// через IngredientRegistrySubsystem не покрыт автотестом напрямую
// (GameInstanceSubsystem недоступен в Editor-мире автотестов, тот же
// пробел, что уже у TradeWithCommunity).

#include "Core/World/GridWorldManager.h"
#include "Core/Config/HerbalistSettings.h"
#include "ProjectHerbalist.h"
#include "HerbalistLogChannels.h"

bool AGridWorldManager::ActivateWardBrewBoost()
{
    const UHerbalistSettings* Settings = GetHerbalistSettings();
    const float DurationSeconds = Settings ? Settings->WardDurationSeconds : 600.0f;
    WardBrewBoostExpiryGameSeconds = GameClockSeconds + DurationSeconds;

    UE_LOG(LogHerbalistWorld, Log, TEXT("[Ward] BrewBoost active until %.1f"), WardBrewBoostExpiryGameSeconds);
    return true;
}

bool AGridWorldManager::IsWardBrewBoostActive() const
{
    return GameClockSeconds < WardBrewBoostExpiryGameSeconds;
}

bool AGridWorldManager::ActivateWardConcealment(const FIntPoint& Center)
{
    const UHerbalistSettings* Settings = GetHerbalistSettings();
    const float DurationSeconds = Settings ? Settings->WardDurationSeconds : 600.0f;
    WardConcealmentCenter = Center;
    WardConcealmentExpiryGameSeconds = GameClockSeconds + DurationSeconds;

    UE_LOG(LogHerbalistWorld, Log, TEXT("[Ward] Concealment active at (%d,%d) until %.1f"),
        Center.X, Center.Y, WardConcealmentExpiryGameSeconds);
    return true;
}

bool AGridWorldManager::IsWardConcealmentActive() const
{
    return GameClockSeconds < WardConcealmentExpiryGameSeconds;
}

bool AGridWorldManager::IsWardConcealmentActive(const FIntPoint& Cell) const
{
    if (!IsWardConcealmentActive()) return false;

    const UHerbalistSettings* Settings = GetHerbalistSettings();
    const int32 Radius = Settings ? Settings->WardConcealmentRadius : 1;
    const int32 Dist = FMath::Max(FMath::Abs(Cell.X - WardConcealmentCenter.X), FMath::Abs(Cell.Y - WardConcealmentCenter.Y));
    return Dist <= Radius;
}

// MorokReduction (Куриный бог, второй заход 2026-09-04) — тот же
// Center+Radius API, что и EntityConceal выше (активация фиксирует клетку
// игрока в момент вызова). Читается только из ComputePerceptionDistortion
// (GridWorldManagerEntities.cpp), которая сама решает применять ли эффект
// только ночью — здесь, на уровне таймера/геометрии, никакого дневного/
// ночного различия нет, ровно как ActivateWardConcealment не знает о
// сущностях, которые он в итоге гасит.
bool AGridWorldManager::ActivateWardMorokReduction(const FIntPoint& Center)
{
    const UHerbalistSettings* Settings = GetHerbalistSettings();
    const float DurationSeconds = Settings ? Settings->WardDurationSeconds : 600.0f;
    WardMorokReductionCenter = Center;
    WardMorokReductionExpiryGameSeconds = GameClockSeconds + DurationSeconds;

    UE_LOG(LogHerbalistWorld, Log, TEXT("[Ward] MorokReduction active at (%d,%d) until %.1f"),
        Center.X, Center.Y, WardMorokReductionExpiryGameSeconds);
    return true;
}

bool AGridWorldManager::IsWardMorokReductionActive() const
{
    return GameClockSeconds < WardMorokReductionExpiryGameSeconds;
}

bool AGridWorldManager::IsWardMorokReductionActive(const FIntPoint& Cell) const
{
    if (!IsWardMorokReductionActive()) return false;

    const UHerbalistSettings* Settings = GetHerbalistSettings();
    const int32 Radius = Settings ? Settings->WardMorokReductionRadius : 1;
    const int32 Dist = FMath::Max(FMath::Abs(Cell.X - WardMorokReductionCenter.X), FMath::Abs(Cell.Y - WardMorokReductionCenter.Y));
    return Dist <= Radius;
}
