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

        // 1. Прямые отражения среды
        M.Distortion = LocalDistortion;      // будет использован только в Direction-дрейфе, не в Meta
        M.Corruption = Env.Toxicity;         // токсичность прямо отображается в скверну

        // 2. Purity – структурная чистота среды, не просто 1 - Toxicity
        M.Purity = 1.0f - Env.Toxicity;     // базовая инверсия, без памяти чтобы не задваивать

        // 3. Stability – комбинация памяти и Zaryana
        M.Stability = FMath::Clamp(
            (1.0f - Memory.AccumulatedDistortion) * 0.5f + ZaryanaStrength * 0.5f,
            0.0f, 1.0f);

        // 4. Усиливающие параметры
        M.Potency = FMath::Clamp(
            Env.Fertility * 0.6f + Env.Moisture * 0.4f,
            0.0f, 1.0f);

        M.Resonance = FMath::Clamp(Memory.HistoryPurity, 0.0f, 1.0f);

        return M;
    }
}