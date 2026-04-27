// InventorySlotWidget.cpp
#include "InventorySlotWidget.h"
#include "ProjectHerbalist.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Player/HerbalistPlayerController.h"
#include "UI/InventoryTransferWidget.h"
#include "UI/AlchemyTransferWidget.h"
#include "UI/ItemTooltipWidget.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Input/Reply.h"
#include "Core/Inventory/InventoryDragDropOperation.h"
#include "Core/Types/HerbalistCoreMath.h"
#include "UI/InventoryDragDropController.h"
#include "Core/Types/HerbalistIngredient.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "Core/Subsystems/IngredientRegistrySubsystem.h"
#include "Core/Data/IngredientTableRow.h"

void UInventorySlotWidget::InitializeSlot(int32 InIndex, const FInventoryItem& InItem, UHerbalistInventoryComponent* InInventory)
{
    SlotIndex = InIndex;
    InventoryComponent = InInventory;
    CachedItem = InItem;
    UpdateDisplay();
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
        if (Item.IngredientID != CachedItem.IngredientID) continue;
        if (HerbalistCore::Math::AreStatesSimilar(Item.State, CachedItem.State))
        {
            return i;
        }
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

    FString DisplayName = CachedItem.IngredientID.ToString();

    AHerbalistPlayerController* PC = Cast<AHerbalistPlayerController>(GetOwningPlayer());
    UIngredientRegistrySubsystem* IngredientSubsystem = nullptr;
    if (PC)
    {
        UGameInstance* GameInstance = PC->GetGameInstance();
        IngredientSubsystem = GameInstance ? GameInstance->GetSubsystem<UIngredientRegistrySubsystem>() : nullptr;
    }

    if (CachedItem.IngredientID != FName(TEXT("Potion")) &&
        CachedItem.IngredientID != FName(TEXT("Water")) &&
        !CachedItem.IngredientID.IsNone())
    {
        if (IngredientSubsystem)
        {
            if (const FIngredientTableRow* Row = IngredientSubsystem->GetRow(CachedItem.IngredientID))
            {
                DisplayName = Row->DisplayName.ToString();
            }
        }
    }
    else if (CachedItem.IngredientID == FName(TEXT("Potion")))
    {
        DisplayName = TEXT("Зелье");
    }
    else if (CachedItem.IngredientID == FName(TEXT("Water")))
    {
        DisplayName = TEXT("Вода");
    }

    if (ItemNameText) ItemNameText->SetText(FText::FromString(DisplayName));
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

    // Если открыт алхимический виджет
    if (PC->CurrentAlchemyWidget && PC->CurrentAlchemyWidget->IsInViewport())
    {
        UAlchemyTransferWidget* AlchemyWidget = Cast<UAlchemyTransferWidget>(PC->CurrentAlchemyWidget);
        if (AlchemyWidget)
        {
            FInventoryItem ItemToAdd = CachedItem;
            ItemToAdd.Count = 1;
            UAlchemySlotWidget* TargetSlot = AlchemyWidget->FindSuitableSlot(ItemToAdd);
            if (UInventoryDragDropController::TryAddToAlchemySlot(ItemToAdd, TargetSlot, PC->InventoryComponent))
            {
                return true;
            }
        }
        return false;
    }

    // Если открыт трансферный виджет
    if (!PC->CurrentTransferWidget || !PC->CurrentTransferWidget->IsInViewport())
        return false;

    UHerbalistInventoryComponent* TargetInventory = nullptr;
    if (InventoryComponent == PC->InventoryComponent)
        TargetInventory = PC->CurrentTransferWidget->GetRightInventory();
    else if (InventoryComponent == PC->CurrentTransferWidget->GetLeftInventory() || InventoryComponent == PC->CurrentTransferWidget->GetRightInventory())
        TargetInventory = PC->InventoryComponent;
    else
        return false;

    if (!TargetInventory) return false;

    return UInventoryDragDropController::TryTransferItem(InventoryComponent, RealIndex, TargetInventory);
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
        return UInventoryDragDropController::TryAddSplitItem(DragOp->SplitItem, DragOp->SourceInventory, InventoryComponent, DragOp);
    }

    if (DragOp->SourceInventory != InventoryComponent)
    {
        return UInventoryDragDropController::TryTransferItem(DragOp->SourceInventory, DragOp->SourceIndex, InventoryComponent);
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
        float GlobalDistortion = 0.3f;
        if (AHerbalistPlayerController* HPC = Cast<AHerbalistPlayerController>(GetOwningPlayer()))
        {
            GlobalDistortion = HPC->CurrentGlobalDistortion;
        }

        ActiveTooltip->SetItem(CachedItem, GlobalDistortion);

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