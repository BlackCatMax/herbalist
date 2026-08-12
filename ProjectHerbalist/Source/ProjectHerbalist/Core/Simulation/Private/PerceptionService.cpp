// PerceptionService.cpp
#include "PerceptionService.h"

namespace Simulation
{
    // При Distortion=0 объект виден почти точно; чем выше Distortion, тем шире
    // диапазон возможного шума. MaxNoise* — амплитуда шума при Distortion=1.
    FRealState FPerceptionService::PerceiveRealState(const FRealState& Real, FRandomStream& Rng)
    {
        const float NoiseScale = FMath::Clamp(Real.Meta.Distortion, 0.f, 1.f);
        const float MaxNoiseMain = 0.15f;   // Magnitude/Distortion/Purity/Stability
        const float MaxNoiseAxis = 0.08f;   // Direction

        FRealState Perceived = Real;
        Perceived.Magnitude = FMath::Clamp(Real.Magnitude + Rng.FRandRange(-MaxNoiseMain, MaxNoiseMain) * NoiseScale, 0.f, 1.f);
        Perceived.Meta.Distortion = FMath::Clamp(Real.Meta.Distortion + Rng.FRandRange(-MaxNoiseMain, MaxNoiseMain) * NoiseScale, 0.f, 1.f);
        Perceived.Meta.Purity = FMath::Clamp(Real.Meta.Purity + Rng.FRandRange(-MaxNoiseMain, MaxNoiseMain) * NoiseScale, 0.f, 1.f);
        Perceived.Meta.Stability = FMath::Clamp(Real.Meta.Stability + Rng.FRandRange(-MaxNoiseMain, MaxNoiseMain) * NoiseScale, 0.f, 1.f);

        Perceived.Direction.Body = FMath::Max(0.f, Real.Direction.Body + Rng.FRandRange(-MaxNoiseAxis, MaxNoiseAxis) * NoiseScale);
        Perceived.Direction.Mind = FMath::Max(0.f, Real.Direction.Mind + Rng.FRandRange(-MaxNoiseAxis, MaxNoiseAxis) * NoiseScale);
        Perceived.Direction.Spirit = FMath::Max(0.f, Real.Direction.Spirit + Rng.FRandRange(-MaxNoiseAxis, MaxNoiseAxis) * NoiseScale);
        Perceived.Direction.Nature = FMath::Max(0.f, Real.Direction.Nature + Rng.FRandRange(-MaxNoiseAxis, MaxNoiseAxis) * NoiseScale);
        Perceived.Direction.NormalizeSum();

        return Perceived;
    }

    FPerceivedWorld FPerceptionService::ComputePerceivedWorld(const FWorldSnapshot& RealWorld, FRandomStream& Rng)
    {
        FPerceivedWorld Result;
        Result.WorldSeed = RealWorld.WorldSeed;
        for (const auto& Pair : RealWorld.GridState)
        {
            FPerceivedCell P;
            P.Coord = Pair.Key;
            P.bIsVisible = true;
            P.PerceivedState = PerceiveRealState(Pair.Value.State, Rng);
            Result.Cells.Add(Pair.Key, P);
        }
        return Result;
    }

    FPerceivedInventory FPerceptionService::ComputePerceivedInventory(const FInventorySnapshot& RealInventory, FRandomStream& Rng)
    {
        FPerceivedInventory Result;
        for (const auto& Pair : RealInventory.ContainerContents)
        {
            TArray<FInventoryItem> PerceivedItems;
            PerceivedItems.Reserve(Pair.Value.Num());
            for (const FInventoryItem& Item : Pair.Value)
            {
                FInventoryItem Perceived = Item;
                Perceived.State = PerceiveRealState(Item.State, Rng);
                PerceivedItems.Add(MoveTemp(Perceived));
            }
            Result.ContainerContents.Add(Pair.Key, MoveTemp(PerceivedItems));
        }
        return Result;
    }
}