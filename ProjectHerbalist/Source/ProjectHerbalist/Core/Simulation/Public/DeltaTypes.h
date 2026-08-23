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

    // Только для крафта (Add): Coherence, который Pipeline уже посчитал
    // (ComputeIntentCoherence). Снято отсюда 2026-08-24 вместе с переносом
    // подношения капищу на Apply (варка сама по себе не должна влиять на
    // клетки) — и заведено заново для другого, законного потребителя:
    // триггера фрагмента памяти Заряны CoherentBrew (RunSimulationStep),
    // которому нужен именно момент варки, не применения. Не тот же случай:
    // фрагмент не трогает Cell.State, только спавнит презентационный актор.
    float Coherence = 0.0f;
};

/**
 * Главная структура дельты – результат обработки одного тика симуляции.
 */
// След алхимического воздействия в биом-графе (05_Systems.md "Biome Context
// Injection", 14_Biome_Graph.md "След игрока (Footprint)") — Pipeline лишь
// формирует эти данные, реальный вызов UBiomeGraphSubsystem::RecordFootprint()
// происходит вне Pipeline (там, где уже разрешено трогать UE-рантайм).
struct FBiomeFootprintEntry
{
    FName BiomeID;
    float MorokImpact = 0.f;
    float ZaryanaImpact = 0.f;
    FVector4 AxisDelta = FVector4(0.f, 0.f, 0.f, 0.f);
};

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

    TArray<FBiomeFootprintEntry> Footprints;
};