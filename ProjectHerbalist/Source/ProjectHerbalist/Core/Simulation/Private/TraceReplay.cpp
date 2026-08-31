// Core/Simulation/Private/TraceReplay.cpp
#include "TraceReplay.h"
#include "PipelineV2.h"
#include "ProjectHerbalist.h"
#include "HerbalistLogChannels.h"

namespace Simulation
{
    bool ReplayAndCompare(const FTraceFrame& Frame, FRandomStream& Rng)
    {
        FInventorySnapshot InvSnap;   // инвентарь не трассируем, используем пустой

        FCommandBatch Batch;
        Batch.Commands = Frame.Commands;

        FStateDelta ReplayedDelta = ExecutePipeline(Frame.WorldSnapshot, InvSnap, Frame.BiomeSnapshot, Batch, Rng);

        // Сравниваем изменения мира
        if (ReplayedDelta.WorldChanges.Num() != Frame.GeneratedDelta.WorldChanges.Num())
        {
            UE_LOG(LogHerbalistSimulation, Warning, TEXT("ReplayAndCompare: WorldChanges count mismatch. Original: %d, Replay: %d"),
                Frame.GeneratedDelta.WorldChanges.Num(), ReplayedDelta.WorldChanges.Num());
            return false;
        }

        for (const auto& Pair : Frame.GeneratedDelta.WorldChanges)
        {
            const FIntPoint& Coord = Pair.Key;
            const FGridCell* ReplayedCell = ReplayedDelta.WorldChanges.Find(Coord);
            if (!ReplayedCell)
            {
                UE_LOG(LogHerbalistSimulation, Warning, TEXT("ReplayAndCompare: Missing cell (%d,%d) in replayed delta"), Coord.X, Coord.Y);
                return false;
            }

            if (ReplayedCell->State.Magnitude != Pair.Value.State.Magnitude ||
                ReplayedCell->State.Meta.Distortion != Pair.Value.State.Meta.Distortion)
            {
                UE_LOG(LogHerbalistSimulation, Warning, TEXT("ReplayAndCompare: Cell (%d,%d) state mismatch"), Coord.X, Coord.Y);
                return false;
            }
        }

        // Сравниваем операции инвентаря — количество И значения. Аудит
        // 2026-08-31 нашёл реальный пробел: до этой правки сравнивалось
        // только Num(), не содержимое — Harvest-команда (единственный
        // тип, чей результат живёт в InventoryOps, не в WorldChanges) могла
        // разойтись по значению (например, другой Magnitude от джиттера
        // сбора при другом сиде) и ReplayAndCompare молча сказал бы
        // "детерминировано", хотя это не так. Та же глубина сравнения, что
        // уже у WorldChanges выше (Magnitude/Distortion, не каждое поле).
        if (ReplayedDelta.InventoryOps.Num() != Frame.GeneratedDelta.InventoryOps.Num())
        {
            UE_LOG(LogHerbalistSimulation, Warning, TEXT("ReplayAndCompare: InventoryOps count mismatch"));
            return false;
        }

        for (int32 i = 0; i < Frame.GeneratedDelta.InventoryOps.Num(); ++i)
        {
            const FInventoryOperation& Original = Frame.GeneratedDelta.InventoryOps[i];
            const FInventoryOperation& Replayed = ReplayedDelta.InventoryOps[i];

            if (Original.ContainerID != Replayed.ContainerID ||
                Original.OpType != Replayed.OpType ||
                Original.Amount != Replayed.Amount ||
                Original.Ingredient.IngredientID != Replayed.Ingredient.IngredientID ||
                Original.Ingredient.State.Magnitude != Replayed.Ingredient.State.Magnitude ||
                Original.Ingredient.State.Meta.Distortion != Replayed.Ingredient.State.Meta.Distortion)
            {
                UE_LOG(LogHerbalistSimulation, Warning, TEXT("ReplayAndCompare: InventoryOp[%d] mismatch (%s)"),
                    i, *Original.Ingredient.IngredientID.ToString());
                return false;
            }
        }

        UE_LOG(LogHerbalistSimulation, Log, TEXT("ReplayAndCompare: SUCCESS — delta is deterministic"));
        return true;
    }
}