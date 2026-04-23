#include "UI/InventoryWidget.h"
#include "ProjectHerbalist.h"
#include "Core/Inventory/HerbalistInventoryComponent.h"
#include "UI/InventorySlotWidget.h"
#include "Core/Inventory/InventoryDragDropOperation.h"
#include "Components/VerticalBox.h"

void UInventoryWidget::BindInventory(UHerbalistInventoryComponent* InInventory)
{
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
}

void UInventoryWidget::SetOtherInventory(UHerbalistInventoryComponent* InOther)
{
    OtherInventory = InOther;
}

void UInventoryWidget::NativeConstruct()
{
    Super::NativeConstruct();
}

void UInventoryWidget::NativeDestruct()
{
    if (InventoryComponent)
    {
        InventoryComponent->OnInventoryChanged.RemoveDynamic(this, &UInventoryWidget::OnInventoryChanged);
    }
    Super::NativeDestruct();
}

void UInventoryWidget::OnInventoryChanged()
{
    RefreshInventoryDisplay();
}

void UInventoryWidget::RefreshInventoryDisplay()
{
    if (!InventoryComponent || !SlotContainer || !SlotWidgetClass)
        return;

    ClearSlots();

    TArray<FInventoryItem> Items = InventoryComponent->GetItems();

    for (int32 i = 0; i < Items.Num(); ++i)
    {
        UInventorySlotWidget* NewSlot = CreateWidget<UInventorySlotWidget>(GetWorld(), SlotWidgetClass);
        if (NewSlot)
        {
            NewSlot->InitializeSlot(i, Items[i], InventoryComponent);
            NewSlot->SetOtherInventory(OtherInventory);
            SlotContainer->AddChildToVerticalBox(NewSlot);
        }
    }
}

void UInventoryWidget::ClearSlots()
{
    if (SlotContainer)
    {
        SlotContainer->ClearChildren();
    }
}

bool UInventoryWidget::NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
    UInventoryDragDropOperation* DragOp = Cast<UInventoryDragDropOperation>(InOperation);
    if (!DragOp || !InventoryComponent)
        return false;

    if (DragOp->bIsSplit)
    {
        if (InventoryComponent->AddItem(DragOp->SplitItem, DragOp->SplitItem.Count))
        {
            DragOp->bIsSplit = false;
            return true;
        }
        return false;
    }

    if (DragOp->SourceInventory && DragOp->SourceInventory != InventoryComponent)
    {
        return DragOp->SourceInventory->TransferItemTo(DragOp->SourceIndex, InventoryComponent);
    }

    return false;
}