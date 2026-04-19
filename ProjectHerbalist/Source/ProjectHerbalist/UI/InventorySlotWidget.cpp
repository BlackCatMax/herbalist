// InventorySlotWidget.cpp
#include "UI/InventorySlotWidget.h"
#include "ProjectHerbalist.h"
#include "Core/Harvest/HerbalistHarvest.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Player/HerbalistPlayerController.h"
#include "UI/InventoryTransferWidget.h"
#include "UI/AlchemyTransferWidget.h"

void UInventorySlotWidget::InitializeSlot(int32 InIndex, const FInventoryItem& InItem, UHerbalistInventoryComponent* InInventory)
{
    SlotIndex = InIndex;
    InventoryComponent = InInventory;
    CachedItem = InItem;
    UpdateDisplay();
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
        if (CachedItem.Count > 1)
            CountText->SetText(FText::AsNumber(CachedItem.Count));
        else
            CountText->SetText(FText::GetEmpty());
    }
}

int32 UInventorySlotWidget::FindRealIndex() const
{
    if (!InventoryComponent) return -1;
    const TArray<FInventoryItem>& Items = InventoryComponent->GetItems();
    for (int32 i = 0; i < Items.Num(); ++i)
    {
        const FInventoryItem& Item = Items[i];
        if (Item.Type == CachedItem.Type &&
            FMath::IsNearlyEqual(Item.State.Magnitude, CachedItem.State.Magnitude, 0.01f) &&
            FMath::IsNearlyEqual(Item.State.Meta.Distortion, CachedItem.State.Meta.Distortion, 0.01f))
        {
            return i;
        }
    }
    return -1;
}

bool UInventorySlotWidget::TryMoveToOtherInventory()
{
    if (!InventoryComponent) return false;

    AHerbalistPlayerController* PC = Cast<AHerbalistPlayerController>(GetWorld()->GetFirstPlayerController());
    if (!PC) return false;

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
                int32 RealIndex = FindRealIndex();
                if (RealIndex != -1)
                    InventoryComponent->RemoveItem(RealIndex, 1);
                return true;
            }
        }
        return false;
    }

    // 2. Трансфер между инвентарями (сундук)
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

    int32 RealIndex = FindRealIndex();
    if (RealIndex == -1) return false;

    FInventoryItem ItemToAdd = CachedItem;
    ItemToAdd.Count = 1;

    if (Target->AddItem(ItemToAdd, 1))
    {
        Source->RemoveItem(RealIndex, 1);
        return true;
    }
    return false;
}

FReply UInventorySlotWidget::NativeOnMouseButtonDoubleClick(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    if (TryMoveToOtherInventory())
        return FReply::Handled();
    return FReply::Unhandled();
}