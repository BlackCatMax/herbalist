// Copyright Project Herbalist. All Rights Reserved.

#include "Core/Inventory/HerbalistInventoryComponent.h"
#include "ProjectHerbalist.h"

UHerbalistInventoryComponent::UHerbalistInventoryComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

bool UHerbalistInventoryComponent::AddItem(const FRealState& State, EResourceType Type)
{
    if (State.Magnitude < 0.01f || State.Meta.Distortion < 0.01f)
    {
        UE_LOG(LogHerbalist, Warning, TEXT("Rejected corrupted or empty resource: Mag=%.2f"), State.Magnitude);
        return false;
    }

    if (Items.Num() >= MaxSlots)
    {
        UE_LOG(LogHerbalist, Warning, TEXT("Inventory full, cannot add item"));
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
    if (Items.IsValidIndex(Index))
    {
        Items.RemoveAt(Index);
        UE_LOG(LogHerbalist, Log, TEXT("Removed from inventory, count=%d"), Items.Num());
        OnInventoryChanged.Broadcast();
    }
}

void UHerbalistInventoryComponent::Clear()
{
    Items.Empty();
    OnInventoryChanged.Broadcast();
}

void UHerbalistInventoryComponent::SwapItems(int32 IndexA, int32 IndexB)
{
    if (Items.IsValidIndex(IndexA) && Items.IsValidIndex(IndexB) && IndexA != IndexB)
    {
        Items.Swap(IndexA, IndexB);
        OnInventoryChanged.Broadcast();
    }
}