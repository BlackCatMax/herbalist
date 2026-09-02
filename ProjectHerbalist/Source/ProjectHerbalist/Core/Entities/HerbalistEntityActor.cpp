// HerbalistEntityActor.cpp
#include "Core/Entities/HerbalistEntityActor.h"
#include "Components/StaticMeshComponent.h"
#include "Core/Config/HerbalistSettings.h"
#include "Engine/StaticMesh.h"

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

    // Меш-заглушка (2026-09-02) — только если у класса, которым нас
    // заспавнили, меша нет вовсе. Blueprint карточки (ActorClass в таблице
    // ранга) выставляет свой меш в дефолтах компонента, и тогда эта ветка не
    // срабатывает — плейсхолдер никогда не перетирает настоящий контент.
    if (MeshComponent && !MeshComponent->GetStaticMesh())
    {
        if (const UHerbalistSettings* Settings = GetHerbalistSettings())
        {
            if (UStaticMesh* Placeholder = Settings->PlaceholderEntityMesh.LoadSynchronous())
            {
                MeshComponent->SetStaticMesh(Placeholder);
            }
        }
    }
}
