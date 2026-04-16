// HerbalistHarvest.cpp
#include "HerbalistHarvest.h"
#include "ProjectHerbalist.h"
#include "Core/Types/HerbalistCoreTypes.h"
#include "Math/UnrealMathUtility.h"

static constexpr float k_biome = 0.6f;
static constexpr float k_condition = 0.4f;

FRealState FHerbalistHarvest::GetBaseResourceParams(EResourceType Type)
{
    FRealState R;
    switch (Type)
    {
    case EResourceType::Nettle:
        R.Magnitude = 0.55f;
        R.Direction.Body = 0.50f; R.Direction.Mind = 0.30f; R.Direction.Spirit = 0.40f; R.Direction.Nature = 0.80f;
        R.Meta.Distortion = 0.30f; R.Meta.Stability = 0.50f; R.Meta.Purity = 0.60f;
        R.Meta.Potency = 0.50f; R.Meta.Resonance = 0.60f; R.Meta.Corruption = 0.30f;
        break;
    case EResourceType::Fern:
        R.Magnitude = 0.65f;
        R.Direction.Body = 0.30f; R.Direction.Mind = 0.30f; R.Direction.Spirit = 0.70f; R.Direction.Nature = 0.70f;
        R.Meta.Distortion = 0.60f; R.Meta.Stability = 0.35f; R.Meta.Purity = 0.40f;
        R.Meta.Potency = 0.70f; R.Meta.Resonance = 0.80f; R.Meta.Corruption = 0.50f;
        break;
    case EResourceType::Mushroom:
        R.Magnitude = 0.70f;
        R.Direction.Body = 0.40f; R.Direction.Mind = 0.80f; R.Direction.Spirit = 0.60f; R.Direction.Nature = 0.20f;
        R.Meta.Distortion = 0.85f; R.Meta.Stability = 0.25f; R.Meta.Purity = 0.30f;
        R.Meta.Potency = 0.75f; R.Meta.Resonance = 0.85f; R.Meta.Corruption = 0.80f;
        break;
    case EResourceType::BirchBark:
        R.Magnitude = 0.50f;
        R.Direction.Body = 0.60f; R.Direction.Mind = 0.50f; R.Direction.Spirit = 0.40f; R.Direction.Nature = 0.50f;
        R.Meta.Distortion = 0.25f; R.Meta.Stability = 0.65f; R.Meta.Purity = 0.70f;
        R.Meta.Potency = 0.45f; R.Meta.Resonance = 0.50f; R.Meta.Corruption = 0.20f;
        break;
    case EResourceType::Moss:
        R.Magnitude = 0.50f;
        R.Direction.Body = 0.30f; R.Direction.Mind = 0.30f; R.Direction.Spirit = 0.60f; R.Direction.Nature = 0.80f;
        R.Meta.Distortion = 0.35f; R.Meta.Stability = 0.55f; R.Meta.Purity = 0.65f;
        R.Meta.Potency = 0.50f; R.Meta.Resonance = 0.65f; R.Meta.Corruption = 0.30f;
        break;
    case EResourceType::Cranberry:
        R.Magnitude = 0.55f;
        R.Direction.Body = 0.40f; R.Direction.Mind = 0.30f; R.Direction.Spirit = 0.60f; R.Direction.Nature = 0.70f;
        R.Meta.Distortion = 0.50f; R.Meta.Stability = 0.45f; R.Meta.Purity = 0.60f;
        R.Meta.Potency = 0.50f; R.Meta.Resonance = 0.65f; R.Meta.Corruption = 0.45f;
        break;
    case EResourceType::BogOre:
        R.Magnitude = 0.60f;
        R.Direction.Body = 0.80f; R.Direction.Mind = 0.20f; R.Direction.Spirit = 0.30f; R.Direction.Nature = 0.70f;
        R.Meta.Distortion = 0.75f; R.Meta.Stability = 0.40f; R.Meta.Purity = 0.35f;
        R.Meta.Potency = 0.55f; R.Meta.Resonance = 0.65f; R.Meta.Corruption = 0.70f;
        break;
    default:
        R.Magnitude = 0.5f;
        R.Direction.Body = R.Direction.Mind = R.Direction.Spirit = R.Direction.Nature = 0.5f;
        R.Meta.Distortion = 0.0f; R.Meta.Stability = 0.5f; R.Meta.Purity = 0.5f;
        R.Meta.Potency = 0.5f; R.Meta.Resonance = 0.5f; R.Meta.Corruption = 0.5f;
        break;
    }
    R.Direction.NormalizeSum();
    R.Meta.Distortion = FMath::Clamp(R.Meta.Distortion, 0.0f, 1.0f);
    R.Meta.Stability = FMath::Clamp(R.Meta.Stability, 0.0f, 1.0f);
    R.Meta.Purity = FMath::Clamp(R.Meta.Purity, 0.0f, 1.0f);
    R.Meta.Potency = FMath::Clamp(R.Meta.Potency, 0.0f, 1.0f);
    R.Meta.Resonance = FMath::Clamp(R.Meta.Resonance, 0.0f, 1.0f);
    R.Meta.Corruption = FMath::Clamp(R.Meta.Corruption, 0.0f, 1.0f);
    R.Magnitude = FMath::Clamp(R.Magnitude, 0.0f, 1.0f);
    return R;
}

FRealState FHerbalistHarvest::Harvest(EResourceType Type, const FRealState& BiomeState, const FConditionModifier& Conditions)
{
    FRealState Base = GetBaseResourceParams(Type);
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

    UE_LOG(LogHerbalist, Log, TEXT("[HARVEST] Type=%d Mag=%.3f Dist=%.3f Stab=%.3f Pur=%.3f Pot=%.3f Res=%.3f Cor=%.3f"),
        (int32)Type, Result.Magnitude, Result.Meta.Distortion, Result.Meta.Stability, Result.Meta.Purity,
        Result.Meta.Potency, Result.Meta.Resonance, Result.Meta.Corruption);
    return Result;
}

FString FHerbalistHarvest::GetResourceName(EResourceType Type, bool bEnglish)
{
    switch (Type)
    {
    case EResourceType::Nettle:      return bEnglish ? TEXT("Nettle") : TEXT("Крапива");
    case EResourceType::Fern:        return bEnglish ? TEXT("Fern") : TEXT("Папоротник");
    case EResourceType::Mushroom:    return bEnglish ? TEXT("Mushroom") : TEXT("Мухомор");
    case EResourceType::BirchBark:   return bEnglish ? TEXT("Birch Bark") : TEXT("Кора берёзы");
    case EResourceType::Moss:        return bEnglish ? TEXT("Moss") : TEXT("Мох");
    case EResourceType::Cranberry:   return bEnglish ? TEXT("Cranberry") : TEXT("Клюква");
    case EResourceType::BogOre:      return bEnglish ? TEXT("Bog Ore") : TEXT("Болотная руда");
    default: return TEXT("Unknown");
    }
}