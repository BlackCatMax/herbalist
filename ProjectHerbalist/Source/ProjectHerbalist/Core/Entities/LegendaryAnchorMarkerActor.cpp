// LegendaryAnchorMarkerActor.cpp
#include "Core/Entities/LegendaryAnchorMarkerActor.h"
#include "Components/StaticMeshComponent.h"

ALegendaryAnchorMarkerActor::ALegendaryAnchorMarkerActor()
{
    PrimaryActorTick.bCanEverTick = false;

    MarkerMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MarkerMesh"));
    RootComponent = MarkerMesh;
    MarkerMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void ALegendaryAnchorMarkerActor::Init(FName InEntityID, const FIntPoint& InGridCell)
{
    EntityID = InEntityID;
    GridCell = InGridCell;
}
