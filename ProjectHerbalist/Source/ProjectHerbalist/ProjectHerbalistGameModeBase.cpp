#include "ProjectHerbalistGameModeBase.h"

#include "Core/World/HerbalistSimulation.h"
#include "Core/Pipeline/HerbalistPipeline.h" // LogHerbalist
#include "Core/Types/HerbalistCoreTypes.h"

void AProjectHerbalistGameModeBase::BeginPlay()
{
    Super::BeginPlay();

    // =========================
    // WORLD INIT
    // =========================
    FWorldState World;

    if (WorldConfig)
    {
        World.Env = WorldConfig->Environment;
        World.Memory = WorldConfig->InitialMemory;
        World.Intent = WorldConfig->Intent;

        UE_LOG(LogHerbalist, Warning, TEXT("WorldConfig loaded"));

        // INPUTS из DataAsset
        A = WorldConfig->InputA;
        B = WorldConfig->InputB;
    }
    else
    {
        UE_LOG(LogHerbalist, Warning, TEXT("WorldConfig is NULL - using defaults"));
    }

    // =========================
    // SIMULATION LOOP
    // =========================
    const int32 Steps = 10;

    for (int32 i = 0; i < Steps; i++)
    {
        FRealState Result =
            HerbalistSimulation::UpdateWorld(World, A, B, Rng);

        UE_LOG(LogHerbalist, Warning,
            TEXT("Step %d | ResultDist: %.3f | MemDist: %.3f"),
            i,
            Result.Meta.Distortion,
            World.Memory.AccumulatedDistortion
        );
    }
}