#include "AlchemyTableActor.h"
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
    
    if (PC->CurrentAlchemyWidget && PC->CurrentAlchemyWidget->IsInViewport())
    {
        PC->CurrentAlchemyWidget->RemoveFromParent();
        PC->CurrentAlchemyWidget = nullptr;
        PC->bIsAnyWidgetOpen = false;
        PC->SetInputMode(FInputModeGameOnly());
        PC->bShowMouseCursor = false;
        return;
    }
    
    UAlchemyTransferWidget* Widget = CreateWidget<UAlchemyTransferWidget>(PC, AlchemyWidgetClass);
    if (Widget)
    {
        Widget->BindInventory(PC->InventoryComponent);
        Widget->AddToViewport();
        PC->CurrentAlchemyWidget = Widget;
        PC->CurrentAlchemyTable = this;
        PC->bIsAnyWidgetOpen = true;
        PC->SetInputMode(FInputModeUIOnly());
        PC->bShowMouseCursor = true;
    }
}