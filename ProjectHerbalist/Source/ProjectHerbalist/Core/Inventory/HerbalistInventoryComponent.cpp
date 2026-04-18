// HerbalistInventoryComponent.cpp
#include "HerbalistInventoryComponent.h"
#include "ProjectHerbalist.h"

UHerbalistInventoryComponent::UHerbalistInventoryComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

bool UHerbalistInventoryComponent::AddItem(const FInventoryItem& Item, int32 Amount)
{
    if (Amount <= 0 || Item.Type == EResourceType::None || Item.Count <= 0)
    {
        UE_LOG(LogHerbalist, Warning, TEXT("AddItem: invalid parameters"));
        return false;
    }

    int32 Remaining = Amount;

    // 1. Пытаемся дополнить существующие стаки того же типа
    while (Remaining > 0)
    {
        int32 SlotIndex = FindStackableSlot(Item.Type);
        if (SlotIndex == INDEX_NONE) break;

        FInventoryItem& Slot = Items[SlotIndex];
        int32 Space = MAX_STACK_SIZE - Slot.Count;
        int32 ToAdd = FMath::Min(Remaining, Space);
        Slot.Count += ToAdd;
        Remaining -= ToAdd;
    }

    // 2. Если остались предметы, создаём новые слоты
    while (Remaining > 0)
    {
        if (Items.Num() >= MaxSlots)
        {
            UE_LOG(LogHerbalist, Warning, TEXT("AddItem: inventory full, %d items not added"), Remaining);
            break;
        }

        FInventoryItem NewSlot = Item;
        NewSlot.Count = FMath::Min(Remaining, MAX_STACK_SIZE);
        Items.Add(NewSlot);
        Remaining -= NewSlot.Count;
    }

    if (Amount != Remaining)
    {
        OnInventoryChanged.Broadcast();
        return true;
    }
    return false;
}

bool UHerbalistInventoryComponent::RemoveItem(int32 Index, int32 Amount)
{
    if (!Items.IsValidIndex(Index) || Amount <= 0) return false;

    FInventoryItem& Slot = Items[Index];
    if (Slot.Count < Amount) return false;

    Slot.Count -= Amount;
    if (Slot.Count <= 0)
    {
        Items.RemoveAt(Index);
    }

    OnInventoryChanged.Broadcast();
    return true;
}

bool UHerbalistInventoryComponent::TransferOneItem(int32 SourceIndex, int32 TargetIndex)
{
    if (!Items.IsValidIndex(SourceIndex) || !Items.IsValidIndex(TargetIndex)) return false;

    FInventoryItem& Source = Items[SourceIndex];
    FInventoryItem& Target = Items[TargetIndex];

    if (Source.Count <= 0) return false;

    // Если целевой слот пуст
    if (Target.Type == EResourceType::None || Target.Count == 0)
    {
        Target = Source;
        Target.Count = 1;
        Source.Count -= 1;
        if (Source.Count <= 0)
            Items.RemoveAt(SourceIndex);
        OnInventoryChanged.Broadcast();
        return true;
    }

    // Если типы совпадают и в целевом слоте есть место
    if (Target.Type == Source.Type && Target.Count < MAX_STACK_SIZE)
    {
        Target.Count += 1;
        Source.Count -= 1;
        if (Source.Count <= 0)
            Items.RemoveAt(SourceIndex);
        OnInventoryChanged.Broadcast();
        return true;
    }

    return false;
}

const FInventoryItem* UHerbalistInventoryComponent::GetSlot(int32 Index) const
{
    return Items.IsValidIndex(Index) ? &Items[Index] : nullptr;
}

void UHerbalistInventoryComponent::Clear()
{
    Items.Empty();
    OnInventoryChanged.Broadcast();
}

int32 UHerbalistInventoryComponent::FindStackableSlot(EResourceType Type) const
{
    for (int32 i = 0; i < Items.Num(); ++i)
    {
        if (Items[i].Type == Type && Items[i].Count < MAX_STACK_SIZE)
            return i;
    }
    return INDEX_NONE;
}