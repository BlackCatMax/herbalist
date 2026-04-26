// HarvestService.cpp
#include "HarvestService.h"
#include "ProjectHerbalist.h"
#include "Core/Data/IngredientRegistry.h"
#include "Core/Data/WaterTypeRegistry.h"
#include "Core/Types/HerbalistIngredient.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/AssetManager.h"

static constexpr float k_biome = 0.6f;
static constexpr float k_condition = 0.4f;

UHerbalistIngredient* UHarvestService::LoadIngredientAsset(FName IngredientID) const
{
    if (IngredientID.IsNone()) return nullptr;
    FString DirectPath = FString::Printf(TEXT("/Game/Data/Ingredients/%s.%s"), *IngredientID.ToString(), *IngredientID.ToString());
    return LoadObject<UHerbalistIngredient>(nullptr, *DirectPath);
}

FRealState UHarvestService::GetBaseResourceParams(FName IngredientID) const
{
    const FIngredientTableRow* Row = FIngredientRegistry::GetRow(IngredientID);
    if (!Row)
    {
        UE_LOG(LogHerbalist, Warning, TEXT("GetBaseResourceParams: Ingredient not found in registry '%s'"), *IngredientID.ToString());
        return FRealState();
    }
    FRealState State = Row->BaseState;
    State.Direction.NormalizeSum();
    return State;
}

FRealState UHarvestService::Harvest(FName IngredientID, const FRealState& BiomeState, const FConditionModifier& Conditions) const
{
    // ... (без изменений, как в исходном коде)
    const FIngredientTableRow* Row = FIngredientRegistry::GetRow(IngredientID);
    if (!Row)
    {
        UE_LOG(LogHerbalist, Warning, TEXT("Harvest: Ingredient not found in registry '%s'"), *IngredientID.ToString());
        return FRealState();
    }
    FRealState Base = Row->BaseState;
    if (Base.Magnitude < 0.01f && Base.Meta.Distortion < 0.01f)
    {
        UE_LOG(LogHerbalist, Warning, TEXT("Harvest: invalid base params for '%s', aborting"), *IngredientID.ToString());
        return FRealState();
    }

    const FRealState& S0 = FAlatyr::S0;

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

    FRealState Result;
    Result.Direction.Body = Base.Direction.Body + k_biome * BiomeDelta.Direction.Body + k_condition * Conditions.DeltaDirection.Body;
    Result.Direction.Mind = Base.Direction.Mind + k_biome * BiomeDelta.Direction.Mind + k_condition * Conditions.DeltaDirection.Mind;
    Result.Direction.Spirit = Base.Direction.Spirit + k_biome * BiomeDelta.Direction.Spirit + k_condition * Conditions.DeltaDirection.Spirit;
    Result.Direction.Nature = Base.Direction.Nature + k_biome * BiomeDelta.Direction.Nature + k_condition * Conditions.DeltaDirection.Nature;
    Result.Magnitude = Base.Magnitude + k_biome * BiomeDelta.Magnitude + k_condition * Conditions.DeltaMagnitude;
    Result.Meta.Stability = Base.Meta.Stability + k_biome * BiomeDelta.Meta.Stability + k_condition * Conditions.DeltaStability;
    Result.Meta.Purity = Base.Meta.Purity + k_biome * BiomeDelta.Meta.Purity + k_condition * Conditions.DeltaPurity;
    Result.Meta.Potency = Base.Meta.Potency + k_biome * BiomeDelta.Meta.Potency + k_condition * Conditions.DeltaPotency;
    Result.Meta.Resonance = Base.Meta.Resonance + k_biome * BiomeDelta.Meta.Resonance + k_condition * Conditions.DeltaResonance;
    Result.Meta.Corruption = Base.Meta.Corruption + k_biome * BiomeDelta.Meta.Corruption + k_condition * Conditions.DeltaCorruption;

    float BaseDist = Base.Meta.Distortion;
    float BiomeDist = BiomeState.Meta.Distortion;
    float CondDist  = Conditions.DeltaDistortion;
    float P_Base  = 1.0f - BaseDist;
    float P_Biome = 1.0f - BiomeDist * k_biome;
    float P_Cond  = 1.0f - FMath::Clamp(CondDist * k_condition, -1.0f, 1.0f);
    float CombinedPurity = P_Base * P_Biome * P_Cond;
    Result.Meta.Distortion = 1.0f - FMath::Clamp(CombinedPurity, 0.0f, 1.0f);

    Result.Direction.NormalizeSum();
    Result.Magnitude = FMath::Clamp(Result.Magnitude, 0.0f, 1.0f);
    Result.Meta.Distortion = FMath::Clamp(Result.Meta.Distortion, 0.0f, 1.0f);
    Result.Meta.Stability = FMath::Clamp(Result.Meta.Stability, 0.0f, 1.0f);
    Result.Meta.Purity = FMath::Clamp(Result.Meta.Purity, 0.0f, 1.0f);
    Result.Meta.Potency = FMath::Clamp(Result.Meta.Potency, 0.0f, 1.0f);
    Result.Meta.Resonance = FMath::Clamp(Result.Meta.Resonance, 0.0f, 1.0f);
    Result.Meta.Corruption = FMath::Clamp(Result.Meta.Corruption, 0.0f, 1.0f);

    UE_LOG(LogHerbalist, Log, TEXT("[HARVEST] ID=%s Mag=%.3f Dist=%.3f Stab=%.3f Pur=%.3f"),
        *IngredientID.ToString(), Result.Magnitude, Result.Meta.Distortion, Result.Meta.Stability, Result.Meta.Purity);
    return Result;
}

