// Core/Simulation/Private/PipelineV2.h
#pragma once

#include "Core/Simulation/Public/SnapshotTypes.h"
#include "Core/Simulation/Public/DeltaTypes.h"
#include "Core/Simulation/Public/CommandTypes.h"
#include "Core/Types/HerbalistCoreTypes.h"
#include "Math/RandomStream.h"   // FRandomStream

namespace Simulation
{
    /**
     * Детерминированный пайплайн обработки команд.
     * @param WorldSnapshot     снимок мира (const, не меняется)
     * @param InventorySnapshot снимок инвентарей (const)
     * @param Commands          граф команд
     * @param Rng               генератор случайных чисел (меняет состояние)
     * @return FStateDelta      изменения для применения
     */
    FStateDelta ExecutePipeline(const FWorldSnapshot& WorldSnapshot,
                                const FInventorySnapshot& InventorySnapshot,
                                const FCommandGraph& Commands,
                                FRandomStream& Rng);
}