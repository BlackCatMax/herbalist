// AlchemyTableActor.cpp
#include "AlchemyTableActor.h"
#include "ProjectHerbalist.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Player/HerbalistPlayerController.h"
#include "UI/AlchemyTransferWidget.h"

AAlchemyTableActor::AAlchemyTableActor()
{
    PrimaryActorTick.bCanEverTick = false;
    Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
    RootComponent = Mesh;
    InteractionBox = CreateDefaultSubobject<UBoxComponent>(TEXT("InteractionBox"));
    InteractionBox->SetupAttachment(RootComponent);
    InteractionBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    InteractionBox->SetCollisionResponseToAllChannels(ECR_Ignore);
    InteractionBox->SetCollisionResponseToChannel(ECC_GameTraceChannel1, ECR_Block);
}

void AAlchemyTableActor::OnInteract(AHerbalistPlayerController* PC)
{
    if (!PC || !AlchemyWidgetClass) return;

    if (AlchemyWidgetInstance && AlchemyWidgetInstance->IsInViewport())
    {
        AlchemyWidgetInstance->RemoveFromParent();
        AlchemyWidgetInstance = nullptr;
        PC->bIsAnyWidgetOpen = false;
        PC->SetInputMode(FInputModeGameOnly());
        PC->bShowMouseCursor = false;
        return;
    }

    if (PC->bIsAnyWidgetOpen) return;

    AlchemyWidgetInstance = CreateWidget<UAlchemyTransferWidget>(GetWorld(), AlchemyWidgetClass);
    if (AlchemyWidgetInstance)
    {
        AlchemyWidgetInstance->BindInventory(PC->InventoryComponent);
        AlchemyWidgetInstance->AddToViewport();

        PC->CurrentAlchemyWidget = AlchemyWidgetInstance;
        PC->CurrentAlchemyTable = this;

        PC->bIsAnyWidgetOpen = true;
        PC->SetInputMode(FInputModeUIOnly());
        PC->bShowMouseCursor = true;
    }
}