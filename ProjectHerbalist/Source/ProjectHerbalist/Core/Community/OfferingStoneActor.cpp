// OfferingStoneActor.cpp
#include "Core/Community/OfferingStoneActor.h"
#include "Core/World/GridWorldManager.h"
#include "Core/Inventory/HerbalistInventoryComponent.h"
#include "HerbalistLogChannels.h"
#include "Player/HerbalistPlayerController.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SphereComponent.h"

AOfferingStoneActor::AOfferingStoneActor()
{
    PrimaryActorTick.bCanEverTick = false;

    MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
    RootComponent = MeshComponent;
    MeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    MeshComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
    MeshComponent->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);

    // Та же сфера, что у тайника: камень ловит взгляд и без меша.
    InteractionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("InteractionSphere"));
    InteractionSphere->SetupAttachment(RootComponent);
    InteractionSphere->SetSphereRadius(60.0f);
    InteractionSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    InteractionSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
    InteractionSphere->SetCollisionResponseToChannel(ECC_GameTraceChannel1, ECR_Block);
    InteractionSphere->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
}

void AOfferingStoneActor::OnInteract_Implementation(AHerbalistPlayerController* PC)
{
    UE_LOG(LogHerbalistWorld, Log, TEXT("[Community] Камень-жертвенник: подношение кладут рукой"));
}

bool AOfferingStoneActor::ReceiveHeldItem(AHerbalistPlayerController* PC, int32 InventoryIndex)
{
    if (!PC || !PC->InventoryComponent || !PC->InventoryComponent->GetItems().IsValidIndex(InventoryIndex)) return false;
    AGridWorldManager* Manager = PC->FindWorldManager();
    if (!Manager) return true;

    FInventoryItem Offering = PC->InventoryComponent->GetItems()[InventoryIndex];
    Offering.Count = 1;
    if (PC->IsArtifactReceipt(Offering))
    {
        UE_LOG(LogHerbalistWorld, Log, TEXT("[Community] '%s' -- артефакт, на камень не кладут"), *Offering.IngredientID.ToString());
        return true;
    }
    // Тот же путь, что у команды OfferToCommunity: менеджер меняет Молву,
    // предмет списывает вызывающая сторона.
    const float DeltaMolva = Manager->OfferToCommunity({ Offering });
    PC->InventoryComponent->RemoveItem(InventoryIndex, 1);
    UE_LOG(LogHerbalistWorld, Log, TEXT("[Community] Подношение '%s' на камне: Молва %+.3f"),
        *Offering.IngredientID.ToString(), DeltaMolva);
    return true;
}
