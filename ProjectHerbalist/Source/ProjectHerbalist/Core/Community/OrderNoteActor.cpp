// Core/Community/OrderNoteActor.cpp

#include "Core/Community/OrderNoteActor.h"
#include "Core/World/GridWorldManager.h"
#include "Player/HerbalistPlayerController.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Core/Config/HerbalistSettings.h"
#include "Core/Types/HerbalistActorLabel.h"
#include "Engine/StaticMesh.h"

AOrderNoteActor::AOrderNoteActor()
{
    PrimaryActorTick.bCanEverTick = false;

    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

    MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
    MeshComponent->SetupAttachment(RootComponent);
    MeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    MeshComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
    MeshComponent->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
    MeshComponent->SetRelativeScale3D(FVector(0.3f, 0.3f, 0.05f));

    InteractionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("InteractionSphere"));
    InteractionSphere->SetupAttachment(RootComponent);
    InteractionSphere->SetSphereRadius(60.0f);
    InteractionSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    InteractionSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
    InteractionSphere->SetCollisionResponseToChannel(ECC_GameTraceChannel1, ECR_Block);
    InteractionSphere->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
}

void AOrderNoteActor::Init(int32 InOrderNumber, AGridWorldManager* InWorldManager)
{
    OrderNumber = InOrderNumber;
    WorldManager = InWorldManager;
    SetHerbalistDebugLabel(this, FString::Printf(TEXT("Записка, заказ %d"), OrderNumber));

    // Заглушка -- та же, что у фрагментов, сплющенная в листок.
    if (MeshComponent && !MeshComponent->GetStaticMesh())
    {
        if (const UHerbalistSettings* Settings = GetHerbalistSettings())
        {
            if (UStaticMesh* Placeholder = Settings->PlaceholderMemoryFragmentMesh.LoadSynchronous())
            {
                MeshComponent->SetStaticMesh(Placeholder);
            }
        }
    }
}

void AOrderNoteActor::OnInteract_Implementation(AHerbalistPlayerController* PC)
{
    if (bRead || !PC || !WorldManager)
    {
        return;
    }
    bRead = true;
    WorldManager->ReadOrderNote(OrderNumber, PC);
    Destroy();
}