FRealState UHarvestService::HarvestWater(const FGridCell& Cell, const FConditionModifier& Conditions) const
{
    // Базовая вода – состояние клетки
    FRealState Water = Cell.State;

    // Применяем специальные параметры из WaterTypeRegistry, если они есть
    if (!Cell.WaterTypeID.IsNone())
    {
        if (const FWaterTypeRow* WaterRow = FWaterTypeRegistry::GetWaterType(Cell.WaterTypeID))
        {
            Water.Meta.Purity = WaterRow->BasePurity;
            Water.Meta.Distortion = WaterRow->BaseDistortion;
            Water.Meta.Stability = WaterRow->BaseStability;
            Water.Meta.Potency = WaterRow->BasePotency;
            Water.Meta.Corruption = WaterRow->BaseCorruption;
            // Направление и магнитуда могут быть оставлены от биома
        }
    }

    // Добавляем влияние условий
    Water.Magnitude += k_condition * Conditions.DeltaMagnitude;
    Water.Direction.Body += k_condition * Conditions.DeltaDirection.Body;
    Water.Direction.Mind += k_condition * Conditions.DeltaDirection.Mind;
    Water.Direction.Spirit += k_condition * Conditions.DeltaDirection.Spirit;
    Water.Direction.Nature += k_condition * Conditions.DeltaDirection.Nature;
    Water.Meta.Distortion += k_condition * Conditions.DeltaDistortion;
    Water.Meta.Stability += k_condition * Conditions.DeltaStability;
    Water.Meta.Purity += k_condition * Conditions.DeltaPurity;
    Water.Meta.Potency += k_condition * Conditions.DeltaPotency;
    Water.Meta.Resonance += k_condition * Conditions.DeltaResonance;
    Water.Meta.Corruption += k_condition * Conditions.DeltaCorruption;

    Water.Direction.NormalizeSum();
    Water.Magnitude = FMath::Clamp(Water.Magnitude, 0.0f, 1.0f);
    Water.Meta.Distortion = FMath::Clamp(Water.Meta.Distortion, 0.0f, 1.0f);
    Water.Meta.Stability = FMath::Clamp(Water.Meta.Stability, 0.0f, 1.0f);
    Water.Meta.Purity = FMath::Clamp(Water.Meta.Purity, 0.0f, 1.0f);
    Water.Meta.Potency = FMath::Clamp(Water.Meta.Potency, 0.0f, 1.0f);
    Water.Meta.Resonance = FMath::Clamp(Water.Meta.Resonance, 0.0f, 1.0f);
    Water.Meta.Corruption = FMath::Clamp(Water.Meta.Corruption, 0.0f, 1.0f);
    return Water;
}

UHerbalistIngredient* UHarvestService::LoadIngredientAssetStatic(FName IngredientID)
{
    return nullptr;
}