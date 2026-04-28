// Core/Simulation/Private/TraceReplay.cpp
#include "TraceReplay.h"
#include "PipelineV2.h"
#include "ProjectHerbalist.h"

namespace Simulation
{
    bool ReplayAndCompare(const FTraceFrame& Frame, FRandomStream& Rng)
    {
        FInventorySnapshot InvSnap;   // инвентарь не трассируем, используем пустой

        FCommandGraph Graph;
        Graph.Commands = Frame.Commands;

        FStateDelta ReplayedDelta = ExecutePipeline(Frame.WorldSnapshot, InvSnap, Graph, Rng);

        // Сравниваем изменения мира
        if (ReplayedDelta.WorldChanges.Num() != Frame.GeneratedDelta.WorldChanges.Num())
        {
            UE_LOG(LogHerbalist, Warning, TEXT("ReplayAndCompare: WorldChanges count mismatch. Original: %d, Replay: %d"),
                Frame.GeneratedDelta.WorldChanges.Num(), ReplayedDelta.WorldChanges.Num());
            return false;
        }

        for (const auto& Pair : Frame.GeneratedDelta.WorldChanges)
        {
            const FIntPoint& Coord = Pair.Key;
            const FGridCell* ReplayedCell = ReplayedDelta.WorldChanges.Find(Coord);
            if (!ReplayedCell)
            {
                UE_LOG(LogHerbalist, Warning, TEXT("ReplayAndCompare: Missing cell (%d,%d) in replayed delta"), Coord.X, Coord.Y);
                return false;
            }

            if (ReplayedCell->State.Magnitude != Pair.Value.State.Magnitude ||
                ReplayedCell->State.Meta.Distortion != Pair.Value.State.Meta.Distortion)
            {
                UE_LOG(LogHerbalist, Warning, TEXT("ReplayAndCompare: Cell (%d,%d) state mismatch"), Coord.X, Coord.Y);
                return false;
            }
        }

        // Сравниваем операции инвентаря
        if (ReplayedDelta.InventoryOps.Num() != Frame.GeneratedDelta.InventoryOps.Num())
        {
            UE_LOG(LogHerbalist, Warning, TEXT("ReplayAndCompare: InventoryOps count mismatch"));
            return false;
        }

        UE_LOG(LogHerbalist, Log, TEXT("ReplayAndCompare: SUCCESS — delta is deterministic"));
        return true;
    }
}