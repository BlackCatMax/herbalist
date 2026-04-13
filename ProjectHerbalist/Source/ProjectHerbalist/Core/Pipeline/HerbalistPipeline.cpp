#include "Core/Pipeline/HerbalistPipeline.h"
#include "Core/Types/HerbalistCoreMath.h"

using namespace HerbalistCore;

FRealState Pipeline::ApplyMorok(
    const FRealState& Input,
    float MorokPower,
    FRngState& Rng
)
{
    FRealState Result = Input;

    Result.Meta.Distortion = Math::Clamp01(Result.Meta.Distortion + MorokPower * 0.2f);
    Result.Meta.Corruption = Math::Clamp01(Result.Meta.Corruption + MorokPower * 0.2f);
    Result.Meta.Purity = Math::Clamp01(Result.Meta.Purity - MorokPower * 0.1f);

    Result.Magnitude = Math::Clamp01(Result.Magnitude);

    return Result;
}

FPerceivedState Pipeline::DistortPerception(
    const FRealState& Real,
    float Distortion,
    float Noise,
    FRngState& Rng
)
{
    FPerceivedState Out;

    Out.Direction = Real.Direction;
    Out.Meta = Real.Meta;
    Out.Magnitude = Real.Magnitude;

    Out.Meta.Distortion = Math::Clamp01(Out.Meta.Distortion + Distortion);
    Out.Meta.Corruption = Math::Clamp01(Out.Meta.Corruption + Distortion * 0.5f);
    Out.Meta.Purity = Math::Clamp01(Out.Meta.Purity - Distortion * 0.3f);

    return Out;
}