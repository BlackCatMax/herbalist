// AHerbalistResourceActor.cpp
#include "AHerbalistResourceActor.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Core/World/GridWorldManager.h"
#include "Core/Harvest/HarvestService.h"
#include "Core/Inventory/HerbalistInventoryComponent.h"
#include "Core/Data/IngredientRegistry.h"
#include "Player/HerbalistPlayerController.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "ProjectHerbalist.h"

AHerbalistResourceActor::AHerbalistResourceActor()
{
    PrimaryActorTick.bCanEverTick = false;

    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    
    MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
    MeshComponent->SetupAttachment(RootComponent);
    MeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    MeshComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
    MeshComponent->SetCollisionResponseToChannel(ECC_GameTraceChannel1, ECR_Block);
    MeshComponent->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);

    CollisionBox = CreateDefaultSubobject<UBoxComponent>(TEXT("CollisionBox"));
    CollisionBox->SetupAttachment(RootComponent);
    CollisionBox->SetBoxExtent(FVector(50.0f, 50.0f, 50.0f));
    CollisionBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    CollisionBox->SetCollisionResponseToAllChannels(ECR_Ignore);
    CollisionBox->SetCollisionResponseToChannel(ECC_GameTraceChannel1, ECR_Block);
    CollisionBox->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
}

void AHerbalistResourceActor::PostInitializeComponents()
{
    Super::PostInitializeComponents();
    
    if (MeshComponent)
    {
        MeshComponent->SetVisibility(true);
    }
}

void AHerbalistResourceActor::BeginPlay()
{
    Super::BeginPlay();
    
    if (!WorldManager)
        FindAndSetWorldManager();
    
    if ((GridX == -1 || GridY == -1) && WorldManager)
    {
        FVector LocalLoc = GetActorLocation() - WorldManager->GetActorLocation();
        GridX = FMath::RoundToInt(LocalLoc.X / WorldManager->CellSize);
        GridY = FMath::RoundToInt(LocalLoc.Y / WorldManager->CellSize);
        UE_LOG(LogHerbalist, Log, TEXT("%s: Auto-assigned to cell (%d,%d)"), 
               *GetName(), GridX, GridY);
    }
}

void AHerbalistResourceActor::FindAndSetWorldManager()
{
    UWorld* World = GetWorld();
    if (!World) return;
    
    for (TActorIterator<AGridWorldManager> It(World); It; ++It)
    {
        WorldManager = *It;
        break;
    }
    
    if (!WorldManager)
    {
        UE_LOG(LogHerbalist, Warning, TEXT("%s: No GridWorldManager found in level"), *GetName());
    }
}

void AHerbalistResourceActor::Init(FName InIngredientID, const FText& InDisplayName, 
    UStaticMesh* Mesh, const FRealState& InBaseState, const FVector& Location, 
    AGridWorldManager* InWorldManager, int32 InGridX, int32 InGridY)
{
    IngredientID = InIngredientID;
    DisplayName = InDisplayName;
    BaseState = InBaseState;
    WorldManager = InWorldManager;
    GridX = InGridX;
    GridY = InGridY;
    SetActorLocation(Location);
    
    if (Mesh)
    {
        MeshComponent->SetStaticMesh(Mesh);
        MeshComponent->SetVisibility(true);
        
        if (WorldManager)
        {
            float CellSize = WorldManager->CellSize;
            float DesiredScale = CellSize / 100.0f;
            MeshComponent->SetWorldScale3D(FVector(DesiredScale, DesiredScale, DesiredScale * 0.8f));
        }
    }
    else
    {
        MeshComponent->SetVisibility(false);
        UE_LOG(LogHerbalist, Warning, TEXT("%s: No mesh provided for ingredient %s"), 
               *GetName(), *IngredientID.ToString());
    }
    
    UE_LOG(LogHerbalist, Log, TEXT("%s: Initialized at cell (%d,%d) with ingredient %s"), 
           *GetName(), GridX, GridY, *IngredientID.ToString());
}

bool AHerbalistResourceActor::IsInHarvestRange(float MaxDistance) const
{
    APlayerController* PC = GetWorld()->GetFirstPlayerController();
    if (!PC || !PC->GetPawn()) return false;
    
    float Distance = FVector::Dist(PC->GetPawn()->GetActorLocation(), GetActorLocation());
    return Distance <= MaxDistance;
}

void AHerbalistResourceActor::Harvest()
{
    if (bIsBeingHarvested)
    {
        UE_LOG(LogHerbalist, Verbose, TEXT("%s: Already being harvested"), *GetName());
        return;
    }
    
    if (!WorldManager)
    {
        UE_LOG(LogHerbalist, Warning, TEXT("%s: Cannot harvest - No WorldManager"), *GetName());
        return;
    }

    FGridCell* Cell = WorldManager->GetCell(GridX, GridY);
    if (!Cell)
    {
        UE_LOG(LogHerbalist, Warning, TEXT("%s: Cannot harvest - Invalid cell (%d,%d)"), 
               *GetName(), GridX, GridY);
        return;
    }

    bIsBeingHarvested = true;
    
    OnHarvestStarted();

    // Предмет будет добавлен в инвентарь в WorldManager->OnResourceCollected
    UE_LOG(LogHerbalist, Log, TEXT("%s: Harvested %s from cell (%d,%d), delegating to WorldManager"), 
           *GetName(), *IngredientID.ToString(), GridX, GridY);

    if (HarvestParticleSystem)
    {
        UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), HarvestParticleSystem, 
                                                  GetActorLocation(), GetActorRotation(), true);
    }
    
    if (HarvestSound)
    {
        UGameplayStatics::PlaySoundAtLocation(GetWorld(), HarvestSound, GetActorLocation());
    }

    WorldManager->OnResourceCollected(this);
    
    OnHarvestComplete();
    
    StartDisappearAnimation();
}

void AHerbalistResourceActor::StartDisappearAnimation()
{
    if (CollisionBox)
    {
        CollisionBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    }
    if (MeshComponent)
    {
        MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    }
    
    if (DisappearDuration > 0.0f)
    {
        GetWorldTimerManager().SetTimer(DisappearTimerHandle, this, 
                                        &AHerbalistResourceActor::OnDisappearAnimationFinished, 
                                        DisappearDuration, false);
    }
    else
    {
        OnDisappearAnimationFinished();
    }
}

void AHerbalistResourceActor::OnDisappearAnimationFinished()
{
    if (DisappearTimerHandle.IsValid())
    {
        GetWorldTimerManager().ClearTimer(DisappearTimerHandle);
    }
    
    Destroy();
}