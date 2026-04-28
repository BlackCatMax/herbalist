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

        // Новый метод для PR-4 (заглушка)
        static FCommandGraph BuildCommandGraph(const TArray<FCommandEntry>& RawCommands);
    };
}