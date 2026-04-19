#include "InventorySlotWidget.h"
#include "ProjectHerbalist.h"
#include "Core/Harvest/HerbalistHarvest.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Player/HerbalistPlayerController.h"
#include "UI/InventoryTransferWidget.h"
#include "UI/AlchemyTransferWidget.h"
#include "UI/ItemTooltipWidget.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Input/Reply.h"
#include "InventoryDragDropOperation.h"

void UInventorySlotWidget::InitializeSlot(int32 InIndex, const FInventoryItem& InItem, UHerbalistInventoryComponent* InInventory)
{
    SlotIndex = InIndex;
    InventoryComponent = InInventory;
    CachedItem = InItem;
    UpdateDisplay();
}

void UInventorySlotWidget::SetOtherInventory(UHerbalistInventoryComponent* InOther)
{
    OtherInventory = InOther;
}

void UInventorySlotWidget::Refresh()
{
    UpdateDisplay();
}

int32 UInventorySlotWidget::FindRealIndex() const
{
    if (!InventoryComponent) return -1;
    const TArray<FInventoryItem>& Items = InventoryComponent->GetItems();
    for (int32 i = 0; i < Items.Num(); ++i)
    {
        const FInventoryItem& Item = Items[i];
        if (Item.Type != CachedItem.Type) continue;
        const FRealState& S1 = Item.State;
        const FRealState& S2 = CachedItem.State;
        if (FMath::Abs(S1.Magnitude - S2.Magnitude) > 0.01f) continue;
        if (FMath::Abs(S1.Meta.Distortion - S2.Meta.Distortion) > 0.01f) continue;
        if (FMath::Abs(S1.Meta.Purity - S2.Meta.Purity) > 0.01f) continue;
        if (FMath::Abs(S1.Meta.Stability - S2.Meta.Stability) > 0.01f) continue;
        if (FMath::Abs(S1.Meta.Potency - S2.Meta.Potency) > 0.01f) continue;
        if (FMath::Abs(S1.Meta.Resonance - S2.Meta.Resonance) > 0.01f) continue;
        if (FMath::Abs(S1.Meta.Corruption - S2.Meta.Corruption) > 0.01f) continue;
        if (FMath::Abs(S1.Direction.Body - S2.Direction.Body) > 0.01f) continue;
        if (FMath::Abs(S1.Direction.Mind - S2.Direction.Mind) > 0.01f) continue;
        if (FMath::Abs(S1.Direction.Spirit - S2.Direction.Spirit) > 0.01f) continue;
        if (FMath::Abs(S1.Direction.Nature - S2.Direction.Nature) > 0.01f) continue;
        return i;
    }
    return -1;
}

void UInventorySlotWidget::UpdateDisplay()
{
    if (CachedItem.IsEmpty())
    {
        if (ItemNameText) ItemNameText->SetText(FText::FromString(TEXT("Empty")));
        if (CountText) CountText->SetText(FText::GetEmpty());
        if (ItemIcon) ItemIcon->SetVisibility(ESlateVisibility::Hidden);
        return;
    }

    FString Name = FHerbalistHarvest::GetResourceName(CachedItem.Type, false);
    if (ItemNameText) ItemNameText->SetText(FText::FromString(Name));

    if (CountText)
    {
        CountText->SetText(CachedItem.Count > 1 ? FText::AsNumber(CachedItem.Count) : FText::GetEmpty());
    }

    if (ItemIcon) ItemIcon->SetVisibility(ESlateVisibility::Visible);
}

FReply UInventorySlotWidget::NativeOnMouseButtonDoubleClick(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    if (TryMoveToOtherInventory())
        return FReply::Handled();
    return FReply::Unhandled();
}

