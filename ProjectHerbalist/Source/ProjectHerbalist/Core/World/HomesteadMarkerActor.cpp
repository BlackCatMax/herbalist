// HomesteadMarkerActor.cpp
#include "Core/World/HomesteadMarkerActor.h"
#include "Components/StaticMeshComponent.h"

AHomesteadMarkerActor::AHomesteadMarkerActor()
{
    PrimaryActorTick.bCanEverTick = false;

    MarkerMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MarkerMesh"));
    RootComponent = MarkerMesh;
    MarkerMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void AHomesteadMarkerActor::Init(const FIntPoint& InGridCell, EHomesteadMarkerKind InKind, EGardenNiche InNiche)
{
    GridCell = InGridCell;
    Kind = InKind;
    Niche = InNiche;
}
