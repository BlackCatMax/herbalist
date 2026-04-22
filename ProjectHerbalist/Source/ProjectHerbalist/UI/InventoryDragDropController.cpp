// InventoryDragDropController.cpp
#include "InventoryDragDropController.h"
#include "Core/Inventory/HerbalistInventoryComponent.h"
#include "UI/AlchemySlotWidget.h"
#include "InventoryDragDropOperation.h"
#include "Core/Types/HerbalistCoreMath.h"

bool UInventoryDragDropController::TryTransferItem(UHerbalistInventoryComponent* SourceInventory, int32 SourceIndex,
                                                   UHerbalistInventoryComponent* TargetInventory)
{
    if (!SourceInventory || !TargetInventory) return false;
    if (!SourceInventory->GetSlot(SourceIndex)) return false;

    return SourceInventory->TransferItemTo(SourceIndex, TargetInventory);
}

bool UInventoryDragDropController::TryAddSplitItem(const FInventoryItem& SplitItem,
                                                   UHerbalistInventoryComponent* SourceInventory,
                                                   UHerbalistInventoryComponent* TargetInventory,
                                                   UInventoryDragDropOperation* DragOp)
{
    if (!TargetInventory) return false;
    if (TargetInventory->AddItem(SplitItem, SplitItem.Count))
    {
        if (DragOp)
        {
            DragOp->bIsSplit = false;
        }
        return true;
    }
    return false;
}

bool UInventoryDragDropController::TryAddToAlchemySlot(const FInventoryItem& Item, UAlchemySlotWidget* Slot,
                                                       UHerbalistInventoryComponent* PlayerInventory)
{
    if (!Slot || !PlayerInventory) return false;
    if (!Slot->CanAcceptItem(Item)) return false;

    // Ищем индекс предмета в инвентаре
    const TArray<FInventoryItem>& Items = PlayerInventory->GetItems();
    int32 FoundIndex = INDEX_NONE;
    for (int32 i = 0; i < Items.Num(); ++i)
    {
        if (Items[i].Type == Item.Type &&
            HerbalistCore::Math::AreStatesSimilar(Items[i].State, Item.State))
        {
            FoundIndex = i;
            break;
        }
    }
    if (FoundIndex == INDEX_NONE) return false;

    // Добавляем в слот
    if (Slot->AddItem(Item, 1))
    {
        PlayerInventory->RemoveItem(FoundIndex, 1);
        return true;
    }
    return false;
}