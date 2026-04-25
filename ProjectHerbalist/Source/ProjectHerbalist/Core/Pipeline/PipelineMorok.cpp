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
    // Morok — матричное искажение
    // =========================================================================

    void ApplyMorokDistortion(FL2Direction& Dir, float Distortion, FRngState& Rng)
    {
        float s = Distortion;
        float mix = s * 0.7f;
        float rand = RandomRange(Rng, -1.0f, 1.0f);
        float k = rand * s * 0.5f;

        const float B = Dir.Body;
        const float M = Dir.Mind;
        const float S = Dir.Spirit;
        const float N = Dir.Nature;

        float newBody = (1.0f - mix) * B + k * M + mix * S;
        float newMind = -k * B + (1.0f - mix) * M + mix * N;
        float newSpirit = mix * B + (1.0f - mix) * S + k * N;
        float newNature = mix * M - k * S + (1.0f - mix) * N;

        Dir.Body = newBody;
        Dir.Mind = newMind;
        Dir.Spirit = newSpirit;
        Dir.Nature = newNature;

        float lengthScale = 1.0f + s * 0.5f;
        Dir.Body *= lengthScale;
        Dir.Mind *= lengthScale;
        Dir.Spirit *= lengthScale;
        Dir.Nature *= lengthScale;

        float LenSq = Dir.Body * Dir.Body + Dir.Mind * Dir.Mind + Dir.Spirit * Dir.Spirit + Dir.Nature * Dir.Nature;
        const float MaxLenSq = 4.0f;
        if (LenSq > MaxLenSq)
        {
            float InvScale = FMath::Sqrt(MaxLenSq / LenSq);
            Dir.Body *= InvScale;
            Dir.Mind *= InvScale;
            Dir.Spirit *= InvScale;
            Dir.Nature *= InvScale;
        }

        UE_LOG(LogHerbalist, Verbose, TEXT("[MOROK] s=%.3f mix=%.3f k=%.3f | Dir=(%.3f, %.3f, %.3f, %.3f)"),
            s, mix, k, Dir.Body, Dir.Mind, Dir.Spirit, Dir.Nature);
    }

} // namespace HerbalistCore