#include "UI/InventoryWidget.h"
#include "ProjectHerbalist.h"
#include "Core/Inventory/HerbalistInventoryComponent.h"
#include "Core/Harvest/HerbalistHarvest.h"
#include "UI/InventorySlotWidget.h"
#include "Components/VerticalBox.h"

void UInventoryWidget::BindInventory(UHerbalistInventoryComponent* InInventory)
{
    UE_LOG(LogHerbalist, Log, TEXT("BindInventory called, InInventory=%s"), InInventory ? TEXT("valid") : TEXT("null"));
    if (InventoryComponent)
    {
        InventoryComponent->OnInventoryChanged.RemoveDynamic(this, &UInventoryWidget::OnInventoryChanged);
    }
    InventoryComponent = InInventory;
    if (InventoryComponent)
    {
        InventoryComponent->OnInventoryChanged.AddDynamic(this, &UInventoryWidget::OnInventoryChanged);
        RefreshInventoryDisplay();
    }
    else
    {
        UE_LOG(LogHerbalist, Warning, TEXT("BindInventory: InventoryComponent is null"));
    }
}

void UInventoryWidget::NativeConstruct()
{
    Super::NativeConstruct();
    UE_LOG(LogHerbalist, Log, TEXT("UInventoryWidget::NativeConstruct"));
}

void UInventoryWidget::NativeDestruct()
{
    UE_LOG(LogHerbalist, Log, TEXT("UInventoryWidget::NativeDestruct"));
    if (InventoryComponent)
    {
        InventoryComponent->OnInventoryChanged.RemoveDynamic(this, &UInventoryWidget::OnInventoryChanged);
    }
    Super::NativeDestruct();
}

void UInventoryWidget::OnInventoryChanged()
{
    UE_LOG(LogHerbalist, Log, TEXT("OnInventoryChanged triggered"));
    RefreshInventoryDisplay();
}

void UInventoryWidget::RefreshInventoryDisplay()
{
    UE_LOG(LogHerbalist, Log, TEXT("RefreshInventoryDisplay called"));
    if (!InventoryComponent)
    {
        UE_LOG(LogHerbalist, Warning, TEXT("RefreshInventoryDisplay: InventoryComponent is null"));
        return;
    }
    if (!SlotContainer)
    {
        UE_LOG(LogHerbalist, Warning, TEXT("RefreshInventoryDisplay: SlotContainer is null (BindWidget failed)"));
        return;
    }
    if (!SlotWidgetClass)
    {
        UE_LOG(LogHerbalist, Warning, TEXT("RefreshInventoryDisplay: SlotWidgetClass is null (not set in Blueprint)"));
        return;
    }

    ClearSlots();

    TArray<FInventoryItem> Items = InventoryComponent->GetItems();
    UE_LOG(LogHerbalist, Log, TEXT("RefreshInventoryDisplay: %d items in inventory"), Items.Num());

    Items.Sort([](const FInventoryItem& A, const FInventoryItem& B) {
        FString NameA = FHerbalistHarvest::GetResourceName(A.Type, false);
        FString NameB = FHerbalistHarvest::GetResourceName(B.Type, false);
        return NameA < NameB;
        });

    for (int32 i = 0; i < Items.Num(); ++i)
    {
        UInventorySlotWidget* NewSlot = CreateWidget<UInventorySlotWidget>(GetWorld(), SlotWidgetClass);
        if (NewSlot)
        {
            UE_LOG(LogHerbalist, Log, TEXT("Creating slot %d, item type=%d"), i, (int32)Items[i].Type);
            NewSlot->InitializeSlot(i, Items[i], InventoryComponent);
            SlotContainer->AddChildToVerticalBox(NewSlot);
        }
        else
        {
            UE_LOG(LogHerbalist, Error, TEXT("Failed to create slot widget for index %d"), i);
        }
    }
}

void UInventoryWidget::ClearSlots()
{
    if (SlotContainer)
    {
        int32 ChildCount = SlotContainer->GetChildrenCount();
        UE_LOG(LogHerbalist, Log, TEXT("ClearSlots: removing %d children"), ChildCount);
        SlotContainer->ClearChildren();
    }
}