#include "Core/Storage/StorageContainer.h"
#include "ProjectHerbalist.h"
#include "HerbalistLogChannels.h"
#include "Player/HerbalistPlayerController.h"
#include "UI/InventoryTransferWidget.h"
#include "Core/World/GridWorldManager.h"
#include "Engine/World.h"
#include "EngineUtils.h"

namespace
{
    AGridWorldManager* FindGridWorldManagerForStorage(UWorld* World)
    {
        if (!World) return nullptr;
        for (TActorIterator<AGridWorldManager> It(World); It; ++It)
        {
            return *It;
        }
        return nullptr;
    }
}

AStorageContainer::AStorageContainer()
{
    PrimaryActorTick.bCanEverTick = false;
    InventoryComponent = CreateDefaultSubobject<UHerbalistInventoryComponent>(TEXT("InventoryComponent"));
    // Разумный дефолт для найденного в мире контейнера (см. комментарий у
    // EStorageContainerType, HerbalistInventoryComponent.h) — редактируемо
    // per-instance, если конкретный контейнер в уровне должен быть Cellar.
    InventoryComponent->ContainerType = EStorageContainerType::Basket;
}

UClass* AStorageContainer::GetTransferWidgetClass() const
{
    return TransferWidgetClass.Get();
}

void AStorageContainer::BeginPlay()
{
    Super::BeginPlay();
    if (InventoryComponent)
    {
        InventoryComponent->MaxSlots = MaxSlots;

        // Контейнер карты снова загружен World Partition (или загружен после
        // LoadGame) -- забирает содержимое, которое держал менеджер (2026-09-14).
        if (!bIsHomeStorage)
        {
            TArray<FInventoryItem> PendingItems;
            AGridWorldManager* Manager = FindGridWorldManagerForStorage(GetWorld());
            if (Manager && Manager->ClaimPlacedContainerContents(GetFName(), PendingItems))
            {
                InventoryComponent->RestoreItems(PendingItems);
            }
        }
    }
}

void AStorageContainer::StashContentsForUnload()
{
    // Построенные хранилища живут в постоянном уровне и не выгружаются.
    if (bIsHomeStorage || !InventoryComponent) return;
    if (AGridWorldManager* Manager = FindGridWorldManagerForStorage(GetWorld()))
    {
        Manager->StashPlacedContainerContents(GetFName(), InventoryComponent->GetItems());
    }
}

void AStorageContainer::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    // Выгрузка World Partition уничтожает актор; содержимое переживает её у
    // менеджера (2026-09-14).
    if (EndPlayReason == EEndPlayReason::RemovedFromWorld)
    {
        StashContentsForUnload();
    }
    Super::EndPlay(EndPlayReason);
}

void AStorageContainer::OnInteract_Implementation(AHerbalistPlayerController* PC)
{
    UE_LOG(LogHerbalistAlchemy, Log, TEXT("AStorageContainer::OnInteract called"));

    if (!PC) return;

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