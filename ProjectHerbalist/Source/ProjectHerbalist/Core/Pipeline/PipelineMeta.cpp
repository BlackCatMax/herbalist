// PipelineMeta.cpp
#include "PipelineMeta.h"
#include "Math/UnrealMathUtility.h"

namespace HerbalistCore
{
    FMeta BuildEnvironmentMeta(
        const FEnvironment& Env,
        const FMemoryState& Memory,
        float LocalDistortion,
        float ZaryanaStrength)
    {
        FMeta M;

        M.Distortion = LocalDistortion;
        M.Corruption = Env.Toxicity;

        M.Purity = 1.0f - Env.Toxicity;

        M.Stability = FMath::Clamp(
            (1.0f - Memory.AccumulatedDistortion) * 0.5f + ZaryanaStrength * 0.5f,
            0.0f, 1.0f);

        M.Potency = FMath::Clamp(
            Env.Fertility * 0.6f + Env.Moisture * 0.4f,
            0.0f, 1.0f);

        M.Resonance = FMath::Clamp(Memory.HistoryPurity, 0.0f, 1.0f);

        return M;
    }
}
