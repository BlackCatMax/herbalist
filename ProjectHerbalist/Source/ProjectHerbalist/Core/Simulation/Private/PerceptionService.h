// PerceptionService.h
#pragma once
#include "PerceivedTypes.h"
#include "SnapshotTypes.h"
#include "Math/RandomStream.h"

namespace Simulation
{
    class PROJECTHERBALIST_API FPerceptionService
    {
    public:
        static FPerceivedWorld ComputePerceivedWorld(const FWorldSnapshot& RealWorld, FRandomStream& Rng);
        static FPerceivedInventory ComputePerceivedInventory(const FInventorySnapshot& RealInventory, FRandomStream& Rng);
    };
}