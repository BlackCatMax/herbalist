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

namespace
{
    // Взвешенное по количеству единиц среднее FRealState — ПРОСТОЕ, без
    // "разброса от смешивания" HerbalistCore::Math::BlendRealStatesForStack:
    // тот приём смоделирован для физического смешения трав в одном стеке/
    // котле (см. довод там же), а не для абстрактной статистики "какого
    // качества сейчас община в среднем видит этот ингредиент" — лишняя
    // драма здесь не нужна, только честное среднее. Локально здесь, не в
    // HerbalistCoreMath.h — пока единственный потребитель.
    void AverageRealStateByCount(FRealState& Target, const FRealState& Source, int32 TargetCount, int32 AddedCount)
    {
        const int32 NewCount = TargetCount + AddedCount;
        if (NewCount <= 0) return;
        const float OldWeight = (float)TargetCount / NewCount;
        const float NewWeight = (float)AddedCount / NewCount;

        Target.Magnitude = Target.Magnitude * OldWeight + Source.Magnitude * NewWeight;

        Target.Direction.Body   = Target.Direction.Body   * OldWeight + Source.Direction.Body   * NewWeight;
        Target.Direction.Mind   = Target.Direction.Mind   * OldWeight + Source.Direction.Mind   * NewWeight;
        Target.Direction.Spirit = Target.Direction.Spirit * OldWeight + Source.Direction.Spirit * NewWeight;
        Target.Direction.Nature = Target.Direction.Nature * OldWeight + Source.Direction.Nature * NewWeight;
        Target.Direction.NormalizeSum();

        Target.Meta.Distortion = Target.Meta.Distortion * OldWeight + Source.Meta.Distortion * NewWeight;
        Target.Meta.Stability  = Target.Meta.Stability  * OldWeight + Source.Meta.Stability  * NewWeight;
        Target.Meta.Purity     = Target.Meta.Purity     * OldWeight + Source.Meta.Purity     * NewWeight;
        Target.Meta.Potency    = Target.Meta.Potency    * OldWeight + Source.Meta.Potency    * NewWeight;
        Target.Meta.Resonance  = Target.Meta.Resonance  * OldWeight + Source.Meta.Resonance  * NewWeight;
        Target.Meta.Corruption = Target.Meta.Corruption * OldWeight + Source.Meta.Corruption * NewWeight;
    }
}

void AGridWorldManager::RecordCommunityIngredientQuality(FName IngredientID, const FRealState& State, int32 Count)
{
    if (Count <= 0 || IngredientID.IsNone()) return;

    int32& SampleCount = CommunityIngredientSampleCount.FindOrAdd(IngredientID, 0);
    if (SampleCount == 0)
    {
        // Первый экземпляр этого вида, увиденный общиной — задаёт стартовое
        // среднее целиком, усреднять пока не с чем.
        CommunityIngredientQuality.Add(IngredientID, State);
    }
    else
    {
        AverageRealStateByCount(CommunityIngredientQuality.FindOrAdd(IngredientID), State, SampleCount, Count);
    }
    SampleCount += Count;
}

float AGridWorldManager::OfferToCommunity(const TArray<FInventoryItem>& Items)
{
    if (Items.Num() == 0) return 0.0f;

    // Сумма, не среднее (аудит 2026-09-05): вызывающая сторона
    // (AHerbalistPlayerController::OfferToCommunity) списывает ровно 1
    // единицу за КАЖДЫЙ найденный слот массива Items -- значит и вклад в
    // Molva обязан расти с числом слотов, иначе поднести 5 разных слотов
    // одной травы давало бы то же ΔMolva, что поднести 1, при этом реально
    // потеряв впятеро больше. Для типового случая "один слот за раз"
    // (Items.Num()==1) поведение не меняется -- сумма одного слагаемого
    // равна среднему по одному слагаемому, калибровку MolvaOfferingGain
    // трогать не пришлось.
    float SumPurity = 0.0f, SumCorruption = 0.0f;
    for (const FInventoryItem& Item : Items)
    {
        SumPurity += Item.State.Meta.Purity;
        SumCorruption += Item.State.Meta.Corruption;

        // Аудит 2026-09-05 (решение пользователя): каждый слот здесь даёт
        // ровно 1 реально отданную единицу (вызывающая сторона списывает
        // RemoveItem(Index, 1) за каждый найденный слот, см. довод выше про
        // Count.Num() vs сумму) — Count=1, не Item.Count.
        RecordCommunityIngredientQuality(Item.IngredientID, Item.State, 1);
    }

    const UHerbalistSettings* Settings = GetHerbalistSettings();
    const float Gain = Settings ? Settings->MolvaOfferingGain : 0.03f;
    const float DeltaMolva = Gain * (SumPurity - SumCorruption);

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

    // Единица желаемого — Count=1, тот же вызов ComputeCommunityTradeValue,
    // что и для предложенного, курс без двойной бухгалтерии. State — аудит
    // 2026-09-05, решение пользователя: раньше здесь всегда стоял чистый
    // табличный BaseState, а предложенный игроком предмет оценивался по
    // РЕАЛЬНОМУ State (с бонусами сушки/отстоя/выпаривания) — асимметрия,
    // позволявшая постепенно "отмывать" качество через округление при
    // повторных обменах туда-обратно. Теперь желаемое оценивается по тому,
    // что община РЕАЛЬНО видела (CommunityIngredientQuality, взвешенное
    // среднее реально полученных единиц — см. RecordCommunityIngredientQuality) —
    // честный фолбэк на BaseState, пока община ни разу не получала этот вид.
    FInventoryItem WantedUnit;
    WantedUnit.IngredientID = WantedIngredientID;
    const FRealState* KnownQuality = CommunityIngredientQuality.Find(WantedIngredientID);
    WantedUnit.State = KnownQuality ? *KnownQuality : WantedRow->BaseState;
    WantedUnit.Count = 1;
    const float WantedUnitValue = ComputeCommunityTradeValue(WantedUnit);
    if (WantedUnitValue <= 0.0f) return false;

    const UHerbalistSettings* Settings = GetHerbalistSettings();
    const float MolvaBonus = Settings ? Settings->TradeMolvaRateBonus : 0.3f;
    // Молва двигает курс, не значение по отдельности — тот же язык, что уже
    // в §1.2 документа ("выше Molva -> выгоднее курс").
    const float Rate = (OfferedValue / WantedUnitValue) * (1.0f + MolvaBonus * Molva);

    int32 ReceivedCount = 0;
    if (!ComputeTradeReceivedCount(Rate, ReceivedCount))
    {
        UE_LOG(LogHerbalistWorld, Log, TEXT("[Community] Trade %s -> %s refused: rate %.4f too low for even 1 unit"),
            *Offered.IngredientID.ToString(), *WantedIngredientID.ToString(), Rate);
        return false;
    }

    OutReceived = WantedUnit;
    OutReceived.Count = ReceivedCount;
    UE_LOG(LogHerbalistWorld, Log, TEXT("[Community] Trade %s(x%d) -> %s(x%d), rate=%.3f, Molva=%.3f"),
        *Offered.IngredientID.ToString(), Offered.Count, *WantedIngredientID.ToString(), OutReceived.Count, Rate, Molva);
    return true;
}

bool AGridWorldManager::ComputeTradeReceivedCount(float Rate, int32& OutCount)
{
    OutCount = FMath::FloorToInt(Rate);
    return OutCount >= 1;
}
