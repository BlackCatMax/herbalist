// Copyright Project Herbalist. All Rights Reserved.

#include "ProjectHerbalistGameModeBase.h"
#include "ProjectHerbalist.h"
#include "Player/HerbalistPlayerController.h"
#include "Core/World/GridWorldManager.h"
#include "Engine/World.h"

AProjectHerbalistGameModeBase::AProjectHerbalistGameModeBase()
{
    PlayerControllerClass = AHerbalistPlayerController::StaticClass();
}

AProjectHerbalistGameModeBase::~AProjectHerbalistGameModeBase()
{
    // Ничего не удаляем (инвентарь больше не здесь)
}

void AProjectHerbalistGameModeBase::BeginPlay()
{
    Super::BeginPlay();
    SpawnWorldManager();
}

void AProjectHerbalistGameModeBase::SpawnWorldManager()
{
    if (!WorldManager)
    {
        FActorSpawnParameters SpawnParams;
        SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        WorldManager = GetWorld()->SpawnActor<AGridWorldManager>(SpawnParams);
        if (WorldManager)
        {
            WorldManager->StateInterpolationSpeed = StateInterpolationSpeed;
            WorldManager->bHarvestAffectsBiome = bHarvestAffectsBiome;
            WorldManager->HarvestStressIncrement = HarvestStressIncrement;
            WorldManager->HarvestStressThreshold = HarvestStressThreshold;
            WorldManager->MaxHarvestImpactOnDistortion = MaxHarvestImpactOnDistortion;
            WorldManager->MaxHarvestImpactOnMagnitude = MaxHarvestImpactOnMagnitude;
            WorldManager->HarvestStressDecayRate = HarvestStressDecayRate;
            WorldManager->bEnableRecovery = bEnableEcologyRecovery;

            UE_LOG(LogHerbalist, Log, TEXT("WorldManager spawned with interpolation speed %.3f, ecology inc=%.4f thresh=%.2f decay=%.4f"),
                StateInterpolationSpeed, HarvestStressIncrement, HarvestStressThreshold, HarvestStressDecayRate);
        }
    }
}