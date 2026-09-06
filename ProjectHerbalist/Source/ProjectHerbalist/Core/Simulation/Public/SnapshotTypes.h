// Core/Simulation/Public/SnapshotTypes.h
#pragma once

#include "CoreMinimal.h"
#include "Core/Types/HerbalistCoreTypes.h"    // FInventoryItem, FGridCell
#include "Core/Shrine/ShrineTypes.h"          // FShrine (должны быть здесь)

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

    // Капища (02_GDD/15_Cycles_And_Shrines.md §15.5, эффект 2) — тот же принцип,
    // что уже есть у Biome Context Injection (FBiomeSnapshot ниже): Pipeline
    // остаётся чистой функцией, читает контекст из снапшота, а не лезет в
    // AGridWorldManager напрямую.
    TArray<FShrine> Shrines;
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
    // ВНИМАНИЕ: MorokField/ZaryanaField с 2026-09-07 -- ЗНАКОВЫЕ ОТКЛОНЕНИЯ
    // от природных значений биома, а не абсолютные уровни (см. довод у
    // GetBiomeSamples, GridWorldManagerCore.cpp). Ноль означает "биом в своей
    // природе". Для сравнения с авторскими АБСОЛЮТНЫМИ порогами и для
    // "насколько это место испорчено" бери AmbientMorok ниже.
    float MorokField = 0.f;
    float ZaryanaField = 0.f;

    // Абсолютный уровень Морока места = дефолт биома + отклонение, [0,1].
    // Заполняется UBiomeGraphSubsystem::CaptureState.
    float AmbientMorok = 0.f;
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