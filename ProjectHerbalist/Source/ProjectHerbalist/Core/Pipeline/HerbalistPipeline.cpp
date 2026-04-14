#include "HerbalistPipeline.h"
#include "ProjectHerbalist.h"
#include "Core/Types/HerbalistCoreTypes.h"
#include "Math/UnrealMathUtility.h"

// никакого DEFINE_LOG_CATEGORY здесь

namespace HerbalistCore
{
    static float Random01(FRngState& Rng)
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

        float Weight = 1.0f;
        const float WeightDecay = 0.8f;
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
        }

        float Len = FMath::Sqrt(
            Accumulated.Direction.Body * Accumulated.Direction.Body +
            Accumulated.Direction.Mind * Accumulated.Direction.Mind +
            Accumulated.Direction.Spirit * Accumulated.Direction.Spirit +
            Accumulated.Direction.Nature * Accumulated.Direction.Nature
        );
        if (Len > KINDA_SMALL_NUMBER)
        {
            Accumulated.Direction.Body /= Len;
            Accumulated.Direction.Mind /= Len;
            Accumulated.Direction.Spirit /= Len;
            Accumulated.Direction.Nature /= Len;
        }
        else
        {
            Accumulated.Direction.Body = 0.5f;
            Accumulated.Direction.Mind = 0.5f;
            Accumulated.Direction.Spirit = 0.5f;
            Accumulated.Direction.Nature = 0.5f;
        }

        Accumulated.Magnitude = FMath::Clamp(Accumulated.Magnitude, 0.0f, 1.0f);
        Accumulated.Meta.Distortion = FMath::Clamp(Accumulated.Meta.Distortion, 0.0f, 1.0f);
        Accumulated.Meta.Stability = FMath::Clamp(Accumulated.Meta.Stability, 0.0f, 1.0f);
        Accumulated.Meta.Purity = FMath::Clamp(Accumulated.Meta.Purity, 0.0f, 1.0f);

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

        return Delta;
    }

    // =========================
    // CONTEXT
    // =========================
    void Pipeline::ApplyContext(FRealState& Delta, const FEnvironment& Env)
    {
        float k_axis = 1.0f - 0.3f * Env.Toxicity + 0.2f * Env.Fertility - 0.1f * Env.Moisture;
        float k_meta = 1.0f + 0.2f * Env.Toxicity - 0.2f * Env.Fertility + 0.1f * Env.Moisture;

        k_axis = FMath::Clamp(k_axis, 0.5f, 1.5f);
        k_meta = FMath::Clamp(k_meta, 0.5f, 1.5f);

        UE_LOG(LogHerbalist, Warning, TEXT("[CONTEXT] k_axis=%.3f, k_meta=%.3f (Tox=%.3f, Fert=%.3f, Moist=%.3f)"),
            k_axis, k_meta, Env.Toxicity, Env.Fertility, Env.Moisture);

        Delta.Direction.Body *= k_axis;
        Delta.Direction.Mind *= k_axis;
        Delta.Direction.Spirit *= k_axis;
        Delta.Direction.Nature *= k_axis;

        Delta.Meta.Distortion *= k_meta;
        Delta.Meta.Stability *= k_meta;
        Delta.Meta.Purity *= k_meta;
    }

    // =========================
    // MAIN PIPELINE
    // =========================
    FRealState Pipeline::ApplyMorok(
        const TArray<FRealState>& Inputs,
        const FRealState& CurrentBiomeState,
        const FEnvironment& Env,
        const FMemoryState& Memory,
        const FIntent& Intent,
        FRngState& Rng)
    {
        FRealState Aggregated = Fold(Inputs);
        UE_LOG(LogHerbalist, Warning, TEXT("[FOLD] Mag: %.3f | Dir: (%.2f, %.2f, %.2f, %.2f)"),
            Aggregated.Magnitude,
            Aggregated.Direction.Body, Aggregated.Direction.Mind,
            Aggregated.Direction.Spirit, Aggregated.Direction.Nature);

        FRealState Delta = ComputeDelta(Aggregated, CurrentBiomeState);
        UE_LOG(LogHerbalist, Warning, TEXT("[DELTA] Mag: %.3f | Dir: (%.2f, %.2f, %.2f, %.2f)"),
            Delta.Magnitude,
            Delta.Direction.Body, Delta.Direction.Mind,
            Delta.Direction.Spirit, Delta.Direction.Nature);

        ApplyContext(Delta, Env);

        float EnvDist = Env.Toxicity * 0.1f;
        float MemoryDist = Memory.AccumulatedDistortion;
        float Distortion = 1.0f - (1.0f - EnvDist) * (1.0f - MemoryDist);
        Distortion = FMath::Clamp(Distortion, 0.0f, 0.95f);
        UE_LOG(LogHerbalist, Warning, TEXT("[DIST] Env: %.3f Mem: %.3f -> %.3f"),
            EnvDist, MemoryDist, Distortion);

        float ZaryanaStrength = Intent.Coherence * (1.0f - Distortion);
        ZaryanaStrength = FMath::Clamp(ZaryanaStrength, 0.0f, 1.0f);
        UE_LOG(LogHerbalist, Warning, TEXT("[ZARYANA] Strength: %.3f"), ZaryanaStrength);

        float Noise = RandomRange(Rng, -1.0f, 1.0f) * Distortion;
        float EffectiveNoise = Noise * (1.0f - ZaryanaStrength);
        float Chaos = EffectiveNoise;

        Delta.Direction.Body = FMath::Lerp(Delta.Direction.Body, Chaos, 0.2f);
        Delta.Direction.Mind = FMath::Lerp(Delta.Direction.Mind, -Chaos, 0.15f);
        Delta.Direction.Spirit = FMath::Lerp(Delta.Direction.Spirit, Chaos, 0.1f);
        Delta.Direction.Nature = FMath::Lerp(Delta.Direction.Nature, -Chaos, 0.05f);

        UE_LOG(LogHerbalist, Warning, TEXT("[MOROK] Noise: %.3f | Eff: %.3f | Chaos: %.3f"),
            Noise, EffectiveNoise, Chaos);

        FRealState NewState = CurrentBiomeState;
        NewState.Direction.Body += Delta.Direction.Body;
        NewState.Direction.Mind += Delta.Direction.Mind;
        NewState.Direction.Spirit += Delta.Direction.Spirit;
        NewState.Direction.Nature += Delta.Direction.Nature;
        NewState.Magnitude += Delta.Magnitude;
        NewState.Meta.Distortion += Delta.Meta.Distortion;
        NewState.Meta.Stability += Delta.Meta.Stability;
        NewState.Meta.Purity += Delta.Meta.Purity;

        float Len = FMath::Sqrt(
            NewState.Direction.Body * NewState.Direction.Body +
            NewState.Direction.Mind * NewState.Direction.Mind +
            NewState.Direction.Spirit * NewState.Direction.Spirit +
            NewState.Direction.Nature * NewState.Direction.Nature
        );
        if (Len > KINDA_SMALL_NUMBER)
        {
            NewState.Direction.Body /= Len;
            NewState.Direction.Mind /= Len;
            NewState.Direction.Spirit /= Len;
            NewState.Direction.Nature /= Len;
        }
        else
        {
            NewState.Direction.Body = 0.25f;
            NewState.Direction.Mind = 0.25f;
            NewState.Direction.Spirit = 0.25f;
            NewState.Direction.Nature = 0.25f;
        }

        NewState.Magnitude = FMath::Clamp(NewState.Magnitude, 0.0f, 1.0f);
        NewState.Meta.Distortion = FMath::Clamp(NewState.Meta.Distortion, 0.0f, 1.0f);
        NewState.Meta.Stability = FMath::Clamp(NewState.Meta.Stability, 0.0f, 1.0f);
        NewState.Meta.Purity = FMath::Clamp(NewState.Meta.Purity, 0.0f, 1.0f);

        float Integrity = (1.0f - NewState.Meta.Distortion * 0.7f) * (1.0f + NewState.Meta.Stability);
        float StructureFactor = FMath::Clamp(Integrity, 0.0f, 1.0f);
        NewState.Magnitude *= StructureFactor;

        UE_LOG(LogHerbalist, Warning, TEXT("[NEW_STATE] Mag: %.3f | Dir: (%.2f, %.2f, %.2f, %.2f)"),
            NewState.Magnitude,
            NewState.Direction.Body, NewState.Direction.Mind,
            NewState.Direction.Spirit, NewState.Direction.Nature);

        return NewState;
    }
}