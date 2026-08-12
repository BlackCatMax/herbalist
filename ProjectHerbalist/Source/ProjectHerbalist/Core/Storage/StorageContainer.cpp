#include "Core/Storage/StorageContainer.h"
#include "ProjectHerbalist.h"
#include "HerbalistLogChannels.h"
#include "Player/HerbalistPlayerController.h"
#include "UI/InventoryTransferWidget.h"
#include "Engine/World.h"

AStorageContainer::AStorageContainer()
{
    PrimaryActorTick.bCanEverTick = false;
    InventoryComponent = CreateDefaultSubobject<UHerbalistInventoryComponent>(TEXT("InventoryComponent"));
}

void AStorageContainer::BeginPlay()
{
    Super::BeginPlay();
    if (InventoryComponent)
    {
        InventoryComponent->MaxSlots = MaxSlots;
    }
}

void AStorageContainer::OnInteract(APlayerController* PlayerController)
{
    UE_LOG(LogHerbalistAlchemy, Log, TEXT("AStorageContainer::OnInteract called"));

    AHerbalistPlayerController* PC = Cast<AHerbalistPlayerController>(PlayerController);
    if (!PC)
    {
        UE_LOG(LogHerbalistAlchemy, Error, TEXT("Failed to cast PlayerController to AHerbalistPlayerController"));
        return;
    }

    if (TransferWidgetInstance && TransferWidgetInstance->IsInViewport())
    {
        UE_LOG(LogHerbalistAlchemy, Log, TEXT("Closing TransferWidget"));
        TransferWidgetInstance->RemoveFromParent();
        TransferWidgetInstance = nullptr;
        PC->CurrentTransferWidget = nullptr;
        PC->bShowMouseCursor = false;
        FInputModeGameOnly InputMode;
        PC->SetInputMode(InputMode);
        PC->SetIgnoreLookInput(false);
        PC->bIsAnyWidgetOpen = false;
        return;
    }

    if (PC->bIsAnyWidgetOpen)
    {
        UE_LOG(LogHerbalistAlchemy, Log, TEXT("Another widget is open, cannot open storage"));
        return;
    }

    if (!PC->InventoryComponent || !InventoryComponent || !TransferWidgetClass)
    {
        UE_LOG(LogHerbalistAlchemy, Error, TEXT("Missing components"));
        return;
    }

    if (TransferWidgetInstance)
    {
        TransferWidgetInstance->RemoveFromParent();
        TransferWidgetInstance = nullptr;
    }

    UE_LOG(LogHerbalistAlchemy, Log, TEXT("Creating TransferWidget instance"));
    TransferWidgetInstance = CreateWidget<UInventoryTransferWidget>(GetWorld(), TransferWidgetClass);
    if (!TransferWidgetInstance)
    {
        UE_LOG(LogHerbalistAlchemy, Error, TEXT("Failed to create TransferWidget"));
        return;
    }

    TransferWidgetInstance->BindInventories(PC->InventoryComponent, InventoryComponent);
    TransferWidgetInstance->AddToViewport();

    PC->CurrentTransferWidget = TransferWidgetInstance;
    PC->bShowMouseCursor = true;
    FInputModeGameAndUI InputMode;
    InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
    PC->SetInputMode(InputMode);
    PC->SetIgnoreLookInput(true);
    PC->bIsAnyWidgetOpen = true;

    UE_LOG(LogHerbalistAlchemy, Log, TEXT("TransferWidget opened successfully"));
}