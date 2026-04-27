// Core/Pipeline/PipelineMeta.cpp
#include "PipelineMeta.h"
#include "Math/UnrealMathUtility.h"

namespace HerbalistCore
{
    FMeta BuildEnvironmentMeta(const FEnvironment& Env, const FMemoryState& Memory)
    {
        FMeta Result;
        // Токсичность увеличивает искажение и скверну, снижает чистоту и стабильность
        Result.Distortion = FMath::Clamp(Env.Toxicity * 0.5f + Memory.AccumulatedDistortion * 0.5f, 0.0f, 1.0f);
        Result.Stability   = FMath::Clamp(1.0f - Env.Toxicity, 0.0f, 1.0f);
        Result.Purity      = FMath::Clamp(1.0f - Env.Toxicity * 0.3f, 0.0f, 1.0f);
        Result.Potency     = FMath::Clamp(Env.Fertility * 0.2f + Env.Moisture * 0.1f, 0.0f, 1.0f);
        Result.Resonance   = 0.0f;
        Result.Corruption  = FMath::Clamp(Env.Toxicity * 0.3f, 0.0f, 1.0f);
        return Result;
    }

    FMeta BlendMeta(const FMeta& A, const FMeta& B, float Alpha)
    {
        Alpha = FMath::Clamp(Alpha, 0.0f, 1.0f);
        FMeta Out;
        Out.Distortion = FMath::Lerp(A.Distortion, B.Distortion, Alpha);
        Out.Stability   = FMath::Lerp(A.Stability,   B.Stability,   Alpha);
        Out.Purity      = FMath::Lerp(A.Purity,      B.Purity,      Alpha);
        Out.Potency     = FMath::Lerp(A.Potency,     B.Potency,     Alpha);
        Out.Resonance   = FMath::Lerp(A.Resonance,   B.Resonance,   Alpha);
        Out.Corruption  = FMath::Lerp(A.Corruption,  B.Corruption,  Alpha);
        return Out;
    }
}