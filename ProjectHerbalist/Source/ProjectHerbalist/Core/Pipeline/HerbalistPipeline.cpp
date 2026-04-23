// HerbalistPipeline.cpp
#include "HerbalistPipeline.h"
#include "ProjectHerbalist.h"
#include "Core/Types/HerbalistCoreTypes.h"
#include "Core/Inventory/HerbalistInventoryComponent.h"
#include "Math/UnrealMathUtility.h"
#include "Core/HerbalistSettings.h"
#include "Core/Types/HerbalistIngredient.h"
#include "Core/Harvest/HarvestService.h"

namespace HerbalistCore
{
    // Вспомогательная функция проверки, является ли ингредиент водой
    static bool IsWaterIngredient(FName IngredientID)
    {
        // Специальный случай: зелье не является водой
        if (IngredientID == FName(TEXT("Potion"))) return false;
        // Если это вода из клетки (ID "Water")
        if (IngredientID == FName(TEXT("Water"))) return true;
        UHerbalistIngredient* Ingredient = UHarvestService::LoadIngredientAssetStatic(IngredientID);
        return Ingredient && Ingredient->bIsWater;
    }

    float Random01(FRngState& Rng)
    {
        Rng.Seed = (Rng.Seed * 196314165) + 907633515;
        return (Rng.Seed & 0x00FFFFFF) / float(0x01000000);
    }

    static float RandomRange(FRngState& Rng, float Min, float Max)
    {
        return Min + (Max - Min) * Random01(Rng);
    }

    // =========================
    // FOLD
    // =========================
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

    // =========================
    // COMPUTE DELTA
    // =========================
    FRealState Pipeline::ComputeDelta(const FRealState& Aggregated, const FRealState& CurrentBiomeState)
    {
        FRealState Delta;
        Delta.Direction.Body = Aggregated.Direction.Body - CurrentBiomeState.Direction.Body;
        Delta.Direction.Mind = Aggregated.Direction.Mind - CurrentBiomeState.Direction.Mind;
        Delta.Direction.Spirit = Aggregated.Direction.Spirit - CurrentBiomeState.Direction.Spirit;
        Delta.Direction.Nature = Aggregated.Direction.Nature - CurrentBiomeState.Direction.Nature;

        Delta.Magnitude = Aggregated.Magnitude - CurrentBiomeState.Magnitude;
        Delta.Meta.Distortion = Aggregated.Meta.Distortion - CurrentBiomeState.Meta.Distortion;
        Delta.Meta.Stability = Aggregated.Meta.Stability - CurrentBiomeState.Meta.Stability;
        Delta.Meta.Purity = Aggregated.Meta.Purity - CurrentBiomeState.Meta.Purity;
        Delta.Meta.Potency = Aggregated.Meta.Potency - CurrentBiomeState.Meta.Potency;
        Delta.Meta.Resonance = Aggregated.Meta.Resonance - CurrentBiomeState.Meta.Resonance;
        Delta.Meta.Corruption = Aggregated.Meta.Corruption - CurrentBiomeState.Meta.Corruption;

        return Delta;
    }

    // =========================
    // ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ ПАЙПЛАЙНА
    // =========================

    void Pipeline::SeparateWaterAndIngredients(
        const TArray<FInventoryItem>& Inputs,
        TArray<FRealState>& OutNonWater,
        TArray<FRealState>& OutWater)
    {
        OutNonWater.Empty();
        OutWater.Empty();
        for (const FInventoryItem& Item : Inputs)
        {
            if (IsWaterIngredient(Item.IngredientID))
                OutWater.Add(Item.State);
            else
                OutNonWater.Add(Item.State);
        }
    }

