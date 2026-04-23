// PipelineFold.cpp
#include "HerbalistPipeline.h"
#include "PipelineTypes.h"
#include "Core/HerbalistSettings.h"
#include "Core/Types/HerbalistCoreMath.h"
#include "Math/UnrealMathUtility.h"

namespace HerbalistCore
{
    // =========================================================================
    // L1 Агрегация (старая версия)
    // =========================================================================
    FRealState Pipeline::Fold(const TArray<FRealState>& Inputs)
    {
        if (Inputs.Num() == 0) return FRealState();

        const UHerbalistSettings* Settings = GetHerbalistSettings();
        float Weight = 1.0f;
        const float WeightDecay = Settings ? Settings->FoldWeightDecay : 0.8f;
        float TotalWeight = 0.0f;

        FRealState Accumulated;
        Accumulated.Magnitude = 0.0f;
        Accumulated.Direction.Body = 0.0f;
        Accumulated.Direction.Mind = 0.0f;
        Accumulated.Direction.Spirit = 0.0f;
        Accumulated.Direction.Nature = 0.0f;
        Accumulated.Meta.Distortion = 0.0f;
        Accumulated.Meta.Stability = 0.0f;
        Accumulated.Meta.Purity = 0.0f;
        Accumulated.Meta.Potency = 0.0f;
        Accumulated.Meta.Resonance = 0.0f;
        Accumulated.Meta.Corruption = 0.0f;

        for (const FRealState& Res : Inputs)
        {
            float w = Weight;
            TotalWeight += w;

            Accumulated.Magnitude += Res.Magnitude * w;
            Accumulated.Direction.Body += Res.Direction.Body * w;
            Accumulated.Direction.Mind += Res.Direction.Mind * w;
            Accumulated.Direction.Spirit += Res.Direction.Spirit * w;
            Accumulated.Direction.Nature += Res.Direction.Nature * w;
            Accumulated.Meta.Distortion += Res.Meta.Distortion * w;
            Accumulated.Meta.Stability += Res.Meta.Stability * w;
            Accumulated.Meta.Purity += Res.Meta.Purity * w;
            Accumulated.Meta.Potency += Res.Meta.Potency * w;
            Accumulated.Meta.Resonance += Res.Meta.Resonance * w;
            Accumulated.Meta.Corruption += Res.Meta.Corruption * w;

            Weight *= WeightDecay;
        }

        if (TotalWeight > KINDA_SMALL_NUMBER)
        {
            Accumulated.Magnitude /= TotalWeight;
            Accumulated.Direction.Body /= TotalWeight;
            Accumulated.Direction.Mind /= TotalWeight;
            Accumulated.Direction.Spirit /= TotalWeight;
            Accumulated.Direction.Nature /= TotalWeight;
            Accumulated.Meta.Distortion /= TotalWeight;
            Accumulated.Meta.Stability /= TotalWeight;
            Accumulated.Meta.Purity /= TotalWeight;
            Accumulated.Meta.Potency /= TotalWeight;
            Accumulated.Meta.Resonance /= TotalWeight;
            Accumulated.Meta.Corruption /= TotalWeight;
        }

        Accumulated.Direction.NormalizeSum();

        Accumulated.Magnitude = FMath::Clamp(Accumulated.Magnitude, 0.0f, 1.0f);
        Accumulated.Meta.Distortion = FMath::Clamp(Accumulated.Meta.Distortion, 0.0f, 1.0f);
        Accumulated.Meta.Stability = FMath::Clamp(Accumulated.Meta.Stability, 0.0f, 1.0f);
        Accumulated.Meta.Purity = FMath::Clamp(Accumulated.Meta.Purity, 0.0f, 1.0f);
        Accumulated.Meta.Potency = FMath::Clamp(Accumulated.Meta.Potency, 0.0f, 1.0f);
        Accumulated.Meta.Resonance = FMath::Clamp(Accumulated.Meta.Resonance, 0.0f, 1.0f);
        Accumulated.Meta.Corruption = FMath::Clamp(Accumulated.Meta.Corruption, 0.0f, 1.0f);

        return Accumulated;
    }

