// Core/Simulation/Private/PipelineV2.h
#pragma once

#include "Core/Simulation/Public/SnapshotTypes.h"
#include "Core/Simulation/Public/DeltaTypes.h"
#include "Core/Simulation/Public/CommandTypes.h"
#include "Core/Types/HerbalistCoreTypes.h"
#include "Math/RandomStream.h"

namespace Simulation
{
    PROJECTHERBALIST_API FStateDelta ExecutePipeline(
        const FWorldSnapshot& WorldSnapshot,
        const FInventorySnapshot& InventorySnapshot,
        const FCommandGraph& Commands,
        FRandomStream& Rng);
}