    FRealState Pipeline::ProcessWaterOnly(const TArray<FRealState>& WaterStates)
    {
        FRealState AvgWater;
        AvgWater.Magnitude = 0.0f;
        AvgWater.Direction.Body = 0.0f; AvgWater.Direction.Mind = 0.0f;
        AvgWater.Direction.Spirit = 0.0f; AvgWater.Direction.Nature = 0.0f;
        AvgWater.Meta.Purity = 0.0f; AvgWater.Meta.Stability = 0.0f;
        AvgWater.Meta.Corruption = 0.0f; AvgWater.Meta.Potency = 0.0f;

        for (const FRealState& W : WaterStates)
        {
            AvgWater.Magnitude += W.Magnitude;
            AvgWater.Direction.Body += W.Direction.Body;
            AvgWater.Direction.Mind += W.Direction.Mind;
            AvgWater.Direction.Spirit += W.Direction.Spirit;
            AvgWater.Direction.Nature += W.Direction.Nature;
            AvgWater.Meta.Purity += W.Meta.Purity;
            AvgWater.Meta.Stability += W.Meta.Stability;
            AvgWater.Meta.Corruption += W.Meta.Corruption;
            AvgWater.Meta.Potency += W.Meta.Potency;
        }
        float Count = (float)WaterStates.Num();
        AvgWater.Magnitude /= Count;
        AvgWater.Direction.Body /= Count;
        AvgWater.Direction.Mind /= Count;
        AvgWater.Direction.Spirit /= Count;
        AvgWater.Direction.Nature /= Count;
        AvgWater.Meta.Purity /= Count;
        AvgWater.Meta.Stability /= Count;
        AvgWater.Meta.Corruption /= Count;
        AvgWater.Meta.Potency /= Count;
        AvgWater.Direction.NormalizeSum();

        // Кипячение
        AvgWater.Magnitude = FMath::Clamp(AvgWater.Magnitude * 0.8f, 0.0f, 1.0f);
        AvgWater.Meta.Purity = FMath::Clamp(AvgWater.Meta.Purity + 0.2f, 0.0f, 1.0f);
        AvgWater.Meta.Stability = FMath::Clamp(AvgWater.Meta.Stability + 0.1f, 0.0f, 1.0f);
        AvgWater.Meta.Distortion = FMath::Clamp(AvgWater.Meta.Distortion - 0.2f, 0.0f, 1.0f);
        AvgWater.Meta.Corruption = FMath::Clamp(AvgWater.Meta.Corruption - 0.1f, 0.0f, 1.0f);

        FVector4 DirVec(AvgWater.Direction.Body, AvgWater.Direction.Mind, AvgWater.Direction.Spirit, AvgWater.Direction.Nature);
        FVector4 Center(0.25f, 0.25f, 0.25f, 0.25f);
        DirVec = FMath::Lerp(DirVec, Center, 0.2f);
        float Sum = DirVec.X + DirVec.Y + DirVec.Z + DirVec.W;
        if (Sum > KINDA_SMALL_NUMBER)
        {
            AvgWater.Direction.Body = DirVec.X / Sum;
            AvgWater.Direction.Mind = DirVec.Y / Sum;
            AvgWater.Direction.Spirit = DirVec.Z / Sum;
            AvgWater.Direction.Nature = DirVec.W / Sum;
        }
        else
        {
            AvgWater.Direction.Body = AvgWater.Direction.Mind = AvgWater.Direction.Spirit = AvgWater.Direction.Nature = 0.25f;
        }

        UE_LOG(LogHerbalist, Log, TEXT("Boiled water produced: Mag=%.2f Purity=%.2f Dist=%.2f"), AvgWater.Magnitude, AvgWater.Meta.Purity, AvgWater.Meta.Distortion);
        return AvgWater;
    }

    FRealState Pipeline::ProcessNoWater()
    {
        FRealState Ash;
        Ash.Magnitude = 0.1f;
        Ash.Meta.Distortion = 0.9f;
        Ash.Meta.Stability = 0.0f;
        Ash.Meta.Purity = 0.0f;
        Ash.Meta.Potency = 0.0f;
        Ash.Meta.Resonance = 0.0f;
        Ash.Meta.Corruption = 0.9f;
        Ash.Direction.Body = Ash.Direction.Mind = Ash.Direction.Spirit = Ash.Direction.Nature = 0.25f;
        UE_LOG(LogHerbalist, Warning, TEXT("No water in ingredients! Produced ash."));
        return Ash;
    }

