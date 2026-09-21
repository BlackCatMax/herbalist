// OrderCacheActor.cpp
#include "Core/Community/OrderCacheActor.h"
#include "Core/World/GridWorldManager.h"
#include "Player/HerbalistPlayerController.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SphereComponent.h"
#include "EngineUtils.h"

AOrderCacheActor::AOrderCacheActor()
{
    PrimaryActorTick.bCanEverTick = false;

    MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
    RootComponent = MeshComponent;
    MeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    MeshComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
    MeshComponent->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);

    // Та же сфера взаимодействия, что у кургана: тайник даёт себя выделить
    // чуть раньше самого меша -- пока меша нет вовсе, только по ней.
    InteractionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("InteractionSphere"));
    InteractionSphere->SetupAttachment(RootComponent);
    InteractionSphere->SetSphereRadius(60.0f);
    InteractionSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    InteractionSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
    InteractionSphere->SetCollisionResponseToChannel(ECC_GameTraceChannel1, ECR_Block);
    InteractionSphere->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
}

void AOrderCacheActor::BeginPlay()
{
    Super::BeginPlay();
    for (TActorIterator<AGridWorldManager> It(GetWorld()); It; ++It)
    {
        RegisteredManager = *It;
        It->RegisterOrderCache(this);
        break;
    }
}

void AOrderCacheActor::EndPlay(const EEndPlayReason::Type Reason)
{
    if (AGridWorldManager* Manager = RegisteredManager.Get())
    {
        Manager->UnregisterOrderCache(this);
    }
    Super::EndPlay(Reason);
}

void AOrderCacheActor::OnInteract_Implementation(AHerbalistPlayerController* PC)
{
    if (PC)
    {
        PC->OpenOrdersWindow();
    }
}
