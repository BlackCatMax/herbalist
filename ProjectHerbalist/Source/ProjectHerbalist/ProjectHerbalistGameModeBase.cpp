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
    // Очистка инвентаря
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
            UE_LOG(LogHerbalist, Log, TEXT("WorldManager spawned at %s"), *WorldManager->GetActorLocation().ToString());
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
}

void AProjectHerbalistGameModeBase::RemoveFromInventory(int32 Index)
{
    if (Inventory.IsValidIndex(Index))
    {
        delete Inventory[Index];
        Inventory.RemoveAt(Index);
        UE_LOG(LogHerbalist, Log, TEXT("Removed from inventory, count=%d"), Inventory.Num());
    }
}