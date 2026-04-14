#include "HerbalistHarvest.h"
#include "ProjectHerbalist.h"
#include "Core/Types/HerbalistCoreTypes.h"
#include "Math/UnrealMathUtility.h"

static constexpr float k_biome = 0.6f;
static constexpr float k_condition = 0.4f;

FRealState FHerbalistHarvest::GetBaseResourceParams(EResourceType Type)
{
    // Базовые значения из GDD таблица 8.4.1
    switch (Type)
    {
    case EResourceType::Nettle:  // крапива
    {
        FRealState R;
        R.Direction.Body = 0.50f; R.Direction.Mind = 0.30f; R.Direction.Spirit = 0.40f; R.Direction.Nature = 0.80f;
        float Len = FMath::Sqrt(R.Direction.Body * R.Direction.Body + R.Direction.Mind * R.Direction.Mind + R.Direction.Spirit * R.Direction.Spirit + R.Direction.Nature * R.Direction.Nature);
        if (Len > KINDA_SMALL_NUMBER) { R.Direction.Body /= Len; R.Direction.Mind /= Len; R.Direction.Spirit /= Len; R.Direction.Nature /= Len; }
        R.Magnitude = 0.55f;
        R.Meta.Distortion = 0.30f; R.Meta.Stability = 0.50f; R.Meta.Purity = 0.60f;
        return R;
    }
    case EResourceType::Fern:    // папоротник
    {
        FRealState R;
        R.Direction.Body = 0.30f; R.Direction.Mind = 0.30f; R.Direction.Spirit = 0.70f; R.Direction.Nature = 0.70f;
        float Len = FMath::Sqrt(R.Direction.Body * R.Direction.Body + R.Direction.Mind * R.Direction.Mind + R.Direction.Spirit * R.Direction.Spirit + R.Direction.Nature * R.Direction.Nature);
        if (Len > KINDA_SMALL_NUMBER) { R.Direction.Body /= Len; R.Direction.Mind /= Len; R.Direction.Spirit /= Len; R.Direction.Nature /= Len; }
        R.Magnitude = 0.65f;
        R.Meta.Distortion = 0.60f; R.Meta.Stability = 0.35f; R.Meta.Purity = 0.40f;
        return R;
    }
    case EResourceType::Mushroom: // мухомор
    {
        FRealState R;
        R.Direction.Body = 0.40f; R.Direction.Mind = 0.80f; R.Direction.Spirit = 0.60f; R.Direction.Nature = 0.20f;
        float Len = FMath::Sqrt(R.Direction.Body * R.Direction.Body + R.Direction.Mind * R.Direction.Mind + R.Direction.Spirit * R.Direction.Spirit + R.Direction.Nature * R.Direction.Nature);
        if (Len > KINDA_SMALL_NUMBER) { R.Direction.Body /= Len; R.Direction.Mind /= Len; R.Direction.Spirit /= Len; R.Direction.Nature /= Len; }
        R.Magnitude = 0.70f;
        R.Meta.Distortion = 0.85f; R.Meta.Stability = 0.25f; R.Meta.Purity = 0.30f;
        return R;
    }
    default:
        return FRealState();
    }
}

FRealState FHerbalistHarvest::Harvest(EResourceType Type, const FRealState& BiomeState, const FConditionModifier& Conditions)
{
    FRealState Base = GetBaseResourceParams(Type);
    const FRealState& S0 = FAlatyr::S0;

    // Разность состояния биома с эталоном
    FRealState BiomeDelta;
    BiomeDelta.Direction.Body = BiomeState.Direction.Body - S0.Direction.Body;
    BiomeDelta.Direction.Mind = BiomeState.Direction.Mind - S0.Direction.Mind;
    BiomeDelta.Direction.Spirit = BiomeState.Direction.Spirit - S0.Direction.Spirit;
    BiomeDelta.Direction.Nature = BiomeState.Direction.Nature - S0.Direction.Nature;
    BiomeDelta.Magnitude = BiomeState.Magnitude - S0.Magnitude;
    BiomeDelta.Meta.Distortion = BiomeState.Meta.Distortion - S0.Meta.Distortion;
    BiomeDelta.Meta.Stability = BiomeState.Meta.Stability - S0.Meta.Stability;
    BiomeDelta.Meta.Purity = BiomeState.Meta.Purity - S0.Meta.Purity;

    // Агрегация
    FRealState Result;
    Result.Direction.Body = Base.Direction.Body + k_biome * BiomeDelta.Direction.Body + k_condition * Conditions.DeltaDirection.Body;
    Result.Direction.Mind = Base.Direction.Mind + k_biome * BiomeDelta.Direction.Mind + k_condition * Conditions.DeltaDirection.Mind;
    Result.Direction.Spirit = Base.Direction.Spirit + k_biome * BiomeDelta.Direction.Spirit + k_condition * Conditions.DeltaDirection.Spirit;
    Result.Direction.Nature = Base.Direction.Nature + k_biome * BiomeDelta.Direction.Nature + k_condition * Conditions.DeltaDirection.Nature;

    Result.Magnitude = Base.Magnitude + k_biome * BiomeDelta.Magnitude + k_condition * Conditions.DeltaMagnitude;
    Result.Meta.Distortion = Base.Meta.Distortion + k_biome * BiomeDelta.Meta.Distortion + k_condition * Conditions.DeltaDistortion;
    Result.Meta.Stability = Base.Meta.Stability + k_biome * BiomeDelta.Meta.Stability + k_condition * Conditions.DeltaStability;
    Result.Meta.Purity = Base.Meta.Purity + k_biome * BiomeDelta.Meta.Purity + k_condition * Conditions.DeltaPurity;

    // Нормализация направления
    float Len = FMath::Sqrt(
        Result.Direction.Body * Result.Direction.Body +
        Result.Direction.Mind * Result.Direction.Mind +
        Result.Direction.Spirit * Result.Direction.Spirit +
        Result.Direction.Nature * Result.Direction.Nature
    );
    if (Len > KINDA_SMALL_NUMBER)
    {
        Result.Direction.Body /= Len;
        Result.Direction.Mind /= Len;
        Result.Direction.Spirit /= Len;
        Result.Direction.Nature /= Len;
    }
    else
    {
        Result.Direction.Body = 0.25f;
        Result.Direction.Mind = 0.25f;
        Result.Direction.Spirit = 0.25f;
        Result.Direction.Nature = 0.25f;
    }

    // Клиппинг
    Result.Magnitude = FMath::Clamp(Result.Magnitude, 0.0f, 1.0f);
    Result.Meta.Distortion = FMath::Clamp(Result.Meta.Distortion, 0.0f, 1.0f);
    Result.Meta.Stability = FMath::Clamp(Result.Meta.Stability, 0.0f, 1.0f);
    Result.Meta.Purity = FMath::Clamp(Result.Meta.Purity, 0.0f, 1.0f);

    UE_LOG(LogHerbalist, Verbose, TEXT("[HARVEST] Type=%d Mag=%.3f Dir=(%.2f,%.2f,%.2f,%.2f)"),
        (int32)Type, Result.Magnitude,
        Result.Direction.Body, Result.Direction.Mind,
        Result.Direction.Spirit, Result.Direction.Nature);

    return Result;
}