bool UInventorySlotWidget::TryMoveToOtherInventory()
{
    if (!InventoryComponent) return false;

    AHerbalistPlayerController* PC = Cast<AHerbalistPlayerController>(GetWorld()->GetFirstPlayerController());
    if (!PC) return false;

    int32 RealIndex = FindRealIndex();
    if (RealIndex == -1) return false;

    // 1. Алхимический виджет
    if (PC->CurrentAlchemyWidget && PC->CurrentAlchemyWidget->IsInViewport())
    {
        UAlchemyTransferWidget* AlchemyWidget = Cast<UAlchemyTransferWidget>(PC->CurrentAlchemyWidget);
        if (AlchemyWidget)
        {
            FInventoryItem ItemToAdd = CachedItem;
            ItemToAdd.Count = 1;
            if (AlchemyWidget->TryAddItemToSlot(ItemToAdd))
            {
                InventoryComponent->RemoveItem(RealIndex, 1);
                return true;
            }
        }
        return false;
    }

    // 2. Трансфер между инвентарями
    if (!PC->CurrentTransferWidget || !PC->CurrentTransferWidget->IsInViewport())
        return false;

    UHerbalistInventoryComponent* Source = InventoryComponent;
    UHerbalistInventoryComponent* Target = nullptr;

    if (Source == PC->InventoryComponent)
        Target = PC->CurrentTransferWidget->GetRightInventory();
    else if (Source == PC->CurrentTransferWidget->GetLeftInventory() || Source == PC->CurrentTransferWidget->GetRightInventory())
        Target = PC->InventoryComponent;
    else
        return false;

    if (!Target) return false;

    return Source->TransferItemTo(RealIndex, Target);
}

FReply UInventorySlotWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && !CachedItem.IsEmpty())
    {
        return FReply::Handled().DetectDrag(TakeWidget(), EKeys::LeftMouseButton);
    }
    return FReply::Unhandled();
}

void UInventorySlotWidget::NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation)
{
    if (!InventoryComponent || CachedItem.IsEmpty())
        return;

    int32 RealIndex = FindRealIndex();
    if (RealIndex == -1) return;

    UInventoryDragDropOperation* DragOp = NewObject<UInventoryDragDropOperation>();
    DragOp->SourceInventory = InventoryComponent;
    DragOp->SourceIndex = RealIndex;

    bool bShiftDown = InMouseEvent.IsShiftDown();
    if (bShiftDown)
    {
        FInventoryItem SplitItem;
        int32 Half = FMath::Max(1, CachedItem.Count / 2);
        if (InventoryComponent->SplitStack(RealIndex, Half, SplitItem))
        {
            DragOp->bIsSplit = true;
            DragOp->SplitItem = SplitItem;
            DragOp->DefaultDragVisual = this;
        }
        else
        {
            return;
        }
    }
    else
    {
        DragOp->DefaultDragVisual = this;
    }

    OutOperation = DragOp;
}

bool UInventorySlotWidget::NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
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

    if (DragOp->SourceInventory != InventoryComponent)
    {
        return DragOp->SourceInventory->TransferItemTo(DragOp->SourceIndex, InventoryComponent);
    }

    return false;
}

void UInventorySlotWidget::NativeOnDragCancelled(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
    UInventoryDragDropOperation* DragOp = Cast<UInventoryDragDropOperation>(InOperation);
    if (DragOp && DragOp->bIsSplit && DragOp->SourceInventory)
    {
        DragOp->SourceInventory->AddItem(DragOp->SplitItem, DragOp->SplitItem.Count);
    }
}

void UInventorySlotWidget::NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    Super::NativeOnMouseEnter(InGeometry, InMouseEvent);

    if (CachedItem.IsEmpty() || !TooltipWidgetClass)
        return;

    if (!ActiveTooltip)
    {
        ActiveTooltip = CreateWidget<UItemTooltipWidget>(GetWorld(), TooltipWidgetClass);
    }

    if (ActiveTooltip)
    {
        ActiveTooltip->SetItem(CachedItem);
        FVector2D MousePos = UWidgetLayoutLibrary::GetMousePositionOnViewport(GetWorld());
        ActiveTooltip->SetPositionInViewport(MousePos + FVector2D(15, 15));
        ActiveTooltip->AddToViewport();
    }
}

void UInventorySlotWidget::NativeOnMouseLeave(const FPointerEvent& InMouseEvent)
{
    Super::NativeOnMouseLeave(InMouseEvent);

    if (ActiveTooltip && ActiveTooltip->IsInViewport())
    {
        ActiveTooltip->RemoveFromParent();
    }
}

void UInventorySlotWidget::NativeDestruct()
{
    if (ActiveTooltip && ActiveTooltip->IsInViewport())
    {
        ActiveTooltip->RemoveFromParent();
        ActiveTooltip = nullptr;
    }
    Super::NativeDestruct();
}