    // =========================================================================
    // L2 Агрегация (новая версия)
    // =========================================================================
    FAggregatedL2 FoldL2(const TArray<FRealState>& Inputs, FRngState& Rng)
    {
        FAggregatedL2 Result;
        if (Inputs.Num() == 0)
        {
            Result.Dir = FL2Direction();
            Result.Magnitude = 0.0f;
            return Result;
        }

        const UHerbalistSettings* Settings = GetHerbalistSettings();
        float Weight = 1.0f;
        const float WeightDecay = Settings ? Settings->FoldWeightDecay : 0.8f;
        float TotalWeight = 0.0f;

        FL2Direction AccumDir;
        float AccumMag = 0.0f;
        FMeta AccumMeta;

        for (const FRealState& Res : Inputs)
        {
            float w = Weight;
            TotalWeight += w;

            FL2Direction L2 = ToL2(Res.Direction, Rng);
            AccumDir.Body   += L2.Body * w;
            AccumDir.Mind   += L2.Mind * w;
            AccumDir.Spirit += L2.Spirit * w;
            AccumDir.Nature += L2.Nature * w;

            AccumMag += Res.Magnitude * w;

            AccumMeta.Distortion += Res.Meta.Distortion * w;
            AccumMeta.Stability  += Res.Meta.Stability * w;
            AccumMeta.Purity     += Res.Meta.Purity * w;
            AccumMeta.Potency    += Res.Meta.Potency * w;
            AccumMeta.Resonance  += Res.Meta.Resonance * w;
            AccumMeta.Corruption += Res.Meta.Corruption * w;

            Weight *= WeightDecay;
        }

        if (TotalWeight > KINDA_SMALL_NUMBER)
        {
            AccumDir.Body   /= TotalWeight;
            AccumDir.Mind   /= TotalWeight;
            AccumDir.Spirit /= TotalWeight;
            AccumDir.Nature /= TotalWeight;
            AccumMag        /= TotalWeight;

            AccumMeta.Distortion /= TotalWeight;
            AccumMeta.Stability  /= TotalWeight;
            AccumMeta.Purity     /= TotalWeight;
            AccumMeta.Potency    /= TotalWeight;
            AccumMeta.Resonance  /= TotalWeight;
            AccumMeta.Corruption /= TotalWeight;
        }

        AccumDir.NormalizeL2(Rng);
        Result.Dir = AccumDir;
        Result.Magnitude = AccumMag;
        Result.Meta = AccumMeta;

        return Result;
    }

    // =========================================================================
    // Агрегация воды (L1)
    // =========================================================================
    FRealState Pipeline::AggregateWater(const TArray<FRealState>& WaterStates)
    {
        FRealState WaterAggregated;
        if (WaterStates.Num() == 0) return WaterAggregated;

        WaterAggregated.Magnitude = 0.0f;
        WaterAggregated.Direction.Body = 0.0f;
        WaterAggregated.Direction.Mind = 0.0f;
        WaterAggregated.Direction.Spirit = 0.0f;
        WaterAggregated.Direction.Nature = 0.0f;
        WaterAggregated.Meta.Purity = 0.0f;
        WaterAggregated.Meta.Stability = 0.0f;
        WaterAggregated.Meta.Corruption = 0.0f;
        WaterAggregated.Meta.Potency = 0.0f;

        for (const FRealState& W : WaterStates)
        {
            WaterAggregated.Magnitude += W.Magnitude;
            WaterAggregated.Direction.Body += W.Direction.Body;
            WaterAggregated.Direction.Mind += W.Direction.Mind;
            WaterAggregated.Direction.Spirit += W.Direction.Spirit;
            WaterAggregated.Direction.Nature += W.Direction.Nature;
            WaterAggregated.Meta.Purity += W.Meta.Purity;
            WaterAggregated.Meta.Stability += W.Meta.Stability;
            WaterAggregated.Meta.Corruption += W.Meta.Corruption;
            WaterAggregated.Meta.Potency += W.Meta.Potency;
        }

        float WaterCount = (float)WaterStates.Num();
        WaterAggregated.Magnitude /= WaterCount;
        WaterAggregated.Direction.Body /= WaterCount;
        WaterAggregated.Direction.Mind /= WaterCount;
        WaterAggregated.Direction.Spirit /= WaterCount;
        WaterAggregated.Direction.Nature /= WaterCount;
        WaterAggregated.Meta.Purity /= WaterCount;
        WaterAggregated.Meta.Stability /= WaterCount;
        WaterAggregated.Meta.Corruption /= WaterCount;
        WaterAggregated.Meta.Potency /= WaterCount;
        WaterAggregated.Direction.NormalizeSum();

        return WaterAggregated;
    }

