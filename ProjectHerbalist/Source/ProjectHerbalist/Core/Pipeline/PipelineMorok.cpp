// PipelineMorok.cpp
#include "ProjectHerbalist.h"
#include "HerbalistPipeline.h"
#include "PipelineTypes.h"
#include "Core/HerbalistSettings.h"
#include "Core/Types/HerbalistCoreMath.h"
#include "Math/UnrealMathUtility.h"

namespace HerbalistCore
{
    // =========================================================================
    // L1 Morok Distortion (старая версия — свап осей + шум)
    // =========================================================================

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

        UE_LOG(LogHerbalist, Verbose, TEXT("[MOROK L1] Noise: %.3f -> Nonlinear: %.3f | MixStrength: %.3f"),
            RawNoise, Nonlinear, MixStrength);
    }

    // =========================================================================
    // L2 Morok Matrix (новая версия — матричное искажение)
    // =========================================================================

    void ApplyMorokMatrix(FL2Direction& Dir, float Distortion, FRngState& Rng)
    {
        float s = Distortion;
        float mix = s * 0.7f;                // Сила смешивания осей (усилена для заметности)
        float rand = RandomRange(Rng, -1.0f, 1.0f);
        float k = rand * s * 0.5f;            // Skew-фактор (асимметрия)

        const float B = Dir.Body;
        const float M = Dir.Mind;
        const float S = Dir.Spirit;
        const float N = Dir.Nature;

        // Матрица 4x4 явно:
        float newBody   = (1.0f - mix) * B + k * M + mix * S;
        float newMind   = -k * B + (1.0f - mix) * M + mix * N;
        float newSpirit = mix * B + (1.0f - mix) * S + k * N;
        float newNature = mix * M - k * S + (1.0f - mix) * N;

        Dir.Body   = newBody;
        Dir.Mind   = newMind;
        Dir.Spirit = newSpirit;
        Dir.Nature = newNature;

        // Масштабирование длины для усиления эффекта Distortion
        float lengthScale = 1.0f + s * 0.5f;
        Dir.Body   *= lengthScale;
        Dir.Mind   *= lengthScale;
        Dir.Spirit *= lengthScale;
        Dir.Nature *= lengthScale;

        // Мягкое ограничение длины, чтобы не уходить в бесконечность (макс. длина 2.0)
        float LenSq = Dir.Body*Dir.Body + Dir.Mind*Dir.Mind + Dir.Spirit*Dir.Spirit + Dir.Nature*Dir.Nature;
        const float MaxLenSq = 4.0f; // 2.0^2
        if (LenSq > MaxLenSq)
        {
            float InvScale = FMath::Sqrt(MaxLenSq / LenSq);
            Dir.Body   *= InvScale;
            Dir.Mind   *= InvScale;
            Dir.Spirit *= InvScale;
            Dir.Nature *= InvScale;
        }

        UE_LOG(LogHerbalist, Log, TEXT("[MOROK L2] s=%.3f mix=%.3f k=%.3f | Dir=(%.3f, %.3f, %.3f, %.3f)"),
            s, mix, k, Dir.Body, Dir.Mind, Dir.Spirit, Dir.Nature);
    }

} // namespace HerbalistCore