    FRealState Pipeline::AggregateWater(const TArray<FRealState>& WaterStates)
    {
        FRealState WaterAggregated;
        WaterAggregated.Magnitude = 0.0f;
        WaterAggregated.Direction.Body = 0.0f; WaterAggregated.Direction.Mind = 0.0f;
        WaterAggregated.Direction.Spirit = 0.0f; WaterAggregated.Direction.Nature = 0.0f;
        WaterAggregated.Meta.Purity = 0.0f; WaterAggregated.Meta.Stability = 0.0f;
        WaterAggregated.Meta.Corruption = 0.0f; WaterAggregated.Meta.Potency = 0.0f;

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

    float Pipeline::ComputeBaseDistortion(const FEnvironment& Env, const FMemoryState& Memory)
    {
        const UHerbalistSettings* Settings = GetHerbalistSettings();
        const float ToxWeight = Settings ? Settings->EnvironmentToxicityWeight : 0.5f;

        float EnvDist = Env.Toxicity * ToxWeight;
        float MemoryDist = Memory.AccumulatedDistortion;
        float Distortion = 1.0f - (1.0f - EnvDist) * (1.0f - MemoryDist);
        Distortion = FMath::Clamp(Distortion, 0.0f, 0.95f);
        return Distortion;
    }

    void Pipeline::ApplyBiomeContext(
        float& InOutDistortion,
        float& InOutZaryanaStrength,
        FRealState& InOutDelta,
        float BiomeMorokField,
        float BiomeZaryanaField,
        const FVector4& BiomeAxisDrift)
    {
        const UHerbalistSettings* Settings = GetHerbalistSettings();
        const float MorokInfluence = Settings ? Settings->BiomeMorokInfluence : 0.3f;
        const float ZaryanaInfluence = Settings ? Settings->BiomeZaryanaInfluence : 0.3f;
        const float DriftWeight = Settings ? Settings->BiomeAxisDriftWeight : 0.1f;

        InOutDistortion = FMath::Clamp(InOutDistortion + BiomeMorokField * MorokInfluence, 0.0f, 0.95f);
        UE_LOG(LogHerbalist, Warning, TEXT("[BIOME CONTEXT] MorokField=%.3f -> Distortion=%.3f"), BiomeMorokField, InOutDistortion);

        InOutZaryanaStrength = FMath::Clamp(InOutZaryanaStrength + BiomeZaryanaField * ZaryanaInfluence, 0.0f, 1.0f);
        UE_LOG(LogHerbalist, Warning, TEXT("[BIOME CONTEXT] ZaryanaField=%.3f -> ZaryanaStrength=%.3f"), BiomeZaryanaField, InOutZaryanaStrength);

        InOutDelta.Direction.Body   += BiomeAxisDrift.X * DriftWeight;
        InOutDelta.Direction.Mind   += BiomeAxisDrift.Y * DriftWeight;
        InOutDelta.Direction.Spirit += BiomeAxisDrift.Z * DriftWeight;
        InOutDelta.Direction.Nature += BiomeAxisDrift.W * DriftWeight;
        UE_LOG(LogHerbalist, Warning, TEXT("[BIOME AXIS DRIFT] Applied: (%.3f, %.3f, %.3f, %.3f)"),
            BiomeAxisDrift.X, BiomeAxisDrift.Y, BiomeAxisDrift.Z, BiomeAxisDrift.W);
    }

    void Pipeline::ApplyPotencyResonanceCorruption(
        FRealState& InOutDelta,
        const FMeta& Meta)
    {
        float PotencyScale = 1.0f + (Meta.Potency - 0.5f) * 0.5f;
        InOutDelta.Magnitude *= PotencyScale;
        InOutDelta.Meta.Distortion *= PotencyScale;
        InOutDelta.Meta.Stability *= PotencyScale;
        InOutDelta.Meta.Purity *= PotencyScale;
        InOutDelta.Meta.Potency *= PotencyScale;
        InOutDelta.Meta.Resonance *= PotencyScale;
        InOutDelta.Meta.Corruption *= PotencyScale;

        float ResonanceFactor = 1.0f + Meta.Resonance * 0.5f;
        InOutDelta.Direction.Body = InOutDelta.Direction.Body > 0 ? InOutDelta.Direction.Body * ResonanceFactor : InOutDelta.Direction.Body;
        InOutDelta.Direction.Mind = InOutDelta.Direction.Mind > 0 ? InOutDelta.Direction.Mind * ResonanceFactor : InOutDelta.Direction.Mind;
        InOutDelta.Direction.Spirit = InOutDelta.Direction.Spirit > 0 ? InOutDelta.Direction.Spirit * ResonanceFactor : InOutDelta.Direction.Spirit;
        InOutDelta.Direction.Nature = InOutDelta.Direction.Nature > 0 ? InOutDelta.Direction.Nature * ResonanceFactor : InOutDelta.Direction.Nature;

        InOutDelta.Meta.Distortion += Meta.Corruption * 0.1f;
        InOutDelta.Meta.Stability -= Meta.Corruption * 0.05f;
        InOutDelta.Meta.Purity -= Meta.Corruption * 0.05f;
        InOutDelta.Meta.Corruption += Meta.Corruption * 0.05f;

        UE_LOG(LogHerbalist, Warning, TEXT("[POTENCY] Scale=%.3f | [RESONANCE] Factor=%.3f | [CORRUPTION] AddDist=%.3f"),
            PotencyScale, ResonanceFactor, Meta.Corruption * 0.1f);
    }

    void Pipeline::ApplyMorokDistortion(
        FRealState& InOutDelta,
        float Distortion,
        FRngState& Rng)
    {
        const UHerbalistSettings* Settings = GetHerbalistSettings();
        const float MixStrengthFactor = Settings ? Settings->MorokMixStrengthFactor : 0.5f;

        float NoiseMagnitude = RandomRange(Rng, 0.0f, Distortion);
        float NoiseDirection = RandomRange(Rng, -1.0f, 1.0f);
        float RawNoise = NoiseMagnitude * NoiseDirection;
        float Nonlinear = FMath::Tanh(RawNoise * 2.0f);
        float MixStrength = Distortion * MixStrengthFactor;

        FRealState OriginalDelta = InOutDelta;
        InOutDelta.Direction.Body = OriginalDelta.Direction.Body * (1.0f - MixStrength) +
            OriginalDelta.Direction.Spirit * MixStrength +
            Nonlinear * 0.1f;
        InOutDelta.Direction.Spirit = OriginalDelta.Direction.Spirit * (1.0f - MixStrength) +
            OriginalDelta.Direction.Body * MixStrength +
            Nonlinear * 0.1f;
        InOutDelta.Direction.Mind = OriginalDelta.Direction.Mind * (1.0f - MixStrength) +
            OriginalDelta.Direction.Nature * MixStrength +
            Nonlinear * 0.1f;
        InOutDelta.Direction.Nature = OriginalDelta.Direction.Nature * (1.0f - MixStrength) +
            OriginalDelta.Direction.Mind * MixStrength +
            Nonlinear * 0.1f;

        InOutDelta.Magnitude = InOutDelta.Magnitude * (1.0f + Nonlinear * 0.2f);
        InOutDelta.Magnitude = FMath::Clamp(InOutDelta.Magnitude, -1.0f, 1.0f);

        UE_LOG(LogHerbalist, Warning, TEXT("[MOROK] Noise: %.3f -> Nonlinear: %.3f | MixStrength: %.3f"),
            RawNoise, Nonlinear, MixStrength);
    }

    void Pipeline::ApplyZaryanaStructuring(
        FRealState& InOutDelta,
        float ZaryanaStrength,
        float Distortion)
    {
        const UHerbalistSettings* Settings = GetHerbalistSettings();
        const float BoostFactor = Settings ? Settings->ZaryanaBoostFactor : 0.5f;
        const float SuppressFactor = Settings ? Settings->ZaryanaSuppressFactor : 0.3f;

        float AvgDir = (InOutDelta.Direction.Body + InOutDelta.Direction.Mind +
            InOutDelta.Direction.Spirit + InOutDelta.Direction.Nature) / 4.0f;
        float Boost = 1.0f + ZaryanaStrength * BoostFactor;
        float Suppress = 1.0f - ZaryanaStrength * SuppressFactor;

        InOutDelta.Direction.Body = (InOutDelta.Direction.Body > AvgDir) ? InOutDelta.Direction.Body * Boost : InOutDelta.Direction.Body * Suppress;
        InOutDelta.Direction.Mind = (InOutDelta.Direction.Mind > AvgDir) ? InOutDelta.Direction.Mind * Boost : InOutDelta.Direction.Mind * Suppress;
        InOutDelta.Direction.Spirit = (InOutDelta.Direction.Spirit > AvgDir) ? InOutDelta.Direction.Spirit * Boost : InOutDelta.Direction.Spirit * Suppress;
        InOutDelta.Direction.Nature = (InOutDelta.Direction.Nature > AvgDir) ? InOutDelta.Direction.Nature * Boost : InOutDelta.Direction.Nature * Suppress;

        float StabilityIncrease = ZaryanaStrength * (1.0f - Distortion) * 0.2f;
        float PurityIncrease = ZaryanaStrength * (1.0f - Distortion) * 0.15f;
        InOutDelta.Meta.Stability += StabilityIncrease;
        InOutDelta.Meta.Purity += PurityIncrease;
        InOutDelta.Meta.Stability = FMath::Clamp(InOutDelta.Meta.Stability, -1.0f, 1.0f);
        InOutDelta.Meta.Purity = FMath::Clamp(InOutDelta.Meta.Purity, -1.0f, 1.0f);

        InOutDelta.Direction.NormalizeSum();

        UE_LOG(LogHerbalist, Warning, TEXT("[ZARYANA_STRUCT] Boost: %.2f, Suppress: %.2f, Stability+%.3f, Purity+%.3f"),
            Boost, Suppress, StabilityIncrease, PurityIncrease);
    }

    FRealState Pipeline::FinalizeState(
        const FRealState& CurrentBiomeState,
        const FRealState& Delta)
    {
        FRealState NewState = CurrentBiomeState;

        FDirection NewDir;
        NewDir.Body = NewState.Direction.Body + Delta.Direction.Body;
        NewDir.Mind = NewState.Direction.Mind + Delta.Direction.Mind;
        NewDir.Spirit = NewState.Direction.Spirit + Delta.Direction.Spirit;
        NewDir.Nature = NewState.Direction.Nature + Delta.Direction.Nature;
        NewDir.NormalizeSum();

        float InterpAlpha = FMath::Clamp(Delta.Magnitude * 2.0f, 0.0f, 1.0f);
        NewState.Direction.Body = FMath::Lerp(NewState.Direction.Body, NewDir.Body, InterpAlpha);
        NewState.Direction.Mind = FMath::Lerp(NewState.Direction.Mind, NewDir.Mind, InterpAlpha);
        NewState.Direction.Spirit = FMath::Lerp(NewState.Direction.Spirit, NewDir.Spirit, InterpAlpha);
        NewState.Direction.Nature = FMath::Lerp(NewState.Direction.Nature, NewDir.Nature, InterpAlpha);
        NewState.Direction.NormalizeSum();

        NewState.Magnitude += Delta.Magnitude;
        NewState.Meta.Distortion += Delta.Meta.Distortion;
        NewState.Meta.Stability += Delta.Meta.Stability;
        NewState.Meta.Purity += Delta.Meta.Purity;
        NewState.Meta.Potency += Delta.Meta.Potency;
        NewState.Meta.Resonance += Delta.Meta.Resonance;
        NewState.Meta.Corruption += Delta.Meta.Corruption;

        NewState.Direction.NormalizeSum();

        NewState.Magnitude = FMath::Clamp(NewState.Magnitude, 0.0f, 1.0f);
        NewState.Meta.Distortion = FMath::Clamp(NewState.Meta.Distortion, 0.0f, 1.0f);
        NewState.Meta.Stability = FMath::Clamp(NewState.Meta.Stability, 0.0f, 1.0f);
        NewState.Meta.Purity = FMath::Clamp(NewState.Meta.Purity, 0.0f, 1.0f);
        NewState.Meta.Potency = FMath::Clamp(NewState.Meta.Potency, 0.0f, 1.0f);
        NewState.Meta.Resonance = FMath::Clamp(NewState.Meta.Resonance, 0.0f, 1.0f);
        NewState.Meta.Corruption = FMath::Clamp(NewState.Meta.Corruption, 0.0f, 1.0f);

        float Integrity = (1.0f - NewState.Meta.Distortion * 0.7f) * (1.0f + NewState.Meta.Stability);
        float StructureFactor = FMath::Clamp(Integrity, 0.0f, 1.0f);
        NewState.Magnitude *= StructureFactor;

        UE_LOG(LogHerbalist, Warning, TEXT("[NEW_STATE] Mag: %.3f | Dir: (%.2f, %.2f, %.2f, %.2f) | Dist:%.3f Stab:%.3f Pur:%.3f Pot:%.3f Res:%.3f Cor:%.3f"),
            NewState.Magnitude,
            NewState.Direction.Body, NewState.Direction.Mind,
            NewState.Direction.Spirit, NewState.Direction.Nature,
            NewState.Meta.Distortion, NewState.Meta.Stability, NewState.Meta.Purity,
            NewState.Meta.Potency, NewState.Meta.Resonance, NewState.Meta.Corruption);

        return NewState;
    }

    // =========================
    // ГЛАВНЫЙ ПАЙПЛАЙН
    // =========================
    FRealState Pipeline::ApplyMorok(
        const TArray<FInventoryItem>& Inputs,
        const FRealState& CurrentBiomeState,
        const FEnvironment& Env,
        const FMemoryState& Memory,
        const FIntent& Intent,
        FRngState& Rng,
        float BiomeMorokField,
        float BiomeZaryanaField,
        const FVector4& BiomeAxisDrift)
    {
        // 1. Разделение ингредиентов
        TArray<FRealState> NonWater, Water;
        SeparateWaterAndIngredients(Inputs, NonWater, Water);

        // 2. Специальные случаи
        if (NonWater.Num() == 0 && Water.Num() > 0)
            return ProcessWaterOnly(Water);

        if (Water.Num() == 0)
            return ProcessNoWater();

        // 3. Агрегация не-воды и воды
        FRealState NonWaterAggregated = Fold(NonWater);
        FRealState WaterAggregated = AggregateWater(Water);

        // 4. Смешивание воды и не-воды
        FRealState Aggregated = BlendWaterAndNonWater(NonWaterAggregated, WaterAggregated, NonWater.Num(), Water.Num());

        UE_LOG(LogHerbalist, Warning, TEXT("[FOLD] Mag: %.3f | Dir: (%.2f, %.2f, %.2f, %.2f) | Dist:%.3f Stab:%.3f Pur:%.3f Pot:%.3f Res:%.3f Cor:%.3f"),
            Aggregated.Magnitude,
            Aggregated.Direction.Body, Aggregated.Direction.Mind,
            Aggregated.Direction.Spirit, Aggregated.Direction.Nature,
            Aggregated.Meta.Distortion, Aggregated.Meta.Stability, Aggregated.Meta.Purity,
            Aggregated.Meta.Potency, Aggregated.Meta.Resonance, Aggregated.Meta.Corruption);

        // 5. Дельта
        FRealState Delta = ComputeDelta(Aggregated, CurrentBiomeState);
        UE_LOG(LogHerbalist, Warning, TEXT("[DELTA] Mag: %.3f | Dir: (%.2f, %.2f, %.2f, %.2f) | Dist:%.3f Stab:%.3f Pur:%.3f Pot:%.3f Res:%.3f Cor:%.3f"),
            Delta.Magnitude,
            Delta.Direction.Body, Delta.Direction.Mind,
            Delta.Direction.Spirit, Delta.Direction.Nature,
            Delta.Meta.Distortion, Delta.Meta.Stability, Delta.Meta.Purity,
            Delta.Meta.Potency, Delta.Meta.Resonance, Delta.Meta.Corruption);

        // 6. Базовый Distortion и ZaryanaStrength
        float Distortion = ComputeBaseDistortion(Env, Memory);
        float ZaryanaStrength = Intent.Coherence * (1.0f - Distortion);

        // 7. Внедрение контекста биома
        ApplyBiomeContext(Distortion, ZaryanaStrength, Delta, BiomeMorokField, BiomeZaryanaField, BiomeAxisDrift);

        // 8. Влияние Intent на дельту
        float IntentFactor = 0.5f + Intent.Coherence;
        Delta.Magnitude *= IntentFactor;
        Delta.Meta.Stability *= IntentFactor;
        Delta.Meta.Purity *= IntentFactor;
        UE_LOG(LogHerbalist, Warning, TEXT("[INTENT] Factor=%.3f"), IntentFactor);

        // 9. Модификаторы от мета-параметров
        ApplyPotencyResonanceCorruption(Delta, Aggregated.Meta);

        // 10. Morok (нелинейное искажение)
        ApplyMorokDistortion(Delta, Distortion, Rng);

        // 11. Zaryana (структурирование)
        ApplyZaryanaStructuring(Delta, ZaryanaStrength, Distortion);

        // 12. Финальное состояние
        return FinalizeState(CurrentBiomeState, Delta);
    }

} // namespace HerbalistCore