    // =========================================================================
    // Смешивание воды и не-воды (L1)
    // =========================================================================
    FRealState Pipeline::BlendWaterAndNonWater(
        const FRealState& NonWaterAggregated,
        const FRealState& WaterAggregated,
        int32 NonWaterCount,
        int32 WaterCount)
    {
        const UHerbalistSettings* Settings = GetHerbalistSettings();
        const float MaxWaterRatio = Settings ? Settings->MaxWaterRatio : 0.8f;
        const float WaterDilutionPenalty = Settings ? Settings->WaterDilutionPenalty : 0.2f;

        float TotalNonWaterVolume = (float)NonWaterCount;
        float TotalWaterVolume = (float)WaterCount;
        float WaterRatio = TotalWaterVolume / (TotalWaterVolume + TotalNonWaterVolume);
        float EffectiveWaterRatio = FMath::Min(WaterRatio, MaxWaterRatio);
        float DilutionPenalty = (WaterRatio > MaxWaterRatio) ? WaterDilutionPenalty : 1.0f;

        const float NonWaterWeight = 1.0f - EffectiveWaterRatio;
        const float WaterWeight = EffectiveWaterRatio;

        FRealState Aggregated;
        Aggregated.Magnitude = NonWaterAggregated.Magnitude * (1.0f - EffectiveWaterRatio * 0.8f) * DilutionPenalty;
        Aggregated.Magnitude = FMath::Clamp(Aggregated.Magnitude, 0.0f, 1.0f);

        Aggregated.Direction.Body = NonWaterAggregated.Direction.Body * NonWaterWeight + WaterAggregated.Direction.Body * WaterWeight;
        Aggregated.Direction.Mind = NonWaterAggregated.Direction.Mind * NonWaterWeight + WaterAggregated.Direction.Mind * WaterWeight;
        Aggregated.Direction.Spirit = NonWaterAggregated.Direction.Spirit * NonWaterWeight + WaterAggregated.Direction.Spirit * WaterWeight;
        Aggregated.Direction.Nature = NonWaterAggregated.Direction.Nature * NonWaterWeight + WaterAggregated.Direction.Nature * WaterWeight;
        Aggregated.Direction.NormalizeSum();

        Aggregated.Meta.Purity = NonWaterAggregated.Meta.Purity * NonWaterWeight + WaterAggregated.Meta.Purity * WaterWeight;
        Aggregated.Meta.Stability = NonWaterAggregated.Meta.Stability * NonWaterWeight + WaterAggregated.Meta.Stability * WaterWeight;
        Aggregated.Meta.Corruption = NonWaterAggregated.Meta.Corruption * NonWaterWeight + WaterAggregated.Meta.Corruption * WaterWeight;
        Aggregated.Meta.Potency = NonWaterAggregated.Meta.Potency * NonWaterWeight + WaterAggregated.Meta.Potency * WaterWeight;
        Aggregated.Meta.Resonance = NonWaterAggregated.Meta.Resonance;
        Aggregated.Meta.Distortion = NonWaterAggregated.Meta.Distortion;

        Aggregated.Meta.Purity = FMath::Clamp(Aggregated.Meta.Purity, 0.0f, 1.0f);
        Aggregated.Meta.Stability = FMath::Clamp(Aggregated.Meta.Stability, 0.0f, 1.0f);
        Aggregated.Meta.Corruption = FMath::Clamp(Aggregated.Meta.Corruption, 0.0f, 1.0f);
        Aggregated.Meta.Potency = FMath::Clamp(Aggregated.Meta.Potency, 0.0f, 1.0f);
        Aggregated.Meta.Resonance = FMath::Clamp(Aggregated.Meta.Resonance, 0.0f, 1.0f);
        Aggregated.Meta.Distortion = FMath::Clamp(Aggregated.Meta.Distortion, 0.0f, 1.0f);

        return Aggregated;
    }

} // namespace HerbalistCore