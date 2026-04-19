// InventorySlotWidget.cpp
#include "UI/InventorySlotWidget.h"
#include "ProjectHerbalist.h"
#include "Core/Harvest/HerbalistHarvest.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Player/HerbalistPlayerController.h"
#include "UI/InventoryTransferWidget.h"

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
    UE_LOG(LogHerbalist, Log, TEXT("UpdateDisplay: Slot %d, Type=%d, Name='%s', Count=%d"),
        SlotIndex, (int32)CachedItem.Type, *Name, CachedItem.Count);

    if (ItemNameText)
    {
        ItemNameText->SetText(FText::FromString(Name));
    }
    else
    {
        UE_LOG(LogHerbalist, Error, TEXT("UpdateDisplay: ItemNameText is NULL for slot %d!"), SlotIndex);
    }

    if (CountText)
    {
        if (CachedItem.Count > 1)
            CountText->SetText(FText::AsNumber(CachedItem.Count));
        else
            CountText->SetText(FText::GetEmpty());
    }
}

bool UInventorySlotWidget::TryMoveToOtherInventory()
{
    if (!InventoryComponent) return false;

    AHerbalistPlayerController* PC = Cast<AHerbalistPlayerController>(GetWorld()->GetFirstPlayerController());
    if (!PC || !PC->CurrentTransferWidget || !PC->CurrentTransferWidget->IsInViewport())
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

    // Найти реальный индекс предмета в источнике
    int32 RealIndex = -1;
    TArray<FInventoryItem> SourceItems = Source->GetItems();
    for (int32 i = 0; i < SourceItems.Num(); ++i)
    {
        const FInventoryItem& Item = SourceItems[i];
        if (Item.Type == CachedItem.Type &&
            FMath::IsNearlyEqual(Item.State.Magnitude, CachedItem.State.Magnitude, 0.01f) &&
            FMath::IsNearlyEqual(Item.State.Meta.Distortion, CachedItem.State.Meta.Distortion, 0.01f))
        {
            RealIndex = i;
            break;
        }
    }
    if (RealIndex == -1) return false;

    // Создаём предмет для добавления (копия State, Count=1)
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