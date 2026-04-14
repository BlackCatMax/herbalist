#include "HerbalistSimulation.h"
#include "ProjectHerbalist.h"
#include "Core/Pipeline/HerbalistPipeline.h"
#include "Math/UnrealMathUtility.h"

FRealState HerbalistSimulation::UpdateWorld(
    FWorldState& World,
    const TArray<FRealState>& Inputs,
    FRngState& Rng)
{
    FRealState NewState = HerbalistCore::Pipeline::ApplyMorok(
        Inputs,
        World.CurrentState,
        World.Env,
        World.Memory,
        World.Intent,
        Rng
    );

    World.CurrentState = NewState;

    // Обновление памяти
    const float AccumulationRate = 0.1f;
    float DistDelta = NewState.Meta.Distortion - World.Memory.AccumulatedDistortion;
    World.Memory.AccumulatedDistortion += DistDelta * AccumulationRate;
    World.Memory.AccumulatedDistortion = FMath::Clamp(World.Memory.AccumulatedDistortion, 0.0f, 1.0f);

    float StabDelta = NewState.Meta.Stability - World.Memory.StabilityMemory;
    World.Memory.StabilityMemory += StabDelta * AccumulationRate;
    World.Memory.StabilityMemory = FMath::Clamp(World.Memory.StabilityMemory, 0.0f, 1.0f);

    UE_LOG(LogHerbalist, Warning, TEXT("[WORLD] MemDist: %.3f | MemStab: %.3f | ResultDist: %.3f"),
        World.Memory.AccumulatedDistortion,
        World.Memory.StabilityMemory,
        NewState.Meta.Distortion);

    return NewState;
}