// HerbalistHarvest.cpp
#include "HerbalistHarvest.h"
#include "ProjectHerbalist.h"
#include "Core/Data/ResourceDataManager.h"
#include "Core/Types/HerbalistItemData.h"

static constexpr float k_biome = 0.6f;
static constexpr float k_condition = 0.4f;

FRealState FHerbalistHarvest::GetBaseResourceParams(EResourceType Type)
{
    UResourceDataManager* Manager = UResourceDataManager::GetInstance();
    if (!Manager)
    {
        UE_LOG(LogHerbalist, Error, TEXT("ResourceDataManager not initialized!"));
        return FRealState();
    }

    // Получаем строку баланса по типу (нужно преобразовать EResourceType в PrimaryAssetId)
    // Упрощённо: предполагаем, что PrimaryAssetId совпадает с именем enum (Nettle, Fern...)
    FName AssetId = UEnum::GetValueAsName(Type); // "EResourceType::Nettle" -> "Nettle"
    const FResourceBalanceRow* BalanceRow = Manager->GetResourceBalanceRow(AssetId);
    if (!BalanceRow)
    {
        UE_LOG(LogHerbalist, Warning, TEXT("No balance row for resource type %d"), (int32)Type);
        return FRealState();
    }

    UHerbalistItemData* ItemData = Manager->GetItemData(AssetId);
    if (!ItemData)
    {
        UE_LOG(LogHerbalist, Warning, TEXT("No ItemData for asset %s"), *AssetId.ToString());
        return FRealState();
    }

    // Копируем базовое состояние из ассета
    FRealState State = ItemData->BaseState;
    State.Direction.NormalizeSum();
    return State;
}

FRealState FHerbalistHarvest::Harvest(EResourceType Type, const FRealState& BiomeState, const FConditionModifier& Conditions)
{
    FRealState Base = GetBaseResourceParams(Type);
    if (Base.Magnitude < 0.01f && Base.Meta.Distortion < 0.01f)
    {
        UE_LOG(LogHerbalist, Warning, TEXT("Harvest: invalid base params for type %d"), (int32)Type);
        return FRealState();
    }

    const FRealState& S0 = FAlatyr::S0;

    // Вычисляем дельту биома относительно эталона
    FRealState BiomeDelta;
    BiomeDelta.Direction.Body = BiomeState.Direction.Body - S0.Direction.Body;
    BiomeDelta.Direction.Mind = BiomeState.Direction.Mind - S0.Direction.Mind;
    BiomeDelta.Direction.Spirit = BiomeState.Direction.Spirit - S0.Direction.Spirit;
    BiomeDelta.Direction.Nature = BiomeState.Direction.Nature - S0.Direction.Nature;
    BiomeDelta.Magnitude = BiomeState.Magnitude - S0.Magnitude;
    BiomeDelta.Meta.Distortion = BiomeState.Meta.Distortion - S0.Meta.Distortion;
    BiomeDelta.Meta.Stability = BiomeState.Meta.Stability - S0.Meta.Stability;
    BiomeDelta.Meta.Purity = BiomeState.Meta.Purity - S0.Meta.Purity;
    BiomeDelta.Meta.Potency = BiomeState.Meta.Potency - S0.Meta.Potency;
    BiomeDelta.Meta.Resonance = BiomeState.Meta.Resonance - S0.Meta.Resonance;
    BiomeDelta.Meta.Corruption = BiomeState.Meta.Corruption - S0.Meta.Corruption;

    // Результат = база + влияние биома + влияние условий
    FRealState Result;
    Result.Direction.Body = Base.Direction.Body + k_biome * BiomeDelta.Direction.Body + k_condition * Conditions.DeltaDirection.Body;
    Result.Direction.Mind = Base.Direction.Mind + k_biome * BiomeDelta.Direction.Mind + k_condition * Conditions.DeltaDirection.Mind;
    Result.Direction.Spirit = Base.Direction.Spirit + k_biome * BiomeDelta.Direction.Spirit + k_condition * Conditions.DeltaDirection.Spirit;
    Result.Direction.Nature = Base.Direction.Nature + k_biome * BiomeDelta.Direction.Nature + k_condition * Conditions.DeltaDirection.Nature;
    Result.Magnitude = Base.Magnitude + k_biome * BiomeDelta.Magnitude + k_condition * Conditions.DeltaMagnitude;
    Result.Meta.Distortion = Base.Meta.Distortion + k_biome * BiomeDelta.Meta.Distortion + k_condition * Conditions.DeltaDistortion;
    Result.Meta.Stability = Base.Meta.Stability + k_biome * BiomeDelta.Meta.Stability + k_condition * Conditions.DeltaStability;
    Result.Meta.Purity = Base.Meta.Purity + k_biome * BiomeDelta.Meta.Purity + k_condition * Conditions.DeltaPurity;
    Result.Meta.Potency = Base.Meta.Potency + k_biome * BiomeDelta.Meta.Potency + k_condition * Conditions.DeltaPotency;
    Result.Meta.Resonance = Base.Meta.Resonance + k_biome * BiomeDelta.Meta.Resonance + k_condition * Conditions.DeltaResonance;
    Result.Meta.Corruption = Base.Meta.Corruption + k_biome * BiomeDelta.Meta.Corruption + k_condition * Conditions.DeltaCorruption;

    Result.Direction.NormalizeSum();
    Result.Magnitude = FMath::Clamp(Result.Magnitude, 0.0f, 1.0f);
    Result.Meta.Distortion = FMath::Clamp(Result.Meta.Distortion, 0.0f, 1.0f);
    Result.Meta.Stability = FMath::Clamp(Result.Meta.Stability, 0.0f, 1.0f);
    Result.Meta.Purity = FMath::Clamp(Result.Meta.Purity, 0.0f, 1.0f);
    Result.Meta.Potency = FMath::Clamp(Result.Meta.Potency, 0.0f, 1.0f);
    Result.Meta.Resonance = FMath::Clamp(Result.Meta.Resonance, 0.0f, 1.0f);
    Result.Meta.Corruption = FMath::Clamp(Result.Meta.Corruption, 0.0f, 1.0f);

    UE_LOG(LogHerbalist, Log, TEXT("[HARVEST] Type=%d Mag=%.3f Dist=%.3f Stab=%.3f Pur=%.3f"),
        (int32)Type, Result.Magnitude, Result.Meta.Distortion, Result.Meta.Stability, Result.Meta.Purity);
    return Result;
}

FString FHerbalistHarvest::GetResourceName(EResourceType Type, bool bEnglish)
{
    UResourceDataManager* Manager = UResourceDataManager::GetInstance();
    if (!Manager) return TEXT("Unknown");

    FName AssetId = UEnum::GetValueAsName(Type);
    UHerbalistItemData* ItemData = Manager->GetItemData(AssetId);
    if (!ItemData) return TEXT("Unknown");

    if (bEnglish)
        return AssetId.ToString();
    else
        return ItemData->DisplayName.ToString();
}