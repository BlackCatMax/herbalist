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

    // Мягкая правка TargetState клетки (BiomeGraph и подобные continuous-field
    // источники) — в отличие от WorldChanges, не трогает Cell.State, только
    // цель, к которой State плавно подтягивается в RegenerateCellParameters.
    // Оба поля идут через один ApplyStateDelta() — единственную точку записи в мир.
    TMap<FIntPoint, FRealState> TargetStateNudges;
};