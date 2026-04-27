// HerbalistInventoryComponent.cpp
#include "HerbalistInventoryComponent.h"
#include "ProjectHerbalist.h"
#include "Core/Types/HerbalistCoreMath.h"
#include "Core/HerbalistSettings.h"
#include "Core/Subsystems/IngredientRegistrySubsystem.h"

UHerbalistInventoryComponent::UHerbalistInventoryComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickInterval = 0.2f;
}

void UHerbalistInventoryComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    TimeSinceLastDecayUpdate += DeltaTime;
    if (TimeSinceLastDecayUpdate < DecayUpdateInterval)
        return;

    TimeSinceLastDecayUpdate = 0.0f;

    const UHerbalistSettings* Settings = GetDefault<UHerbalistSettings>();
    const float GlobalDecayRate = Settings ? Settings->InventoryDecayRate : 0.02f;

    // Получаем реестр для индивидуальных множителей
    UIngredientRegistrySubsystem* IngredientReg = nullptr;
    if (UWorld* World = GetWorld())
    {
        if (UGameInstance* GI = World->GetGameInstance())
        {
            IngredientReg = GI->GetSubsystem<UIngredientRegistrySubsystem>();
        }
    }

    for (FInventoryItem& Item : Items)
    {
        if (Item.bSubjectToDecay)
        {
            float IngredientDecay = 1.0f;
            if (IngredientReg)
            {
                if (const FIngredientTableRow* Row = IngredientReg->GetRow(Item.IngredientID))
                {
                    IngredientDecay = Row->DecayRate;
                }
            }
            ApplyDecayToItem(Item, DecayUpdateInterval, GlobalDecayRate * IngredientDecay);
        }
    }
    // Не дёргаем OnInventoryChanged каждый тик, чтобы не спамить UI;
    // тултип обновится при следующем открытии инвентаря.
}

void UHerbalistInventoryComponent::ApplyDecayToItem(FInventoryItem& Item, float DeltaTime, float DecayRate)
{
    const float Instability = 1.0f - Item.State.Meta.Stability;
    const float DecayFactor = DecayRate * DeltaTime * Instability;

    Item.State.Meta.Distortion = FMath::Min(Item.State.Meta.Distortion + DecayFactor * 0.5f, 1.0f);
    Item.State.Meta.Corruption = FMath::Min(Item.State.Meta.Corruption + DecayFactor * 0.3f, 1.0f);
    Item.State.Meta.Purity      = FMath::Max(Item.State.Meta.Purity      - DecayFactor * 0.2f, 0.0f);
    Item.State.Meta.Stability   = FMath::Max(Item.State.Meta.Stability   - DecayFactor * 0.1f, 0.0f);

    // Лёгкое хаотичное смещение осей
    Item.State.Direction.Body   = FMath::Clamp(Item.State.Direction.Body   + FMath::FRandRange(-0.01f, 0.01f) * Instability, 0.0f, 1.0f);
    Item.State.Direction.Mind   = FMath::Clamp(Item.State.Direction.Mind   + FMath::FRandRange(-0.01f, 0.01f) * Instability, 0.0f, 1.0f);
    Item.State.Direction.Spirit = FMath::Clamp(Item.State.Direction.Spirit + FMath::FRandRange(-0.01f, 0.01f) * Instability, 0.0f, 1.0f);
    Item.State.Direction.Nature = FMath::Clamp(Item.State.Direction.Nature + FMath::FRandRange(-0.01f, 0.01f) * Instability, 0.0f, 1.0f);
    Item.State.Direction.NormalizeSum();
}

