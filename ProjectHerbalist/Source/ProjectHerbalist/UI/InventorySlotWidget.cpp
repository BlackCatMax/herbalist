#include "UI/InventorySlotWidget.h"
#include "ProjectHerbalist.h"
#include "Core/Harvest/HerbalistHarvest.h"
#include "Core/Inventory/InventoryDragDropOperation.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"

void UInventorySlotWidget::InitializeSlot(int32 InIndex, const FInventoryItem& InItem, UHerbalistInventoryComponent* InInventory)
{
    UE_LOG(LogHerbalist, Log, TEXT("InitializeSlot: index=%d, type=%d"), InIndex, (int32)InItem.Type);
    SlotIndex = InIndex;          // может пригодиться для перетаскивания
    InventoryComponent = InInventory;
    CachedItem = InItem;          // <-- сохраняем копию
    UpdateDisplay();
}

void UInventorySlotWidget::UpdateDisplay()
{
    // Используем CachedItem, а не InventoryComponent->GetItems()[SlotIndex]
    if (CachedItem.Type == EResourceType::None && CachedItem.State.Magnitude < 0.01f)
    {
        UE_LOG(LogHerbalist, Warning, TEXT("UpdateDisplay: slot %d invalid (empty), setting Empty"), SlotIndex);
        if (ItemNameText)
        {
            ItemNameText->SetText(FText::FromString(TEXT("Empty")));
        }
        return;
    }

    FString Name = FHerbalistHarvest::GetResourceName(CachedItem.Type, false);
    UE_LOG(LogHerbalist, Log, TEXT("UpdateDisplay: slot %d, type=%d, name=%s"), SlotIndex, (int32)CachedItem.Type, *Name);
    if (ItemNameText)
    {
        ItemNameText->SetText(FText::FromString(Name));
    }
}

FReply UInventorySlotWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && InventoryComponent && SlotIndex < InventoryComponent->GetItems().Num())
    {
        return FReply::Handled().DetectDrag(TakeWidget(), EKeys::LeftMouseButton);
    }
    return FReply::Unhandled();
}

void UInventorySlotWidget::NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation)
{
    if (!InventoryComponent || SlotIndex < 0 || SlotIndex >= InventoryComponent->GetItems().Num())
        return;

    UInventoryDragDropOperation* DragOp = NewObject<UInventoryDragDropOperation>();
    DragOp->SourceIndex = SlotIndex;
    DragOp->SourceInventory = InventoryComponent;
    DragOp->DefaultDragVisual = this;
    OutOperation = DragOp;
}

bool UInventorySlotWidget::NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
    UInventoryDragDropOperation* DragOp = Cast<UInventoryDragDropOperation>(InOperation);
    if (!DragOp || !DragOp->SourceInventory)
        return false;

    return TryMoveItem(DragOp);
}

bool UInventorySlotWidget::TryMoveItem(UInventoryDragDropOperation* DragOp)
{
    if (DragOp->SourceInventory == InventoryComponent)
    {
        if (SlotIndex == DragOp->SourceIndex) return false;
        TArray<FInventoryItem> Items = InventoryComponent->GetItems();
        if (!Items.IsValidIndex(DragOp->SourceIndex) || !Items.IsValidIndex(SlotIndex))
            return false;
        Items.Swap(DragOp->SourceIndex, SlotIndex);
        InventoryComponent->Clear();
        for (const FInventoryItem& Item : Items)
        {
            InventoryComponent->AddItem(Item.State, Item.Type);
        }
        InventoryComponent->OnInventoryChanged.Broadcast();
        return true;
    }
    return false;
}