// ProjectHerbalistGameModeBase.cpp
#include "ProjectHerbalistGameModeBase.h"
#include "ProjectHerbalist.h"
#include "Core/World/HerbalistSimulation.h"
#include "Core/Pipeline/HerbalistPipeline.h"
#include "Core/Types/HerbalistCoreTypes.h"
#include "Core/Harvest/HerbalistHarvest.h"
#include "Math/UnrealMathUtility.h"

void AProjectHerbalistGameModeBase::BeginPlay()
{
    Super::BeginPlay();

    FWorldState World;
    if (WorldConfig)
    {
        World.Env = WorldConfig->Environment;
        World.Memory = WorldConfig->InitialMemory;
        World.Intent = WorldConfig->Intent;
        World.CurrentState = WorldConfig->InitialBiomeState;

        const int32 Steps = 10;

        for (int32 i = 0; i < Steps; i++)
        {
            // Генерация ресурсов через Harvest на основе текущего состояния биома
            FConditionModifier NeutralCond; // пока без погоды/времени суток
            FRealState Resource1 = FHerbalistHarvest::Harvest(EResourceType::Nettle, World.CurrentState, NeutralCond);
            FRealState Resource2 = FHerbalistHarvest::Harvest(EResourceType::Fern, World.CurrentState, NeutralCond);

            // Опциональная мутация ресурсов (для тестовой динамики)
            if (bEnableRandomResourceMutation)
            {
                FRandomStream Stream(Rng.Seed + i);

                auto MutateResource = [&Stream](FRealState& Res)
                    {
                        Res.Magnitude += Stream.FRandRange(-0.05f, 0.05f);
                        Res.Magnitude = FMath::Clamp(Res.Magnitude, 0.0f, 1.0f);
                        Res.Direction.Body += Stream.FRandRange(-0.05f, 0.05f);
                        Res.Direction.Mind += Stream.FRandRange(-0.05f, 0.05f);
                        Res.Direction.Spirit += Stream.FRandRange(-0.05f, 0.05f);
                        Res.Direction.Nature += Stream.FRandRange(-0.05f, 0.05f);
                        float Len = FMath::Sqrt(Res.Direction.Body * Res.Direction.Body + Res.Direction.Mind * Res.Direction.Mind + Res.Direction.Spirit * Res.Direction.Spirit + Res.Direction.Nature * Res.Direction.Nature);
                        if (Len > KINDA_SMALL_NUMBER) { Res.Direction.Body /= Len; Res.Direction.Mind /= Len; Res.Direction.Spirit /= Len; Res.Direction.Nature /= Len; }
                        Res.Meta.Distortion = FMath::Clamp(Res.Meta.Distortion + Stream.FRandRange(-0.02f, 0.02f), 0.0f, 1.0f);
                        Res.Meta.Stability = FMath::Clamp(Res.Meta.Stability + Stream.FRandRange(-0.02f, 0.02f), 0.0f, 1.0f);
                        Res.Meta.Purity = FMath::Clamp(Res.Meta.Purity + Stream.FRandRange(-0.02f, 0.02f), 0.0f, 1.0f);
                    };

                MutateResource(Resource1);
                MutateResource(Resource2);
            }

            TArray<FRealState> Inputs = { Resource1, Resource2 };
            FRealState Result = HerbalistSimulation::UpdateWorld(World, Inputs, Rng);

            UE_LOG(LogHerbalist, Warning, TEXT("Step %d | ResultDist: %.3f | MemDist: %.3f"),
                i, Result.Meta.Distortion, World.Memory.AccumulatedDistortion);
        }
    }
    else
    {
        UE_LOG(LogHerbalist, Warning, TEXT("WorldConfig is NULL - using defaults"));
    }
}