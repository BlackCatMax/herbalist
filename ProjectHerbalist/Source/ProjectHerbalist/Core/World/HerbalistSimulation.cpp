// HerbalistSimulation.cpp
#include "HerbalistSimulation.h"
#include "Core/Pipeline/HerbalistPipeline.h"
#include "Math/UnrealMathUtility.h"

FRealState HerbalistSimulation::UpdateWorld(
    FWorldState& World,
    const FRealState& A,
    const FRealState& B,
    FRngState& Rng)
{
    // =========================
    // CORE TRANSFORMATION (НЕ ТРОГАЕМ)
    // =========================
    FRealState Result = HerbalistCore::Pipeline::ApplyMorok(
        A,
        B,
        World.Env,
        World.Memory,
        World.Intent,
        Rng
    );

    // =========================
    // APPLY RESULT TO WORLD
    // =========================
    World.CurrentState = Result;

    // =========================
    // MEMORY LOOP (УЛУЧШЕННЫЙ БАЗОВЫЙ)
    // =========================
    const float AccumulationRate = 0.1f;

    float PrevDist = World.Memory.AccumulatedDistortion;
    float PrevStab = World.Memory.StabilityMemory;

    float DistDelta = (Result.Meta.Distortion - World.Memory.AccumulatedDistortion);
    World.Memory.AccumulatedDistortion += DistDelta * AccumulationRate;

    float StabDelta = (Result.Meta.Stability - World.Memory.StabilityMemory);
    World.Memory.StabilityMemory += StabDelta * AccumulationRate;

    UE_LOG(LogHerbalist, Warning,
        TEXT("[WORLD] MemDist: %.3f -> %.3f | ResultDist: %.3f"),
        PrevDist,
        World.Memory.AccumulatedDistortion,
        Result.Meta.Distortion
    );

    UE_LOG(LogHerbalist, Warning,
        TEXT("[WORLD] MemStab: %.3f -> %.3f | ResultStab: %.3f"),
        PrevStab,
        World.Memory.StabilityMemory,
        Result.Meta.Stability
    );

    return Result;
}