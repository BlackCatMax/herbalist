#include "UI/InventorySlotWidget.h"
#include "ProjectHerbalist.h"
#include "Core/Harvest/HerbalistHarvest.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Player/HerbalistPlayerController.h"
#include "UI/InventoryTransferWidget.h"

void UInventorySlotWidget::InitializeSlot(int32 InIndex, const FInventoryItem& InItem, UHerbalistInventoryComponent* InInventory)
{
    UE_LOG(LogHerbalist, Log, TEXT("InitializeSlot: index=%d, type=%d"), InIndex, (int32)InItem.Type);
    SlotIndex = InIndex;
    InventoryComponent = InInventory;
    CachedItem = InItem;
    UpdateDisplay();
}

void UInventorySlotWidget::UpdateDisplay()
{
    if (CachedItem.Type == EResourceType::None && CachedItem.State.Magnitude < 0.01f)
    {
        UE_LOG(LogHerbalist, Warning, TEXT("UpdateDisplay: slot %d invalid (empty), setting Empty"), SlotIndex);
        if (ItemNameText)
            ItemNameText->SetText(FText::FromString(TEXT("Empty")));
        return;
    }

    FString Name = FHerbalistHarvest::GetResourceName(CachedItem.Type, false);
    UE_LOG(LogHerbalist, Log, TEXT("UpdateDisplay: slot %d, type=%d, name=%s"), SlotIndex, (int32)CachedItem.Type, *Name);
    if (ItemNameText)
        ItemNameText->SetText(FText::FromString(Name));
}

FReply UInventorySlotWidget::NativeOnMouseButtonDoubleClick(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    UE_LOG(LogHerbalist, Log, TEXT("Double click on slot %d"), SlotIndex);
    if (TryMoveToOtherInventory())
        return FReply::Handled();
    return FReply::Unhandled();
}

bool UInventorySlotWidget::TryMoveToOtherInventory()
{
    // Проверяем, что предмет в слоте валидный
    if (!InventoryComponent || CachedItem.Type == EResourceType::None || CachedItem.State.Magnitude < 0.01f)
    {
        UE_LOG(LogHerbalist, Warning, TEXT("TryMoveToOtherInventory: invalid cached item"));
        return false;
    }

    AHerbalistPlayerController* PC = Cast<AHerbalistPlayerController>(GetWorld()->GetFirstPlayerController());
    if (!PC)
    {
        UE_LOG(LogHerbalist, Warning, TEXT("TryMoveToOtherInventory: no PlayerController"));
        return false;
    }

    // Только если открыт виджет сундука
    if (!PC->CurrentTransferWidget || !PC->CurrentTransferWidget->IsInViewport())
    {
        UE_LOG(LogHerbalist, Log, TEXT("TryMoveToOtherInventory: transfer widget not open"));
        return false;
    }

    // Определяем источник и цель
    UHerbalistInventoryComponent* Source = InventoryComponent;
    UHerbalistInventoryComponent* Target = nullptr;

    if (Source == PC->InventoryComponent)
        Target = PC->CurrentTransferWidget->GetRightInventory();   // из игрока в сундук
    else if (Source == PC->CurrentTransferWidget->GetLeftInventory() || Source == PC->CurrentTransferWidget->GetRightInventory())
        Target = PC->InventoryComponent;                           // из сундука в игрока
    else
    {
        UE_LOG(LogHerbalist, Warning, TEXT("TryMoveToOtherInventory: unknown source inventory"));
        return false;
    }

    if (!Target)
    {
        UE_LOG(LogHerbalist, Warning, TEXT("TryMoveToOtherInventory: target inventory is null"));
        return false;
    }

    // Проверяем место в цели
    if (Target->GetItems().Num() >= Target->MaxSlots)
    {
        UE_LOG(LogHerbalist, Warning, TEXT("TryMoveToOtherInventory: target inventory is full"));
        return false;
    }

    // Находим реальный индекс предмета CachedItem в источнике (по уникальным свойствам)
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

    if (RealIndex == -1)
    {
        UE_LOG(LogHerbalist, Error, TEXT("TryMoveToOtherInventory: cannot find cached item in source inventory (type %d)"), (int32)CachedItem.Type);
        return false;
    }

    FInventoryItem Item = SourceItems[RealIndex];
    UE_LOG(LogHerbalist, Log, TEXT("TryMoveToOtherInventory: moving item type %d from index %d"), (int32)Item.Type, RealIndex);

    // Перемещение
    Source->RemoveItem(RealIndex);
    bool bAdded = Target->AddItem(Item.State, Item.Type);

    if (!bAdded)
    {
        // Откат: возвращаем предмет обратно
        Source->AddItem(Item.State, Item.Type);
        UE_LOG(LogHerbalist, Error, TEXT("TryMoveToOtherInventory: failed to add to target, rolled back"));
        return false;
    }

    UE_LOG(LogHerbalist, Log, TEXT("TryMoveToOtherInventory: successfully moved item type %d"), (int32)Item.Type);
    return true;
}