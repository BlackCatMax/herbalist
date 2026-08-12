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

        // Общая формула искажения: шум масштабируется собственным Distortion
        // объекта (S_Perceived = Distort(S_real, Morok) — при Distortion≈0 почти
        // не искажает, при высоком Distortion шум заметен), а не фиксированный
        // диапазон независимо от состояния.
        static FRealState PerceiveRealState(const FRealState& Real, FRandomStream& Rng);
    };
}