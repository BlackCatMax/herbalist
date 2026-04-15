#include "BiomeTypes.h"
#include "Math/UnrealMathUtility.h"

FRealState FBiomeDefaults::GetDefaultState(EBiomeType Biome)
{
    FRealState S;
    // Значения из GDD таблица 8.1.1 (упрощённо для прототипа)
    switch (Biome)
    {
    case EBiomeType::MixedForest:
        S.Magnitude = 0.65f;
        S.Direction.Body = 0.50f; S.Direction.Mind = 0.55f; S.Direction.Spirit = 0.45f; S.Direction.Nature = 0.48f;
        S.Meta.Distortion = 0.28f; S.Meta.Stability = 0.65f; S.Meta.Purity = 0.70f;
        break;
    case EBiomeType::Swamp:
        S.Magnitude = 0.70f;
        S.Direction.Body = 0.35f; S.Direction.Mind = 0.35f; S.Direction.Spirit = 0.65f; S.Direction.Nature = 0.65f;
        S.Meta.Distortion = 0.75f; S.Meta.Stability = 0.25f; S.Meta.Purity = 0.25f;
        break;
    case EBiomeType::Steppe:
        S.Magnitude = 0.45f;
        S.Direction.Body = 0.65f; S.Direction.Mind = 0.60f; S.Direction.Spirit = 0.30f; S.Direction.Nature = 0.35f;
        S.Meta.Distortion = 0.38f; S.Meta.Stability = 0.50f; S.Meta.Purity = 0.55f;
        break;
    case EBiomeType::Floodplain:
        S.Magnitude = 0.75f;
        S.Direction.Body = 0.45f; S.Direction.Mind = 0.45f; S.Direction.Spirit = 0.55f; S.Direction.Nature = 0.55f;
        S.Meta.Distortion = 0.50f; S.Meta.Stability = 0.45f; S.Meta.Purity = 0.50f;
        break;
    default:
        break;
    }
    // Нормализация направления
    float Len = FMath::Sqrt(S.Direction.Body * S.Direction.Body + S.Direction.Mind * S.Direction.Mind +
        S.Direction.Spirit * S.Direction.Spirit + S.Direction.Nature * S.Direction.Nature);
    if (Len > KINDA_SMALL_NUMBER)
    {
        S.Direction.Body /= Len;
        S.Direction.Mind /= Len;
        S.Direction.Spirit /= Len;
        S.Direction.Nature /= Len;
    }
    S.Meta.Distortion = FMath::Clamp(S.Meta.Distortion, 0.0f, 1.0f);
    S.Meta.Stability = FMath::Clamp(S.Meta.Stability, 0.0f, 1.0f);
    S.Meta.Purity = FMath::Clamp(S.Meta.Purity, 0.0f, 1.0f);
    return S;
}

FEnvironment FBiomeDefaults::GetDefaultEnvironment(EBiomeType Biome)
{
    FEnvironment Env;
    // Взять из таблицы дополнительных параметров биомов (GDD 8.1.1)
    switch (Biome)
    {
    case EBiomeType::MixedForest:
        Env.Toxicity = 0.25f; Env.Fertility = 0.70f; Env.Moisture = 0.60f;
        break;
    case EBiomeType::Swamp:
        Env.Toxicity = 0.75f; Env.Fertility = 0.40f; Env.Moisture = 0.85f;
        break;
    case EBiomeType::Steppe:
        Env.Toxicity = 0.40f; Env.Fertility = 0.45f; Env.Moisture = 0.30f;
        break;
    case EBiomeType::Floodplain:
        Env.Toxicity = 0.45f; Env.Fertility = 0.80f; Env.Moisture = 0.80f;
        break;
    default:
        break;
    }
    return Env;
}