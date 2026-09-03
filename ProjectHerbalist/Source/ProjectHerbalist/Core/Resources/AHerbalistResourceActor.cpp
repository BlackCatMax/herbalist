// AHerbalistResourceActor.cpp
#include "AHerbalistResourceActor.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Core/Inventory/HerbalistInventoryComponent.h"
#include "Core/Subsystems/IngredientRegistrySubsystem.h"
#include "Core/Types/HerbalistActorLabel.h"
#include "Core/World/GridWorldManager.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Player/HerbalistPlayerController.h"
#include "ProjectHerbalist.h"
#include "HerbalistLogChannels.h"
#include "TimerManager.h"

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

    HarvestSphere = CreateDefaultSubobject<USphereComponent>(TEXT("HarvestSphere"));
    HarvestSphere->SetupAttachment(RootComponent);
    HarvestSphere->SetSphereRadius(70.0f);
    HarvestSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    HarvestSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
    HarvestSphere->SetCollisionResponseToChannel(ECC_GameTraceChannel1, ECR_Block);
    HarvestSphere->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
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
        UE_LOG(LogHerbalistHarvest, Verbose, TEXT("%s: Auto-assigned to cell (%d,%d)"),
            *GetName(), GridX, GridY);
    }

    // Актор, поставленный в мир напрямую (уровень/PCG-граф) без вызова
    // Init() -- WorldManager/GridX/GridY выше уже определены автоматически,
    // данные строки добираем из реестра (2026-09-03), регистрация та же,
    // что Init() делает для C++-пути.
    if (!bInitializedFromCode)
    {
        ResolveFromIngredientRegistry();
    }
    RegisterOnCell();
}

void AHerbalistResourceActor::ResolveFromIngredientRegistry(UIngredientRegistrySubsystem* Registry)
{
    if (bInitializedFromCode || IngredientID.IsNone()) return;

    if (!Registry)
    {
        UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
        Registry = GameInstance ? GameInstance->GetSubsystem<UIngredientRegistrySubsystem>() : nullptr;
    }
    const FIngredientTableRow* Row = Registry ? Registry->GetRow(IngredientID) : nullptr;
    if (!Row)
    {
        UE_LOG(LogHerbalistHarvest, Warning, TEXT("%s: ингредиент '%s' не найден в DT_IngredientClass -- актор останется с пустыми данными"),
            *GetName(), *IngredientID.ToString());
        return;
    }

    DisplayName  = Row->DisplayName;
    BaseState    = Row->BaseState;
    Resilience   = Row->Resilience;
    bIronAverse  = Row->bIronAverse;
    bDelicate    = Row->bDelicate;

    // Меш из строки — только если своего нет: Blueprint-наследник, у которого
    // меш выставлен в дефолтах компонента, всегда главнее (тот же принцип,
    // что у меша-заглушки сущностей, HerbalistEntityActor.cpp).
    if (MeshComponent && !MeshComponent->GetStaticMesh() && Row->ResourceMesh)
    {
        MeshComponent->SetStaticMesh(Row->ResourceMesh);
        MeshComponent->SetVisibility(true);
    }

    // Тот же довод, что в Init() -- но этот путь проходят акторы, которые
    // Init() не звали вовсе (PCG-граф, ручная расстановка на уровне), и без
    // подписи здесь они остались бы безымянными именно в том сценарии,
    // ради которого PCG-путь и заводился.
    SetHerbalistDebugLabel(this, FString::Printf(TEXT("%s (%d,%d)"),
        *IngredientID.ToString(), GridX, GridY));

    bInitializedFromCode = true;
}

void AHerbalistResourceActor::RegisterOnCell()
{
    if (!WorldManager) return;
    if (FGridCell* Cell = WorldManager->GetCell(GridX, GridY))
    {
        Cell->ResourceActors.AddUnique(this);
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
        UE_LOG(LogHerbalistHarvest, Warning, TEXT("%s: No GridWorldManager found in level"), *GetName());
    }
}

void AHerbalistResourceActor::Init(FName InIngredientID, const FText& InDisplayName,
    UStaticMesh* Mesh, const FRealState& InBaseState, const FVector& Location,
    AGridWorldManager* InWorldManager, int32 InGridX, int32 InGridY,
    float InResilience, bool InIronAverse, bool InDelicate)
{
    IngredientID = InIngredientID;
    DisplayName = InDisplayName;
    BaseState = InBaseState;
    Resilience = InResilience;
    bIronAverse = InIronAverse;
    bDelicate = InDelicate;
    bInitializedFromCode = true;
    bSpawnedByGrid = true;   // C++-путь спавна -- см. WasSpawnedByGrid()
    WorldManager = InWorldManager;
    GridX = InGridX;
    GridY = InGridY;
    SetActorLocation(Location);

    // Саморегистрация в Cell.ResourceActors (2026-09-02) -- та же RegisterOnCell,
    // что и у BeginPlay()'s авто-определения; AddUnique внутри неё не
    // задвоит, если Init() вызван на уже зарегистрированном акторе повторно.
    RegisterOnCell();

    if (Mesh)
    {
        MeshComponent->SetStaticMesh(Mesh);
        MeshComponent->SetVisibility(true);
    }
    else
    {
        MeshComponent->SetVisibility(false);
        UE_LOG(LogHerbalistHarvest, Verbose, TEXT("%s: No mesh provided for ingredient %s"),
            *GetName(), *IngredientID.ToString());
    }

    // Подпись для аутлайнера/выделения в PIE -- см. HerbalistActorLabel.h.
    // Клетка в имени не косметика: одинаковых "ste_06" в мире тысячи, и
    // без координат выделенный актор всё равно неотличим от соседнего.
    SetHerbalistDebugLabel(this, FString::Printf(TEXT("%s (%d,%d)"),
        *IngredientID.ToString(), GridX, GridY));

    UE_LOG(LogHerbalistHarvest, Verbose, TEXT("%s: Initialized at cell (%d,%d) with ingredient %s"),
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
        UE_LOG(LogHerbalistHarvest, Verbose, TEXT("%s: Already being harvested"), *GetName());
        return;
    }

    if (!WorldManager)
    {
        UE_LOG(LogHerbalistHarvest, Warning, TEXT("%s: Cannot harvest - No WorldManager"), *GetName());
        return;
    }

    FGridCell* Cell = WorldManager->GetCell(GridX, GridY);
    if (!Cell)
    {
        UE_LOG(LogHerbalistHarvest, Warning, TEXT("%s: Cannot harvest - Invalid cell (%d,%d)"),
            *GetName(), GridX, GridY);
        return;
    }

    bIsBeingHarvested = true;

    OnHarvestStarted();

    WorldManager->OnResourceCollected(this);

    OnHarvestComplete();

    StartDisappearAnimation();
}

void AHerbalistResourceActor::StartDisappearAnimation()
{
    if (HarvestSphere)
    {
        HarvestSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
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