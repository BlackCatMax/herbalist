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
    for (FRealState* Res : Inventory)
    {
        delete Res;
    }
    Inventory.Empty();
}

void AProjectHerbalistGameModeBase::BeginPlay()
{
    Super::BeginPlay();
    SpawnWorldManager();
    Inventory.Empty();
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

void AProjectHerbalistGameModeBase::AddToInventory(const FRealState& Resource)
{
    if (FMath::IsNaN(Resource.Magnitude) || Resource.Magnitude < -0.1f || Resource.Magnitude > 1.1f)
    {
        UE_LOG(LogHerbalist, Error, TEXT("Rejected corrupted resource: Mag=%.2f"), Resource.Magnitude);
        return;
    }
    FRealState* NewResource = new FRealState(Resource);
    Inventory.Add(NewResource);
    UE_LOG(LogHerbalist, Log, TEXT("Added to inventory, count=%d, Mag=%.2f"), Inventory.Num(), NewResource->Magnitude);
    OnInventoryChanged.Broadcast();
}

void AProjectHerbalistGameModeBase::RemoveFromInventory(int32 Index)
{
    if (Inventory.IsValidIndex(Index))
    {
        delete Inventory[Index];
        Inventory.RemoveAt(Index);
        UE_LOG(LogHerbalist, Log, TEXT("Removed from inventory, count=%d"), Inventory.Num());
        OnInventoryChanged.Broadcast();
    }
}