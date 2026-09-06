// POIActors.cpp
#include "Core/World/POIActors.h"
#include "Core/World/GridWorldManager.h"
#include "Core/Config/HerbalistSettings.h"
#include "Components/StaticMeshComponent.h"
#include "HerbalistLogChannels.h"

// ============================================================================
// Тотем
// ============================================================================

APOI_Totem::APOI_Totem()
{
    PrimaryActorTick.bCanEverTick = true;

    LowerTierMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LowerTierMesh"));
    RootComponent = LowerTierMesh;
    LowerTierMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    MiddleTierMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MiddleTierMesh"));
    MiddleTierMesh->SetupAttachment(RootComponent);
    MiddleTierMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    UpperTierMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("UpperTierMesh"));
    UpperTierMesh->SetupAttachment(RootComponent);
    UpperTierMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void APOI_Totem::Init(AGridWorldManager* InWorldManager, int32 InGridX, int32 InGridY)
{
    WorldManager = InWorldManager;
    GridX = InGridX;
    GridY = InGridY;
}

void APOI_Totem::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (!WorldManager) return;
    const FGridCell* Cell = WorldManager->GetCellConst(GridX, GridY);
    if (!Cell) return;

    // Нижний ярус — всегда виден, интенсивность растёт с Distortion
    // (DESIGN_POI_Art_And_LevelDesign.md §1).
    LowerTierIntensity = Cell->State.Meta.Distortion;

    bMiddleTierVisible = WorldManager->IsTotemMiddleTierVisible();
    MiddleTierMesh->SetVisibility(bMiddleTierVisible);

    // Верхний ярус: тот же порог, что уже читает GetTotemRevealText — не
    // дублируем логику там (текстовый запрос), просто сверяем
    // Cell->State.Meta.Purity напрямую с тем же UHerbalistSettings::
    // TotemUpperTierPurityThreshold, дешевле, чем звать текстовую функцию
    // каждый кадр только ради bool.
    const UHerbalistSettings* Settings = GetHerbalistSettings();
    const float PurityThreshold = Settings ? Settings->TotemUpperTierPurityThreshold : 0.6f;
    bUpperTierVisible = Cell->State.Meta.Purity >= PurityThreshold;
    UpperTierMesh->SetVisibility(bUpperTierVisible);
}

// ============================================================================
// Светлояр
// ============================================================================

APOI_Svetloyar::APOI_Svetloyar()
{
    PrimaryActorTick.bCanEverTick = true;

    LakeMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LakeMesh"));
    RootComponent = LakeMesh;
    LakeMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void APOI_Svetloyar::Init(AGridWorldManager* InWorldManager, int32 InGridX, int32 InGridY)
{
    WorldManager = InWorldManager;
    GridX = InGridX;
    GridY = InGridY;
}

void APOI_Svetloyar::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (!WorldManager) return;

    bCityVisible = WorldManager->IsSvetloyarVisible();
    SoundTier = WorldManager->GetSvetloyarSoundTier();
}

// ============================================================================
// Горюч-камень
// ============================================================================

APOI_GoryuchKamen::APOI_GoryuchKamen()
{
    PrimaryActorTick.bCanEverTick = true;

    StoneMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StoneMesh"));
    RootComponent = StoneMesh;
    StoneMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void APOI_GoryuchKamen::Init(AGridWorldManager* InWorldManager, int32 InGridX, int32 InGridY)
{
    WorldManager = InWorldManager;
    GridX = InGridX;
    GridY = InGridY;
    LastSeenAttemptCount = InWorldManager ? InWorldManager->GetGoryuchKamenApplyAttemptCount() : 0;
}

void APOI_GoryuchKamen::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (!WorldManager) return;

    const int32 CurrentCount = WorldManager->GetGoryuchKamenApplyAttemptCount();
    if (CurrentCount != LastSeenAttemptCount)
    {
        LastSeenAttemptCount = CurrentCount;
        UE_LOG(LogHerbalistWorld, Log, TEXT("[GoryuchKamen] Thud at (%d,%d)"), GridX, GridY);
        OnThud();
    }
}
