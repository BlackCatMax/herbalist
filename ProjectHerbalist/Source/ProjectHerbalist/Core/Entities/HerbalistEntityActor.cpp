// HerbalistEntityActor.cpp
#include "Core/Entities/HerbalistEntityActor.h"
#include "Components/StaticMeshComponent.h"

AHerbalistEntityActor::AHerbalistEntityActor()
{
    PrimaryActorTick.bCanEverTick = false;

    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

    MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
    MeshComponent->SetupAttachment(RootComponent);
    // Без меша по умолчанию (невидимый маркер) -- см. комментарий у класса
    // в HerbalistEntityActor.h. Коллизия не нужна, пока у базового класса
    // нет собственного взаимодействия (OnInteract_Implementation пуст).
    MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void AHerbalistEntityActor::Init(FName InEntityID, const FIntPoint& InCell, AGridWorldManager* InWorldManager)
{
    EntityID = InEntityID;
    GridCell = InCell;
    WorldManagerRef = InWorldManager;
}
