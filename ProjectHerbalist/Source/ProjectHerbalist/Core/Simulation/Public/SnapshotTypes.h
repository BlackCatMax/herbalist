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

    // Игровое время на момент снапшота (GetWorld()->GetTimeSeconds()) — нужно
    // Pipeline'у для FInventoryItem::CreationTime, не обращаясь к UWorld напрямую.
    float WorldTime = 0.f;
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
 * Контекст одного узла биом-графа на момент снапшота — то, что Pipeline
 * должен учитывать при Biome Context Injection (05_Systems.md, 14_Biome_Graph.md),
 * не обращаясь к UBiomeGraphSubsystem напрямую.
 */
struct FBiomeFieldContext
{
    float MorokField = 0.f;
    float ZaryanaField = 0.f;
    float MorokAffinity = 0.5f;
    float ZaryanaAffinity = 0.5f;
    FVector4 AxisDrift = FVector4(0.25f, 0.25f, 0.25f, 0.25f);
};

/**
 * Замороженное состояние биомного графа.
 */
struct FBiomeSnapshot
{
    // Список идентификаторов (FName) активных в данный момент биомов
    TArray<FName> ActiveBiomeIds;

    // Поля/аффинити/дрейф осей на биом — заполняется из UBiomeGraphSubsystem::CaptureState()
    TMap<FName, FBiomeFieldContext> Contexts;

    // Порог бифуркации (Collapse/Purification), общий на граф (пока не per-node)
    float CollapseThreshold = 0.85f;
};