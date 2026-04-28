#include "SnapshotService.h"

namespace Simulation
{
    FWorldSnapshot FSnapshotService::CaptureWorld()
    {
        return FWorldSnapshot{};
    }

    FInventorySnapshot FSnapshotService::CaptureInventory()
    {
        return FInventorySnapshot{};
    }

    FBiomeSnapshot FSnapshotService::CaptureBiomes()
    {
        return FBiomeSnapshot{};
    }

    void FSnapshotService::ApplyDeltaToWorld(const FStateDelta& Delta) {}
    void FSnapshotService::ApplyDeltaToInventory(const FStateDelta& Delta) {}
    void FSnapshotService::ApplyDeltaToBiomes(const FStateDelta& Delta) {}
	
	FCommandGraph FSnapshotService::BuildCommandGraph(const TArray<FCommandEntry>& RawCommands)
    {
        FCommandGraph Graph;
        // Просто копируем команды как есть (сортировка позже)
        Graph.Commands = RawCommands;
        // В реальной реализации здесь должна быть валидация, сортировка по ExecutionOrder и т.д.
        return Graph;
    }
}