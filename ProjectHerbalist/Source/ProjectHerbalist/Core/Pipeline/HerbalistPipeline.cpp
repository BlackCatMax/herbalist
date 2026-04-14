#include "Core/Pipeline/HerbalistPipeline.h"
#include "Core/Types/HerbalistCoreTypes.h"
#include "Math/UnrealMathUtility.h"

DEFINE_LOG_CATEGORY(LogHerbalist);

namespace HerbalistCore
{
    // =========================
    // RNG
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
        // BASE
        // =========================

        Result.Magnitude = (A.Magnitude + B.Magnitude) * 0.5f;
        Result.Meta.Distortion = 0.0f;

        // ВАЖНО: теперь используем память как источник стабильности
        Result.Meta.Stability = Memory.StabilityMemory;

        Result.Direction.Body = (A.Direction.Body + B.Direction.Body) * 0.5f;
        Result.Direction.Mind = (A.Direction.Mind + B.Direction.Mind) * 0.5f;
        Result.Direction.Spirit = (A.Direction.Spirit + B.Direction.Spirit) * 0.5f;
        Result.Direction.Nature = (A.Direction.Nature + B.Direction.Nature) * 0.5f;

        UE_LOG(LogHerbalist, Warning, TEXT("[BASE] Mag: %.3f | Dist: %.3f"),
            Result.Magnitude, Result.Meta.Distortion);

        // =========================
        // DISTORTION (NON-LINEAR, CLAMPED)
        // =========================

        float EnvDist = Env.Toxicity * 0.1f;
        float MemoryDist = Memory.AccumulatedDistortion;

        Result.Meta.Distortion =
            1.0f - (1.0f - EnvDist) * (1.0f - MemoryDist);

        // ❗ не даём уйти в полный максимум
        Result.Meta.Distortion = FMath::Clamp(Result.Meta.Distortion, 0.0f, 0.95f);

        UE_LOG(LogHerbalist, Warning, TEXT("[DIST] Env: %.3f Mem: %.3f -> %.3f"),
            EnvDist, MemoryDist, Result.Meta.Distortion);

        // =========================
        // INTENT
        // =========================

        UE_LOG(LogHerbalist, Warning, TEXT("[INTENT] Coh: %.2f"),
            Intent.Coherence);

        // =========================
        // ZARYANA (STABLE)
        // =========================

        float ZaryanaStrength =
            Intent.Coherence *
            (1.0f - Result.Meta.Distortion);

        ZaryanaStrength = FMath::Clamp(ZaryanaStrength, 0.0f, 1.0f);

        UE_LOG(LogHerbalist, Warning, TEXT("[ZARYANA] Strength: %.3f"),
            ZaryanaStrength);

        // =========================
        // MOROK (SIGNED CHAOS)
        // =========================

        float Noise =
            RandomRange(Rng, -1.0f, 1.0f) * Result.Meta.Distortion;

        float EffectiveNoise =
            Noise * (1.0f - ZaryanaStrength);

        float Chaos = EffectiveNoise; // ❗ сохраняем знак

        // ❗ деформация, а не просто усиление
        Result.Direction.Body = FMath::Lerp(Result.Direction.Body, Chaos, 0.2f);
        Result.Direction.Mind = FMath::Lerp(Result.Direction.Mind, -Chaos, 0.15f);
        Result.Direction.Spirit = FMath::Lerp(Result.Direction.Spirit, Chaos, 0.1f);
        Result.Direction.Nature = FMath::Lerp(Result.Direction.Nature, -Chaos, 0.05f);

        UE_LOG(LogHerbalist, Warning,
            TEXT("[MOROK] Noise: %.3f | Eff: %.3f | Chaos: %.3f"),
            Noise, EffectiveNoise, Chaos);

        // =========================
        // NORMALIZATION (SAFE)
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
        // FEEDBACK (SOFT)
        // =========================

        float Feedback =
            0.5f + (Result.Magnitude * 0.5f);

        Result.Magnitude *= Feedback;

        UE_LOG(LogHerbalist, Warning,
            TEXT("[FEEDBACK] Mag: %.3f"),
            Result.Magnitude);

        // =========================
        // STRUCT (NOW WORKS)
        // =========================

        float Integrity =
            (1.0f - Result.Meta.Distortion * 0.7f) *
            (1.0f + Result.Meta.Stability);

        float StructureFactor =
            FMath::Clamp(Integrity, 0.0f, 1.0f);

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