bool UHerbalistInventoryComponent::AddItem(const FInventoryItem& Item, int32 Amount)
{
    if (Amount <= 0 || Item.IngredientID.IsNone() || Item.Count <= 0)
    {
        UE_LOG(LogHerbalist, Warning, TEXT("AddItem: invalid parameters"));
        return false;
    }

    int32 Remaining = Amount;
    FInventoryItem TempItem = Item;

    while (Remaining > 0)
    {
        int32 SlotIndex = FindStackableSlot(TempItem);
        if (SlotIndex == INDEX_NONE) break;

        FInventoryItem& Slot = Items[SlotIndex];
        int32 Space = MAX_STACK_SIZE - Slot.Count;
        int32 ToAdd = FMath::Min(Remaining, Space);

        MergeStack(Slot, TempItem, ToAdd);
        Remaining -= ToAdd;
    }

    while (Remaining > 0)
    {
        if (Items.Num() >= MaxSlots)
        {
            UE_LOG(LogHerbalist, Warning, TEXT("AddItem: inventory full, %d items not added"), Remaining);
            break;
        }

        FInventoryItem NewSlot = TempItem;
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

    if (Target.IngredientID.IsNone() || Target.Count == 0)
    {
        Target = Source;
        Target.Count = 1;
        Source.Count -= 1;
        if (Source.Count <= 0)
            Items.RemoveAt(SourceIndex);
        OnInventoryChanged.Broadcast();
        return true;
    }

    if (Target.IngredientID == Source.IngredientID && Target.Count < MAX_STACK_SIZE && AreItemsStackable(Target, Source))
    {
        int32 Space = MAX_STACK_SIZE - Target.Count;
        int32 ToAdd = FMath::Min(1, Space);
        if (ToAdd > 0)
        {
            MergeStack(Target, Source, ToAdd);
            Source.Count -= ToAdd;
            if (Source.Count <= 0)
                Items.RemoveAt(SourceIndex);
            OnInventoryChanged.Broadcast();
            return true;
        }
    }

    return false;
}

bool UHerbalistInventoryComponent::TransferItemTo(int32 SourceIndex, UHerbalistInventoryComponent* TargetInventory)
{
    if (!TargetInventory || !Items.IsValidIndex(SourceIndex))
        return false;

    FInventoryItem& SourceItem = Items[SourceIndex];
    if (SourceItem.Count <= 0)
        return false;

    if (TargetInventory->AddItem(SourceItem, 1))
    {
        SourceItem.Count--;
        if (SourceItem.Count <= 0)
        {
            Items.RemoveAt(SourceIndex);
        }
        OnInventoryChanged.Broadcast();
        return true;
    }
    return false;
}

bool UHerbalistInventoryComponent::SplitStack(int32 Index, int32 Amount, FInventoryItem& OutItem)
{
    if (!Items.IsValidIndex(Index) || Amount <= 0)
        return false;

    FInventoryItem& Source = Items[Index];
    if (Source.Count <= Amount)
        return false;

    OutItem = Source;
    OutItem.Count = Amount;

    Source.Count -= Amount;
    OnInventoryChanged.Broadcast();
    return true;
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

int32 UHerbalistInventoryComponent::FindStackableSlot(const FInventoryItem& Item) const
{
    for (int32 i = 0; i < Items.Num(); ++i)
    {
        const FInventoryItem& Slot = Items[i];
        if (Slot.IngredientID == Item.IngredientID && Slot.Count < MAX_STACK_SIZE && AreItemsStackable(Slot, Item))
            return i;
    }
    return INDEX_NONE;
}

bool UHerbalistInventoryComponent::AreItemsStackable(const FInventoryItem& A, const FInventoryItem& B) const
{
    if (A.IngredientID != B.IngredientID) return false;
    return HerbalistCore::Math::AreStatesSimilar(A.State, B.State);
}

void UHerbalistInventoryComponent::MergeStack(FInventoryItem& Target, const FInventoryItem& Source, int32 AddedCount)
{
    int32 NewCount = Target.Count + AddedCount;
    float OldWeight = (float)Target.Count / NewCount;
    float NewWeight = (float)AddedCount / NewCount;

    FRealState& T = Target.State;
    const FRealState& S = Source.State;

    T.Magnitude = T.Magnitude * OldWeight + S.Magnitude * NewWeight;

    T.Direction.Body = T.Direction.Body * OldWeight + S.Direction.Body * NewWeight;
    T.Direction.Mind = T.Direction.Mind * OldWeight + S.Direction.Mind * NewWeight;
    T.Direction.Spirit = T.Direction.Spirit * OldWeight + S.Direction.Spirit * NewWeight;
    T.Direction.Nature = T.Direction.Nature * OldWeight + S.Direction.Nature * NewWeight;
    T.Direction.NormalizeSum();

    T.Meta.Distortion = T.Meta.Distortion * OldWeight + S.Meta.Distortion * NewWeight;
    T.Meta.Stability = T.Meta.Stability * OldWeight + S.Meta.Stability * NewWeight;
    T.Meta.Purity = T.Meta.Purity * OldWeight + S.Meta.Purity * NewWeight;
    T.Meta.Potency = T.Meta.Potency * OldWeight + S.Meta.Potency * NewWeight;
    T.Meta.Resonance = T.Meta.Resonance * OldWeight + S.Meta.Resonance * NewWeight;
    T.Meta.Corruption = T.Meta.Corruption * OldWeight + S.Meta.Corruption * NewWeight;

    float DistortionDiff = FMath::Abs(T.Meta.Distortion - S.Meta.Distortion);
    T.Meta.Distortion = FMath::Clamp(T.Meta.Distortion + DistortionDiff * 0.15f, 0.0f, 1.0f);

    float PurityDiff = FMath::Abs(T.Meta.Purity - S.Meta.Purity);
    T.Meta.Purity = FMath::Clamp(T.Meta.Purity - PurityDiff * 0.1f, 0.0f, 1.0f);

    // Усредняем CreationTime
    Target.CreationTime = Target.CreationTime * OldWeight + Source.CreationTime * NewWeight;

    // bSubjectToDecay остаётся true, если оба true; иначе false (если хоть один – зелье)
    Target.bSubjectToDecay = Target.bSubjectToDecay && Source.bSubjectToDecay;

    Target.Count = NewCount;
}