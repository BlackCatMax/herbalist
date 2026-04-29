// Core/Simulation/Public/DeltaTypes.h
#pragma once

#include "CoreMinimal.h"
#include "SnapshotTypes.h"                      // FWorldSnapshot, FBiomeSnapshot, FInventorySnapshot
#include "Core/Types/HerbalistCoreTypes.h"      // FInventoryItem, FGridCell

enum class EInventoryOpType : uint8
{
    Add,
    Remove,
    Transfer
};

struct FInventoryOperation
{
    int32 ContainerID = 0;
    FInventoryItem Ingredient;
    EInventoryOpType OpType = EInventoryOpType::Add;
    int32 Amount = 1;
};

/**
 * Главная структура дельты – результат обработки одного тика симуляции.
 */
struct FStateDelta
{
    TMap<FIntPoint, FGridCell> WorldChanges;
    TArray<FInventoryOperation> InventoryOps;
    TArray<FName> BiomeActivations;   // теперь FName, как в реальной системе
    bool bIsPotionEffect = false;      // true, если дельта возникла из-за применения зелья на клетку
};