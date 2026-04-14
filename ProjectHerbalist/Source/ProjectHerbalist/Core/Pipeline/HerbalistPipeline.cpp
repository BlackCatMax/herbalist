#include "Core/Pipeline/HerbalistPipeline.h"
#include "Core/Types/HerbalistCoreTypes.h"
#include "Math/UnrealMathUtility.h"

DEFINE_LOG_CATEGORY(LogHerbalist);

namespace HerbalistCore
{
    // =========================
    // RNG (простой)
    // =========================

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
    // MAIN PIPELINE
    // =========================

    FRealState Pipeline::ApplyMorok(
        const FRealState& A,
        const FRealState& B,
        const FEnvironment& Env,
        const FMemoryState& Memory,
        const FIntent& Intent,
        FRngState& Rng)
    {
        FRealState Result;

        // =========================
        // BASE (FIX: направление НЕ пустое)
        // =========================

        Result.Magnitude = (A.Magnitude + B.Magnitude) * 0.5f;

        Result.Direction.Body = (A.Direction.Body + B.Direction.Body) * 0.5f;
        Result.Direction.Mind = (A.Direction.Mind + B.Direction.Mind) * 0.5f;
        Result.Direction.Spirit = (A.Direction.Spirit + B.Direction.Spirit) * 0.5f;
        Result.Direction.Nature = (A.Direction.Nature + B.Direction.Nature) * 0.5f;

        Result.Meta.Distortion = 0.0f;
        Result.Meta.Stability = 0.0f;

        UE_LOG(LogHerbalist, Warning, TEXT("[BASE] Mag: %.3f | Dist: %.3f"),
            Result.Magnitude, Result.Meta.Distortion);

        // =========================
        // ENV
        // =========================

        Result.Meta.Distortion += Env.Toxicity * 0.1f;

        UE_LOG(LogHerbalist, Warning, TEXT("[ENV] Tox: %.2f | Dist: %.3f"),
            Env.Toxicity, Result.Meta.Distortion);

        // =========================
        // MEMORY
        // =========================

        Result.Meta.Distortion += Memory.AccumulatedDistortion;

        UE_LOG(LogHerbalist, Warning, TEXT("[MEM] Dist: %.3f"),
            Result.Meta.Distortion);

        // =========================
        // INTENT
        // =========================

        UE_LOG(LogHerbalist, Warning, TEXT("[INTENT] Coh: %.2f"),
            Intent.Coherence);

        // =========================
        // ZARYANA (оставляем как есть, она корректная)
        // =========================

        float Base = Intent.Coherence;

        float DistortionResistance =
            1.0f / (1.0f + Result.Meta.Distortion);

        float StabilityBonus =
            Result.Meta.Stability * 0.3f;

        float ZaryanaStrength =
            Base * DistortionResistance;

        ZaryanaStrength += StabilityBonus * (1.0f - Base);

        ZaryanaStrength = FMath::Clamp(ZaryanaStrength, 0.0f, 1.0f);

        UE_LOG(LogHerbalist, Warning, TEXT("[ZARYANA] Strength: %.3f"),
            ZaryanaStrength);

        // =========================
        // MOROK (теперь искажает ОСНОВУ, а не ноль)
        // =========================

        float Noise =
            RandomRange(Rng, -1.0f, 1.0f) * Result.Meta.Distortion;

        float EffectiveNoise =
            Noise * (1.0f - ZaryanaStrength);

        Result.Direction.Body += EffectiveNoise * 0.2f;
        Result.Direction.Mind -= EffectiveNoise * 0.15f;
        Result.Direction.Spirit += EffectiveNoise * 0.1f;
        Result.Direction.Nature -= EffectiveNoise * 0.05f;

        UE_LOG(LogHerbalist, Warning,
            TEXT("[MOROK] Noise: %.3f | Eff: %.3f"),
            Noise, EffectiveNoise);

        // =========================
        // NORMALIZATION (FIXED)
        // =========================

        Result.Direction.Body = FMath::Max(0.0f, Result.Direction.Body);
        Result.Direction.Mind = FMath::Max(0.0f, Result.Direction.Mind);
        Result.Direction.Spirit = FMath::Max(0.0f, Result.Direction.Spirit);
        Result.Direction.Nature = FMath::Max(0.0f, Result.Direction.Nature);

        float Sum =
            Result.Direction.Body +
            Result.Direction.Mind +
            Result.Direction.Spirit +
            Result.Direction.Nature;

        if (Sum > KINDA_SMALL_NUMBER)
        {
            Result.Direction.Body /= Sum;
            Result.Direction.Mind /= Sum;
            Result.Direction.Spirit /= Sum;
            Result.Direction.Nature /= Sum;
        }
        else
        {
            Result.Direction.Body = 0.25f;
            Result.Direction.Mind = 0.25f;
            Result.Direction.Spirit = 0.25f;
            Result.Direction.Nature = 0.25f;
        }

        UE_LOG(LogHerbalist, Warning,
            TEXT("[NORM] Dir: B %.2f M %.2f S %.2f N %.2f"),
            Result.Direction.Body,
            Result.Direction.Mind,
            Result.Direction.Spirit,
            Result.Direction.Nature);

        // =========================
        // FEEDBACK (мягче, иначе всё схлопывается)
        // =========================

        float Feedback = 0.5f + Result.Magnitude * 0.5f;
        Result.Magnitude *= Feedback;

        UE_LOG(LogHerbalist, Warning,
            TEXT("[FEEDBACK] Mag: %.3f"),
            Result.Magnitude);

        // =========================
        // STRUCT (менее агрессивный)
        // =========================

        float Integrity =
            (1.0f - Result.Meta.Distortion * 0.5f) *
            (1.0f + Result.Meta.Stability);

        float StructureFactor =
            FMath::Clamp(Integrity, 0.1f, 1.0f);

        Result.Magnitude *= StructureFactor;

        UE_LOG(LogHerbalist, Warning,
            TEXT("[STRUCT] Integrity: %.3f | Factor: %.3f | Mag: %.3f"),
            Integrity, StructureFactor, Result.Magnitude);

        // =========================
        // FINAL
        // =========================

        UE_LOG(LogHerbalist, Warning,
            TEXT("[FINAL] Mag: %.3f"),
            Result.Magnitude);

        return Result;
    }
}