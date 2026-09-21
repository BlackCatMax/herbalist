// SleepBenchActor.cpp
#include "Core/Storage/SleepBenchActor.h"
#include "Player/HerbalistPlayerController.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SphereComponent.h"

ASleepBenchActor::ASleepBenchActor()
{
    PrimaryActorTick.bCanEverTick = false;

    MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
    RootComponent = MeshComponent;
    MeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    MeshComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
    MeshComponent->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);

    InteractionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("InteractionSphere"));
    InteractionSphere->SetupAttachment(RootComponent);
    InteractionSphere->SetSphereRadius(60.0f);
    InteractionSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    InteractionSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
    InteractionSphere->SetCollisionResponseToChannel(ECC_GameTraceChannel1, ECR_Block);
    InteractionSphere->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
}

void ASleepBenchActor::OnInteract_Implementation(AHerbalistPlayerController* PC)
{
    if (PC)
    {
        PC->SleepUntilDawn();
    }
}
