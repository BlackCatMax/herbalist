#include "UI/InventoryWidget.h"
#include "ProjectHerbalist.h"
#include "HerbalistLogChannels.h"
#include "Core/Inventory/HerbalistInventoryComponent.h"
#include "UI/InventorySlotWidget.h"
#include "UI/HerbalistWidgetSizing.h"
#include "UI/InventoryDragDropController.h"
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

void UInventoryWidget::NativeConstruct()
{
    Super::NativeConstruct();
    HerbalistUI::LetSizeBoxesGrowWithContent(WidgetTree);
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
            SlotContainer->AddChildToVerticalBox(NewSlot);
        }
    }
	
	UE_LOG(LogHerbalistUI, Warning, TEXT("RefreshInventoryDisplay: %d items in inventory"), InventoryComponent ? InventoryComponent->GetItems().Num() : 0);
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
        // Тот же путь, что у дропа на слот: сплит ложится целиком или не
        // ложится, остаток возвращает отмена перетаскивания.
        return UInventoryDragDropController::TryAddSplitItem(DragOp->SplitItem, DragOp->SourceInventory, InventoryComponent, DragOp);
    }

    if (DragOp->SourceInventory && DragOp->SourceInventory != InventoryComponent)
    {
        return DragOp->SourceInventory->TransferItemTo(DragOp->SourceIndex, InventoryComponent);
    }

    return false;
}