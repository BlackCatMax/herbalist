// PerceptionService.cpp
#include "PerceptionService.h"

namespace Simulation
{
    FPerceivedWorld FPerceptionService::ComputePerceivedWorld(const FWorldSnapshot& RealWorld, FRandomStream& Rng)
    {
        FPerceivedWorld Result;
        Result.WorldSeed = RealWorld.WorldSeed;
        for (const auto& Pair : RealWorld.GridState)
        {
            FPerceivedCell P;
            P.Coord = Pair.Key;
            P.bIsVisible = true;
            // Добавляем небольшой шум к магнитуде и искажению
            P.PerceivedState = Pair.Value.State;
            P.PerceivedState.Magnitude = FMath::Clamp(P.PerceivedState.Magnitude + Rng.FRandRange(-0.05f, 0.05f), 0.f, 1.f);
            P.PerceivedState.Meta.Distortion = FMath::Clamp(P.PerceivedState.Meta.Distortion + Rng.FRandRange(-0.03f, 0.03f), 0.f, 1.f);
            Result.Cells.Add(Pair.Key, P);
        }
        return Result;
    }

    FPerceivedInventory FPerceptionService::ComputePerceivedInventory(const FInventorySnapshot& RealInventory, FRandomStream& Rng)
    {
        // Пока без искажений, просто копируем
        FPerceivedInventory Result;
        Result.ContainerContents = RealInventory.ContainerContents;
        return Result;
    }
}