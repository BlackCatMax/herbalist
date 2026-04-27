// GridWorldManagerHarvest.cpp
#include "Core/World/GridWorldManager.h"
#include "Core/Harvest/HarvestService.h"
#include "Core/Inventory/HerbalistInventoryComponent.h"
#include "Player/HerbalistPlayerController.h"
#include "ProjectHerbalist.h"

FRealState AGridWorldManager::HarvestFromCell(int32 X, int32 Y, const FConditionModifier& Conditions)
{
    // Эта функция больше не используется для игрового сбора, оставлена для тестов.
    // Возвращаем пустое состояние.
    UE_LOG(LogHerbalist, Warning, TEXT("HarvestFromCell called but should not be used with multiple resources system."));
    return FRealState();
}

FRealState AGridWorldManager::HarvestFromCellSimple(int32 X, int32 Y)
{
    return HarvestFromCell(X, Y, FConditionModifier());
}

void AGridWorldManager::HarvestTest(int32 X, int32 Y)
{
    // Тестовый сбор через консоль – не рекомендуется, но оставим заглушку.
    UE_LOG(LogHerbalist, Warning, TEXT("HarvestTest called but not implemented in multiple resources system."));
}

void AGridWorldManager::MassHarvestTest(int32 X, int32 Y, int32 Count)
{
    for (int32 i = 0; i < Count; ++i) HarvestTest(X, Y);
    UE_LOG(LogHerbalist, Log, TEXT("Mass harvest %d times at (%d,%d)"), Count, X, Y);
}