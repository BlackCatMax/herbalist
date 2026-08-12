// Core/Simulation/Public/SnapshotTypes.h
#pragma once

#include "CoreMinimal.h"
#include "Core/Types/HerbalistCoreTypes.h"    // FInventoryItem, FGridCell (должны быть здесь)

/**
 * Замороженное состояние игрового мира.
 * Простая C++ структура – не требует UPROPERTY, не сериализуется.
 */
struct FWorldSnapshot
{
    TMap<FIntPoint, FGridCell> GridState;
    int32 WorldSeed = 0;
    int32 TickIndex = 0;
};

/**
 * Замороженное состояние всех инвентарей (игрок, контейнеры).
 */
struct FInventorySnapshot
{
    // Ключ – ID контейнера (например, GetUniqueID компонента)
    TMap<int32, TArray<FInventoryItem>> ContainerContents;
};

/**
 * Замороженное состояние биомного графа.
 */
struct FBiomeSnapshot
{
    // Список идентификаторов (FName) активных в данный момент биомов
    TArray<FName> ActiveBiomeIds;
};