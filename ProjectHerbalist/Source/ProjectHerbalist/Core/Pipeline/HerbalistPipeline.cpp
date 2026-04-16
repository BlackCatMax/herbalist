// HerbalistPipeline.cpp
#include "HerbalistPipeline.h"
#include "ProjectHerbalist.h"
#include "Core/Types/HerbalistCoreTypes.h"
#include "Math/UnrealMathUtility.h"

namespace HerbalistCore
{
    // Реализация Random01 (не static)
    float Random01(FRngState& Rng)
    {
        Rng.Seed = (Rng.Seed * 196314165) + 907633515;
        return (Rng.Seed & 0x00FFFFFF) / float(0x01000000);
    }

    // Вспомогательная функция RandomRange (static, используется только внутри)
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

        // Нормализация направления по сумме
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
        UE_LOG(LogHerbalist, Warning, TEXT("[FOLD] Mag: %.3f | Dir: (%.2f, %.2f, %.2f, %.2f) | Dist:%.3f Stab:%.3f Pur:%.3f Pot:%.3f Res:%.3f Cor:%.3f"),
            Aggregated.Magnitude,
            Aggregated.Direction.Body, Aggregated.Direction.Mind,
            Aggregated.Direction.Spirit, Aggregated.Direction.Nature,
            Aggregated.Meta.Distortion, Aggregated.Meta.Stability, Aggregated.Meta.Purity,
            Aggregated.Meta.Potency, Aggregated.Meta.Resonance, Aggregated.Meta.Corruption);

        // 2. ComputeDelta
        FRealState Delta = ComputeDelta(Aggregated, CurrentBiomeState);
        UE_LOG(LogHerbalist, Warning, TEXT("[DELTA] Mag: %.3f | Dir: (%.2f, %.2f, %.2f, %.2f) | Dist:%.3f Stab:%.3f Pur:%.3f Pot:%.3f Res:%.3f Cor:%.3f"),
            Delta.Magnitude,
            Delta.Direction.Body, Delta.Direction.Mind,
            Delta.Direction.Spirit, Delta.Direction.Nature,
            Delta.Meta.Distortion, Delta.Meta.Stability, Delta.Meta.Purity,
            Delta.Meta.Potency, Delta.Meta.Resonance, Delta.Meta.Corruption);

        // 3. Distortion (среда + память)
        float EnvDist = Env.Toxicity * 0.5f;
        float MemoryDist = Memory.AccumulatedDistortion;
        float Distortion = 1.0f - (1.0f - EnvDist) * (1.0f - MemoryDist);
        Distortion = FMath::Clamp(Distortion, 0.0f, 0.95f);
        UE_LOG(LogHerbalist, Warning, TEXT("[DIST] Env: %.3f Mem: %.3f -> %.3f"), EnvDist, MemoryDist, Distortion);

        // 4. ZaryanaStrength
        float ZaryanaStrength = Intent.Coherence * (1.0f - Distortion);
        ZaryanaStrength = FMath::Clamp(ZaryanaStrength, 0.0f, 1.0f);
        UE_LOG(LogHerbalist, Warning, TEXT("[ZARYANA] Strength: %.3f"), ZaryanaStrength);

        // 5. Влияние Intent на Delta
        float IntentFactor = 0.5f + Intent.Coherence;
        Delta.Magnitude *= IntentFactor;
        Delta.Meta.Stability *= IntentFactor;
        Delta.Meta.Purity *= IntentFactor;
        UE_LOG(LogHerbalist, Warning, TEXT("[INTENT] Factor=%.3f"), IntentFactor);

        // 6. Масштабирование от Potency, Resonance, Corruption
        float PotencyScale = 1.0f + (Aggregated.Meta.Potency - 0.5f) * 0.5f;
        Delta.Magnitude *= PotencyScale;
        Delta.Meta.Distortion *= PotencyScale;
        Delta.Meta.Stability *= PotencyScale;
        Delta.Meta.Purity *= PotencyScale;
        Delta.Meta.Potency *= PotencyScale;
        Delta.Meta.Resonance *= PotencyScale;
        Delta.Meta.Corruption *= PotencyScale;

