// Core/World/GridWorldManagerCommunity.cpp
//
// Общинный кластер (DESIGN_Community_And_Homestead.md §1,
// 17_Hero_And_Community.md §17.3) — реализация 2026-08-31. Молва растёт тем
// же знаковым приёмом, что уже даёт Restoration капищ и Respect хозяев места
// (Gain × (Purity − Corruption)), см. RunSimulationStep (GridWorldManagerTick
// .cpp) — но, в отличие от них, не привязана к клетке: община — не место,
// не проходит через Delta.WorldChanges/детерминированный пайплайн вовсе, тем
// же принципом, что UpdateShrines/UpdateMemoryFragments уже внепайплайновые.

#include "Core/World/GridWorldManager.h"
#include "Core/Config/HerbalistSettings.h"
#include "Core/Subsystems/IngredientRegistrySubsystem.h"
#include "Core/Data/IngredientTableRow.h"
#include "ProjectHerbalist.h"
#include "HerbalistLogChannels.h"

float AGridWorldManager::OfferToCommunity(const TArray<FInventoryItem>& Items)
{
    if (Items.Num() == 0) return 0.0f;

    float SumPurity = 0.0f, SumCorruption = 0.0f;
    for (const FInventoryItem& Item : Items)
    {
        SumPurity += Item.State.Meta.Purity;
        SumCorruption += Item.State.Meta.Corruption;
    }
    const float AvgPurity = SumPurity / Items.Num();
    const float AvgCorruption = SumCorruption / Items.Num();

    const UHerbalistSettings* Settings = GetHerbalistSettings();
    const float Gain = Settings ? Settings->MolvaOfferingGain : 0.03f;
    const float DeltaMolva = Gain * (AvgPurity - AvgCorruption);

    Molva = FMath::Clamp(Molva + DeltaMolva, -1.0f, 1.0f);
    UE_LOG(LogHerbalistWorld, Log, TEXT("[Community] Offered %d item(s), ΔMolva=%.4f, Molva=%.3f"),
        Items.Num(), DeltaMolva, Molva);
    return DeltaMolva;
}

float AGridWorldManager::ComputeCommunityTradeValue(const FInventoryItem& Item) const
{
    UGameInstance* GameInstance = GetGameInstance();
    UIngredientRegistrySubsystem* IngredientSubsystem = GameInstance ? GameInstance->GetSubsystem<UIngredientRegistrySubsystem>() : nullptr;
    const FIngredientTableRow* Row = IngredientSubsystem ? IngredientSubsystem->GetRow(Item.IngredientID) : nullptr;
    if (!Row) return 0.0f;

    const UHerbalistSettings* Settings = GetHerbalistSettings();
    const float PurityWeight = Settings ? Settings->TradeValuePurityWeight : 0.5f;
    const float RarityWeight = Settings ? Settings->TradeValueRarityWeight : 0.5f;

    // RarityWeight ингредиента >= 1 (см. ClampMin в IngredientTableRow.h) —
    // 1/RarityWeight в (0, 1], редкие (низкий вес спавна) ближе к 1, частые
    // тянут множитель к нулю.
    const float InverseRarity = Row->RarityWeight > 0 ? 1.0f / static_cast<float>(Row->RarityWeight) : 0.0f;

    const float Value = Item.State.Magnitude
        * (1.0f + PurityWeight * Item.State.Meta.Purity)
        * (1.0f + RarityWeight * InverseRarity);
    return FMath::Max(0.0f, Value) * FMath::Max(1, Item.Count);
}

bool AGridWorldManager::TryTradeWithCommunity(const FInventoryItem& Offered, FName WantedIngredientID, FInventoryItem& OutReceived) const
{
    if (Offered.Count <= 0) return false;

    UGameInstance* GameInstance = GetGameInstance();
    UIngredientRegistrySubsystem* IngredientSubsystem = GameInstance ? GameInstance->GetSubsystem<UIngredientRegistrySubsystem>() : nullptr;
    const FIngredientTableRow* WantedRow = IngredientSubsystem ? IngredientSubsystem->GetRow(WantedIngredientID) : nullptr;
    if (!WantedRow) return false;

    const float OfferedValue = ComputeCommunityTradeValue(Offered);
    if (OfferedValue <= 0.0f) return false;

    // Единица желаемого — её собственный BaseState, Count=1, тот же вызов
    // ComputeCommunityTradeValue, что и для предложенного, курс без двойной
    // бухгалтерии.
    FInventoryItem WantedUnit;
    WantedUnit.IngredientID = WantedIngredientID;
    WantedUnit.State = WantedRow->BaseState;
    WantedUnit.Count = 1;
    const float WantedUnitValue = ComputeCommunityTradeValue(WantedUnit);
    if (WantedUnitValue <= 0.0f) return false;

    const UHerbalistSettings* Settings = GetHerbalistSettings();
    const float MolvaBonus = Settings ? Settings->TradeMolvaRateBonus : 0.3f;
    // Молва двигает курс, не значение по отдельности — тот же язык, что уже
    // в §1.2 документа ("выше Molva -> выгоднее курс").
    const float Rate = (OfferedValue / WantedUnitValue) * (1.0f + MolvaBonus * Molva);

    OutReceived = WantedUnit;
    OutReceived.Count = FMath::Max(1, FMath::FloorToInt(Rate));
    UE_LOG(LogHerbalistWorld, Log, TEXT("[Community] Trade %s(x%d) -> %s(x%d), rate=%.3f, Molva=%.3f"),
        *Offered.IngredientID.ToString(), Offered.Count, *WantedIngredientID.ToString(), OutReceived.Count, Rate, Molva);
    return true;
}
