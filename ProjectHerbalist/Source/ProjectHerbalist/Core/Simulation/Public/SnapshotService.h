#pragma once

#include "SnapshotTypes.h"
#include "DeltaTypes.h"
#include "CommandTypes.h"          // теперь тоже доступен

namespace Simulation
{
    class FSnapshotService
    {
    public:
        static FWorldSnapshot CaptureWorld();
        static FInventorySnapshot CaptureInventory();
        static FBiomeSnapshot CaptureBiomes();

        static void ApplyDeltaToWorld(const FStateDelta& Delta);
        static void ApplyDeltaToInventory(const FStateDelta& Delta);
        static void ApplyDeltaToBiomes(const FStateDelta& Delta);

        static FCommandBatch BuildCommandBatch(const TArray<FCommandEntry>& RawCommands);
		static FStateDelta ExecuteTick(const FCommandBatch& Commands);
    };
}