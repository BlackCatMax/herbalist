#include "HerbalistPipeline.h"
#include "ProjectHerbalist.h"
#include "Core/Types/HerbalistCoreTypes.h"
#include "Math/UnrealMathUtility.h"

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
    // FOLD (агрегация с весами)
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

        // Нормализация направления
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
    // CONTEXT (временный костыль, будет заменён на Harvest)
    // =========================
    void Pipeline::ApplyContext(FRealState& Aggregated, const FEnvironment& Env)
    {
        float k_axis = 1.0f - 0.3f * Env.Toxicity + 0.2f * Env.Fertility - 0.1f * Env.Moisture;
        float k_meta = 1.0f + 0.2f * Env.Toxicity - 0.2f * Env.Fertility + 0.1f * Env.Moisture;

        k_axis = FMath::Clamp(k_axis, 0.5f, 1.5f);
        k_meta = FMath::Clamp(k_meta, 0.5f, 1.5f);

        UE_LOG(LogHerbalist, Warning, TEXT("[CONTEXT] k_axis=%.3f, k_meta=%.3f (Tox=%.3f, Fert=%.3f, Moist=%.3f)"),
            k_axis, k_meta, Env.Toxicity, Env.Fertility, Env.Moisture);

        Aggregated.Direction.Body *= k_axis;
        Aggregated.Direction.Mind *= k_axis;
        Aggregated.Direction.Spirit *= k_axis;
        Aggregated.Direction.Nature *= k_axis;

        Aggregated.Meta.Distortion *= k_meta;
        Aggregated.Meta.Stability *= k_meta;
        Aggregated.Meta.Purity *= k_meta;

        // Повторная нормализация направления
        float Len = FMath::Sqrt(
            Aggregated.Direction.Body * Aggregated.Direction.Body +
            Aggregated.Direction.Mind * Aggregated.Direction.Mind +
            Aggregated.Direction.Spirit * Aggregated.Direction.Spirit +
            Aggregated.Direction.Nature * Aggregated.Direction.Nature
        );
        if (Len > KINDA_SMALL_NUMBER)
        {
            Aggregated.Direction.Body /= Len;
            Aggregated.Direction.Mind /= Len;
            Aggregated.Direction.Spirit /= Len;
            Aggregated.Direction.Nature /= Len;
        }
        else
        {
            Aggregated.Direction.Body = 0.25f;
            Aggregated.Direction.Mind = 0.25f;
            Aggregated.Direction.Spirit = 0.25f;
            Aggregated.Direction.Nature = 0.25f;
        }

        Aggregated.Meta.Distortion = FMath::Clamp(Aggregated.Meta.Distortion, 0.0f, 1.0f);
        Aggregated.Meta.Stability = FMath::Clamp(Aggregated.Meta.Stability, 0.0f, 1.0f);
        Aggregated.Meta.Purity = FMath::Clamp(Aggregated.Meta.Purity, 0.0f, 1.0f);
    }

    // =========================
    // MAIN PIPELINE (Fold → Context → Delta → Morok → Zaryana)
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

        // 2. ApplyContext (временный костыль)
        ApplyContext(Aggregated, Env);
        UE_LOG(LogHerbalist, Warning, TEXT("[AFTER_CONTEXT] Mag: %.3f | Dir: (%.2f, %.2f, %.2f, %.2f)"),
            Aggregated.Magnitude,
            Aggregated.Direction.Body, Aggregated.Direction.Mind,
            Aggregated.Direction.Spirit, Aggregated.Direction.Nature);

        // 3. ComputeDelta
        FRealState Delta = ComputeDelta(Aggregated, CurrentBiomeState);
        UE_LOG(LogHerbalist, Warning, TEXT("[DELTA] Mag: %.3f | Dir: (%.2f, %.2f, %.2f, %.2f)"),
            Delta.Magnitude,
            Delta.Direction.Body, Delta.Direction.Mind,
            Delta.Direction.Spirit, Delta.Direction.Nature);

        // 4. Distortion (среда + память)
        float EnvDist = Env.Toxicity * 0.1f;
        float MemoryDist = Memory.AccumulatedDistortion;
        float Distortion = 1.0f - (1.0f - EnvDist) * (1.0f - MemoryDist);
        Distortion = FMath::Clamp(Distortion, 0.0f, 0.95f);
        UE_LOG(LogHerbalist, Warning, TEXT("[DIST] Env: %.3f Mem: %.3f -> %.3f"),
            EnvDist, MemoryDist, Distortion);

        // 5. ZaryanaStrength (управляет Morok и Zaryana)
        float ZaryanaStrength = Intent.Coherence * (1.0f - Distortion);
        ZaryanaStrength = FMath::Clamp(ZaryanaStrength, 0.0f, 1.0f);
        UE_LOG(LogHerbalist, Warning, TEXT("[ZARYANA] Strength: %.3f"), ZaryanaStrength);

        // =========================
        // MOROK (нелинейное искажение)
        // =========================
        float NoiseMagnitude = RandomRange(Rng, 0.0f, Distortion);
        float NoiseDirection = RandomRange(Rng, -1.0f, 1.0f);
        float RawNoise = NoiseMagnitude * NoiseDirection;
        float EffectiveNoise = RawNoise * (1.0f - ZaryanaStrength);
        float Nonlinear = FMath::Tanh(EffectiveNoise * 2.0f);      // насыщение
        float MixStrength = Distortion * 0.5f;                     // сила перемешивания осей

        FRealState OriginalDelta = Delta;
        // Перекрёстное смешивание: body<->spirit, mind<->nature
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
        float Boost = 1.0f + ZaryanaStrength * 0.5f;      // максимум 1.5
        float Suppress = 1.0f - ZaryanaStrength * 0.3f;   // минимум 0.7

        Delta.Direction.Body = (Delta.Direction.Body > AvgDir) ? Delta.Direction.Body * Boost : Delta.Direction.Body * Suppress;
        Delta.Direction.Mind = (Delta.Direction.Mind > AvgDir) ? Delta.Direction.Mind * Boost : Delta.Direction.Mind * Suppress;
        Delta.Direction.Spirit = (Delta.Direction.Spirit > AvgDir) ? Delta.Direction.Spirit * Boost : Delta.Direction.Spirit * Suppress;
        Delta.Direction.Nature = (Delta.Direction.Nature > AvgDir) ? Delta.Direction.Nature * Boost : Delta.Direction.Nature * Suppress;

        float StabilityIncrease = ZaryanaStrength * (1.0f - Distortion) * 0.2f;
        float PurityIncrease = ZaryanaStrength * (1.0f - Distortion) * 0.15f;
        Delta.Meta.Stability += StabilityIncrease;
        Delta.Meta.Purity += PurityIncrease;
        Delta.Meta.Stability = FMath::Clamp(Delta.Meta.Stability, -1.0f, 1.0f);
        Delta.Meta.Purity = FMath::Clamp(Delta.Meta.Purity, -1.0f, 1.0f);

        // Нормализация направления после структурирования
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