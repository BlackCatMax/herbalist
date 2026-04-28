// Core/World/GridWorldManagerHarvest.cpp
#include "Core/World/GridWorldManager.h"
#include "Core/Harvest/HarvestService.h"
#include "Core/Inventory/HerbalistInventoryComponent.h"
#include "Player/HerbalistPlayerController.h"
#include "ProjectHerbalist.h"

// ============================================================================
// СБОР ЧЕРЕЗ СТАРЫЙ КОД (устарело, оставлено для обратной совместимости)
// ============================================================================

FRealState AGridWorldManager::HarvestFromCell(int32 X, int32 Y, const FConditionModifier& Conditions)
{
    UE_LOG(LogHerbalist, Warning, TEXT("HarvestFromCell: deprecated, use command-based harvest."));
    return FRealState();
}

FRealState AGridWorldManager::HarvestFromCellSimple(int32 X, int32 Y)
{
    return HarvestFromCell(X, Y, FConditionModifier());
}

// ============================================================================
// ТЕСТОВЫЕ КОМАНДЫ
// ============================================================================

void AGridWorldManager::HarvestTest(int32 X, int32 Y)
{
    UE_LOG(LogHerbalist, Warning, TEXT("HarvestTest: deprecated."));
}

void AGridWorldManager::MassHarvestTest(int32 X, int32 Y, int32 Count)
{
    for (int32 i = 0; i < Count; ++i)
    {
        HarvestTest(X, Y);
    }
    UE_LOG(LogHerbalist, Log, TEXT("Mass harvest %d times at (%d,%d)"), Count, X, Y);
}