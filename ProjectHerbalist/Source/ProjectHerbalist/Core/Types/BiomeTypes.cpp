// BiomeTypes.cpp
#include "BiomeTypes.h"
#include "Math/UnrealMathUtility.h"

FRealState FBiomeDefaults::GetDefaultState(EBiomeType Biome)
{
    FRealState S;
    switch (Biome)
    {
    case EBiomeType::MixedForest:
        S.Magnitude = 0.65f;
        S.Direction.Body = 0.50f; S.Direction.Mind = 0.55f; S.Direction.Spirit = 0.45f; S.Direction.Nature = 0.48f;
        S.Meta.Distortion = 0.28f; S.Meta.Stability = 0.65f; S.Meta.Purity = 0.70f;
        S.Meta.Potency = 0.58f; S.Meta.Resonance = 0.55f; S.Meta.Corruption = 0.25f;
        break;
    case EBiomeType::Swamp:
        S.Magnitude = 0.70f;
        S.Direction.Body = 0.35f; S.Direction.Mind = 0.35f; S.Direction.Spirit = 0.65f; S.Direction.Nature = 0.65f;
        S.Meta.Distortion = 0.75f; S.Meta.Stability = 0.25f; S.Meta.Purity = 0.25f;
        S.Meta.Potency = 0.70f; S.Meta.Resonance = 0.80f; S.Meta.Corruption = 0.70f;
        break;
    case EBiomeType::Steppe:
        S.Magnitude = 0.45f;
        S.Direction.Body = 0.65f; S.Direction.Mind = 0.60f; S.Direction.Spirit = 0.30f; S.Direction.Nature = 0.35f;
        S.Meta.Distortion = 0.38f; S.Meta.Stability = 0.50f; S.Meta.Purity = 0.55f;
        S.Meta.Potency = 0.40f; S.Meta.Resonance = 0.45f; S.Meta.Corruption = 0.40f;
        break;
    case EBiomeType::Floodplain:
        S.Magnitude = 0.75f;
        S.Direction.Body = 0.45f; S.Direction.Mind = 0.45f; S.Direction.Spirit = 0.55f; S.Direction.Nature = 0.55f;
        S.Meta.Distortion = 0.50f; S.Meta.Stability = 0.45f; S.Meta.Purity = 0.50f;
        S.Meta.Potency = 0.70f; S.Meta.Resonance = 0.70f; S.Meta.Corruption = 0.45f;
        break;
    default: break;
    }
    float Len = FMath::Sqrt(S.Direction.Body * S.Direction.Body + S.Direction.Mind * S.Direction.Mind + S.Direction.Spirit * S.Direction.Spirit + S.Direction.Nature * S.Direction.Nature);
    if (Len > KINDA_SMALL_NUMBER) { S.Direction.Body /= Len; S.Direction.Mind /= Len; S.Direction.Spirit /= Len; S.Direction.Nature /= Len; }
    S.Meta.Distortion = FMath::Clamp(S.Meta.Distortion, 0.0f, 1.0f);
    S.Meta.Stability = FMath::Clamp(S.Meta.Stability, 0.0f, 1.0f);
    S.Meta.Purity = FMath::Clamp(S.Meta.Purity, 0.0f, 1.0f);
    S.Meta.Potency = FMath::Clamp(S.Meta.Potency, 0.0f, 1.0f);
    S.Meta.Resonance = FMath::Clamp(S.Meta.Resonance, 0.0f, 1.0f);
    S.Meta.Corruption = FMath::Clamp(S.Meta.Corruption, 0.0f, 1.0f);
    S.Magnitude = FMath::Clamp(S.Magnitude, 0.0f, 1.0f);
    return S;
}

FEnvironment FBiomeDefaults::GetDefaultEnvironment(EBiomeType Biome)
{
    FEnvironment Env;
    switch (Biome)
    {
    case EBiomeType::MixedForest: Env.Toxicity = 0.30f; Env.Fertility = 0.70f; Env.Moisture = 0.60f; break;
    case EBiomeType::Swamp:       Env.Toxicity = 0.80f; Env.Fertility = 0.40f; Env.Moisture = 0.85f; break;
    case EBiomeType::Steppe:      Env.Toxicity = 0.60f; Env.Fertility = 0.45f; Env.Moisture = 0.30f; break;
    case EBiomeType::Floodplain:  Env.Toxicity = 0.60f; Env.Fertility = 0.80f; Env.Moisture = 0.80f; break;
    default: break;
    }
    return Env;
}

EResourceType FBiomeDefaults::GetRandomResourceForBiome(EBiomeType Biome, FRandomStream& Rng)
{
    switch (Biome)
    {
    case EBiomeType::MixedForest:
    {
        int32 r = Rng.RandRange(0, 99);
        if (r < 35) return EResourceType::Nettle;      // 35%
        if (r < 55) return EResourceType::Fern;        // 20%
        if (r < 70) return EResourceType::Mushroom;    // 15%
        if (r < 85) return EResourceType::BirchBark;   // 15%
        return EResourceType::Moss;                    // 15%
    }
    case EBiomeType::Swamp:
    {
        int32 r = Rng.RandRange(0, 99);
        if (r < 30) return EResourceType::Mushroom;    // 30%
        if (r < 50) return EResourceType::Cranberry;   // 20%
        if (r < 70) return EResourceType::Moss;        // 20%
        if (r < 85) return EResourceType::BogOre;      // 15%
        return EResourceType::Fern;                    // 15%
    }
    case EBiomeType::Steppe:
    {
        int32 r = Rng.RandRange(0, 99);
        if (r < 50) return EResourceType::Nettle;      // 50%
        if (r < 80) return EResourceType::Fern;        // 30%
        return EResourceType::BirchBark;               // 20%
    }
    case EBiomeType::Floodplain:
    {
        int32 r = Rng.RandRange(0, 99);
        if (r < 40) return EResourceType::Nettle;      // 40%
        if (r < 70) return EResourceType::BirchBark;   // 30%
        if (r < 85) return EResourceType::BogOre;      // 15%
        return EResourceType::Moss;                    // 15%
    }
    default:
        return EResourceType::Nettle;
    }
}