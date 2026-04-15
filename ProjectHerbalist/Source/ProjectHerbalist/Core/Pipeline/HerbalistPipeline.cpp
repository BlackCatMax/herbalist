#include "HerbalistPipeline.h"
#include "ProjectHerbalist.h"
#include "Core/Types/HerbalistCoreTypes.h"
#include "Math/UnrealMathUtility.h"

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
        // 1. Fold
        FRealState Aggregated = Fold(Inputs);
        UE_LOG(LogHerbalist, Warning, TEXT("[FOLD] Mag: %.3f | Dir: (%.2f, %.2f, %.2f, %.2f)"),
            Aggregated.Magnitude,
            Aggregated.Direction.Body, Aggregated.Direction.Mind,
            Aggregated.Direction.Spirit, Aggregated.Direction.Nature);

        // 2. ComputeDelta
        FRealState Delta = ComputeDelta(Aggregated, CurrentBiomeState);
        UE_LOG(LogHerbalist, Warning, TEXT("[DELTA] Mag: %.3f | Dir: (%.2f, %.2f, %.2f, %.2f)"),
            Delta.Magnitude,
            Delta.Direction.Body, Delta.Direction.Mind,
            Delta.Direction.Spirit, Delta.Direction.Nature);

        // 3. Distortion (среда + память)
        float EnvDist = Env.Toxicity * 0.5f; // увеличено для более сильного влияния среды, было 0.2f
        float MemoryDist = Memory.AccumulatedDistortion;
        float Distortion = 1.0f - (1.0f - EnvDist) * (1.0f - MemoryDist);
        Distortion = FMath::Clamp(Distortion, 0.0f, 0.95f);
        UE_LOG(LogHerbalist, Warning, TEXT("[DIST] Env: %.3f Mem: %.3f -> %.3f"),
            EnvDist, MemoryDist, Distortion);

        // =========================
        // БИФУРКАЦИЯ (катастрофа при высоком искажении)
        // =========================
        float BifurcationThreshold = 0.85f;
        if (Distortion > BifurcationThreshold)
        {
            float EventChance = FMath::Clamp((Distortion - BifurcationThreshold) / 0.15f, 0.0f, 1.0f);
            EventChance *= (1.0f - Memory.StabilityMemory); // нестабильность повышает шанс
            if (Random01(Rng) < EventChance)
            {
                bool bCollapse = Random01(Rng) < 0.5f;
                if (bCollapse)
                {
                    Distortion = 0.2f;
                    Delta.Meta.Stability = -0.5f;
                    Delta.Meta.Purity = -0.3f;
                    UE_LOG(LogHerbalist, Warning, TEXT("[CATASTROPHE] COLLAPSE! Distortion reset to 0.2"));
                }
                else
                {
                    Distortion = 0.4f;
                    Delta.Meta.Stability += 0.4f;
                    Delta.Meta.Purity += 0.3f;
                    UE_LOG(LogHerbalist, Warning, TEXT("[CATASTROPHE] PURIFICATION! Distortion reset to 0.4"));
                }
                Delta.Meta.Stability = FMath::Clamp(Delta.Meta.Stability, -1.0f, 1.0f);
                Delta.Meta.Purity = FMath::Clamp(Delta.Meta.Purity, -1.0f, 1.0f);
            }
        }

        // 4. ZaryanaStrength (управляет Morok и Zaryana)
        float ZaryanaStrength = Intent.Coherence * (1.0f - Distortion);
        ZaryanaStrength = FMath::Clamp(ZaryanaStrength, 0.0f, 1.0f);

        // 4.1 Нелинейное усиление Zaryana при высоком Distortion (помогает выйти из плато)
        float NonlinearZaryana = ZaryanaStrength;
        if (Distortion > 0.7f)
        {
            NonlinearZaryana = ZaryanaStrength * (1.0f + (Distortion - 0.7f) / 0.3f);
            NonlinearZaryana = FMath::Clamp(NonlinearZaryana, 0.0f, 1.0f);
            UE_LOG(LogHerbalist, Verbose, TEXT("[ZARYANA] Nonlinear: %.3f -> %.3f"), ZaryanaStrength, NonlinearZaryana);
        }
        UE_LOG(LogHerbalist, Warning, TEXT("[ZARYANA] Strength: %.3f (Nonlinear: %.3f)"), ZaryanaStrength, NonlinearZaryana);

        // =========================
        // MOROK (нелинейное искажение)
        // =========================
        float NoiseMagnitude = RandomRange(Rng, 0.0f, Distortion);
        float NoiseDirection = RandomRange(Rng, -1.0f, 1.0f);
        float RawNoise = NoiseMagnitude * NoiseDirection;
        float EffectiveNoise = RawNoise * (1.0f - NonlinearZaryana);
        float Nonlinear = FMath::Tanh(EffectiveNoise * 2.0f);
        float MixStrength = Distortion * 0.5f;

        FRealState OriginalDelta = Delta;
        Delta.Direction.Body = OriginalDelta.Direction.Body * (1.0f - MixStrength) +
            OriginalDelta.Direction.Spirit * MixStrength +
            Nonlinear * 0.1f;
        Delta.Direction.Spirit = OriginalDelta.Direction.Spirit * (1.0f - MixStrength) +
            OriginalDelta.Direction.Body * MixStrength +
            Nonlinear * 0.1f;
        Delta.Direction.Mind = OriginalDelta.Direction.Mind * (1.0f - MixStrength) +
            OriginalDelta.Direction.Nature * MixStrength +
            Nonlinear * 0.1f;
        Delta.Direction.Nature = OriginalDelta.Direction.Nature * (1.0f - MixStrength) +
            OriginalDelta.Direction.Mind * MixStrength +
            Nonlinear * 0.1f;

        Delta.Magnitude = Delta.Magnitude * (1.0f + Nonlinear * 0.2f);
        Delta.Magnitude = FMath::Clamp(Delta.Magnitude, -1.0f, 1.0f);

        UE_LOG(LogHerbalist, Warning, TEXT("[MOROK] Noise: %.3f -> Nonlinear: %.3f | MixStrength: %.3f"),
            RawNoise, Nonlinear, MixStrength);

        // =========================
        // ZARYANA (структурирование)
        // =========================
        float AvgDir = (Delta.Direction.Body + Delta.Direction.Mind +
            Delta.Direction.Spirit + Delta.Direction.Nature) / 4.0f;
        float Boost = 1.0f + NonlinearZaryana * 0.5f;
        float Suppress = 1.0f - NonlinearZaryana * 0.3f;

        Delta.Direction.Body = (Delta.Direction.Body > AvgDir) ? Delta.Direction.Body * Boost : Delta.Direction.Body * Suppress;
        Delta.Direction.Mind = (Delta.Direction.Mind > AvgDir) ? Delta.Direction.Mind * Boost : Delta.Direction.Mind * Suppress;
        Delta.Direction.Spirit = (Delta.Direction.Spirit > AvgDir) ? Delta.Direction.Spirit * Boost : Delta.Direction.Spirit * Suppress;
        Delta.Direction.Nature = (Delta.Direction.Nature > AvgDir) ? Delta.Direction.Nature * Boost : Delta.Direction.Nature * Suppress;

        float StabilityIncrease = NonlinearZaryana * (1.0f - Distortion) * 0.2f;
        float PurityIncrease = NonlinearZaryana * (1.0f - Distortion) * 0.15f;
        Delta.Meta.Stability += StabilityIncrease;
        Delta.Meta.Purity += PurityIncrease;
        Delta.Meta.Stability = FMath::Clamp(Delta.Meta.Stability, -1.0f, 1.0f);
        Delta.Meta.Purity = FMath::Clamp(Delta.Meta.Purity, -1.0f, 1.0f);

        float LenZ = FMath::Sqrt(
            Delta.Direction.Body * Delta.Direction.Body +
            Delta.Direction.Mind * Delta.Direction.Mind +
            Delta.Direction.Spirit * Delta.Direction.Spirit +
            Delta.Direction.Nature * Delta.Direction.Nature
        );
        if (LenZ > KINDA_SMALL_NUMBER)
        {
            Delta.Direction.Body /= LenZ;
            Delta.Direction.Mind /= LenZ;
            Delta.Direction.Spirit /= LenZ;
            Delta.Direction.Nature /= LenZ;
        }

        UE_LOG(LogHerbalist, Warning, TEXT("[ZARYANA_STRUCT] Boost: %.2f, Suppress: %.2f, Stability+%.3f, Purity+%.3f"),
            Boost, Suppress, StabilityIncrease, PurityIncrease);

        // =========================
        // ПРИМЕНЕНИЕ ДЕЛЬТЫ
        // =========================
        FRealState NewState = CurrentBiomeState;
        NewState.Direction.Body += Delta.Direction.Body;
        NewState.Direction.Mind += Delta.Direction.Mind;
        NewState.Direction.Spirit += Delta.Direction.Spirit;
        NewState.Direction.Nature += Delta.Direction.Nature;
        NewState.Magnitude += Delta.Magnitude;
        NewState.Meta.Distortion += Delta.Meta.Distortion;
        NewState.Meta.Stability += Delta.Meta.Stability;
        NewState.Meta.Purity += Delta.Meta.Purity;

        // =========================
        // НОРМАЛИЗАЦИЯ И КЛИППИНГ
        // =========================
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

        // =========================
        // СТРУКТУРНЫЙ ФАКТОР
        // =========================
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