        float ResonanceFactor = 1.0f + Aggregated.Meta.Resonance * 0.5f;
        Delta.Direction.Body = Delta.Direction.Body > 0 ? Delta.Direction.Body * ResonanceFactor : Delta.Direction.Body;
        Delta.Direction.Mind = Delta.Direction.Mind > 0 ? Delta.Direction.Mind * ResonanceFactor : Delta.Direction.Mind;
        Delta.Direction.Spirit = Delta.Direction.Spirit > 0 ? Delta.Direction.Spirit * ResonanceFactor : Delta.Direction.Spirit;
        Delta.Direction.Nature = Delta.Direction.Nature > 0 ? Delta.Direction.Nature * ResonanceFactor : Delta.Direction.Nature;

        Delta.Meta.Distortion += Aggregated.Meta.Corruption * 0.1f;
        Delta.Meta.Stability -= Aggregated.Meta.Corruption * 0.05f;
        Delta.Meta.Purity -= Aggregated.Meta.Corruption * 0.05f;
        Delta.Meta.Corruption += Aggregated.Meta.Corruption * 0.05f;

        UE_LOG(LogHerbalist, Warning, TEXT("[POTENCY] Scale=%.3f | [RESONANCE] Factor=%.3f | [CORRUPTION] AddDist=%.3f"),
            PotencyScale, ResonanceFactor, Aggregated.Meta.Corruption * 0.1f);

        // 7. Morok (нелинейное искажение)
        float NoiseMagnitude = RandomRange(Rng, 0.0f, Distortion);
        float NoiseDirection = RandomRange(Rng, -1.0f, 1.0f);
        float RawNoise = NoiseMagnitude * NoiseDirection;
        float Nonlinear = FMath::Tanh(RawNoise * 2.0f);
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

        // 8. Zaryana (структурирование)
        float AvgDir = (Delta.Direction.Body + Delta.Direction.Mind +
            Delta.Direction.Spirit + Delta.Direction.Nature) / 4.0f;
        float Boost = 1.0f + ZaryanaStrength * 0.5f;
        float Suppress = 1.0f - ZaryanaStrength * 0.3f;

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

        // Нормализация дельты направления
        Delta.Direction.NormalizeSum();

        UE_LOG(LogHerbalist, Warning, TEXT("[ZARYANA_STRUCT] Boost: %.2f, Suppress: %.2f, Stability+%.3f, Purity+%.3f"),
            Boost, Suppress, StabilityIncrease, PurityIncrease);

        // 9. Применение дельты с интерполяцией направления
        FRealState NewState = CurrentBiomeState;

        // Интерполяция направления: целевой вектор = текущий + Delta (с последующей нормализацией)
        FDirection NewDir;
        NewDir.Body = NewState.Direction.Body + Delta.Direction.Body;
        NewDir.Mind = NewState.Direction.Mind + Delta.Direction.Mind;
        NewDir.Spirit = NewState.Direction.Spirit + Delta.Direction.Spirit;
        NewDir.Nature = NewState.Direction.Nature + Delta.Direction.Nature;
        NewDir.NormalizeSum();

        // Смешивание с исходным направлением в зависимости от силы изменения
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

        // 10. Нормализация и клиппинг
        NewState.Direction.NormalizeSum();

        NewState.Magnitude = FMath::Clamp(NewState.Magnitude, 0.0f, 1.0f);
        NewState.Meta.Distortion = FMath::Clamp(NewState.Meta.Distortion, 0.0f, 1.0f);
        NewState.Meta.Stability = FMath::Clamp(NewState.Meta.Stability, 0.0f, 1.0f);
        NewState.Meta.Purity = FMath::Clamp(NewState.Meta.Purity, 0.0f, 1.0f);
        NewState.Meta.Potency = FMath::Clamp(NewState.Meta.Potency, 0.0f, 1.0f);
        NewState.Meta.Resonance = FMath::Clamp(NewState.Meta.Resonance, 0.0f, 1.0f);
        NewState.Meta.Corruption = FMath::Clamp(NewState.Meta.Corruption, 0.0f, 1.0f);

        // 11. Структурный фактор
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
}