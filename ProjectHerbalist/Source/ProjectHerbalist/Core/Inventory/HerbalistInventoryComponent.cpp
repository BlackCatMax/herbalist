// Copyright Project Herbalist. All Rights Reserved.

#include "Core/Inventory/HerbalistInventoryComponent.h"
#include "ProjectHerbalist.h"

UHerbalistInventoryComponent::UHerbalistInventoryComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

bool UHerbalistInventoryComponent::AddItem(const FRealState& State, EResourceType Type)
{
    UE_LOG(LogHerbalist, Log, TEXT("AddItem called: Type=%d, Mag=%.2f, Dist=%.2f"), (int32)Type, State.Magnitude, State.Meta.Distortion);

    if (State.Magnitude < 0.01f || State.Meta.Distortion < 0.01f)
    {
        UE_LOG(LogHerbalist, Warning, TEXT("Rejected corrupted or empty resource: Mag=%.2f"), State.Magnitude);
        return false;
    }

    if (Items.Num() >= MaxSlots)
    {
        UE_LOG(LogHerbalist, Warning, TEXT("Inventory full (%d/%d), cannot add item"), Items.Num(), MaxSlots);
        return false;
    }

    FInventoryItem NewItem;
    NewItem.Type = Type;
    NewItem.State = State;
    Items.Add(NewItem);

    UE_LOG(LogHerbalist, Log, TEXT("Added to inventory, count=%d, Mag=%.2f, Type=%d"), Items.Num(), State.Magnitude, (int32)Type);
    OnInventoryChanged.Broadcast();
    return true;
}

void UHerbalistInventoryComponent::RemoveItem(int32 Index)
{
    UE_LOG(LogHerbalist, Log, TEXT("RemoveItem called: index %d, current count=%d"), Index, Items.Num());
    if (Items.IsValidIndex(Index))
    {
        FInventoryItem Removed = Items[Index];
        Items.RemoveAt(Index);
        UE_LOG(LogHerbalist, Log, TEXT("Removed from inventory, count=%d, removed type=%d"), Items.Num(), (int32)Removed.Type);
        OnInventoryChanged.Broadcast();
    }
    else
    {
        UE_LOG(LogHerbalist, Warning, TEXT("RemoveItem: invalid index %d"), Index);
    }
}

void UHerbalistInventoryComponent::Clear()
{
    UE_LOG(LogHerbalist, Log, TEXT("Clear inventory, %d items removed"), Items.Num());
    Items.Empty();
    OnInventoryChanged.Broadcast();
}

void UHerbalistInventoryComponent::SwapItems(int32 IndexA, int32 IndexB)
{
    if (Items.IsValidIndex(IndexA) && Items.IsValidIndex(IndexB) && IndexA != IndexB)
    {
        Items.Swap(IndexA, IndexB);
        UE_LOG(LogHerbalist, Log, TEXT("Swapped items %d and %d"), IndexA, IndexB);
        OnInventoryChanged.Broadcast();
    }
}