// HerbalistCoreMath.cpp
#include "HerbalistCoreMath.h"

bool HerbalistCore::Math::AreStatesSimilar(const FRealState& A, const FRealState& B,
                                           float MagnitudeThreshold,
                                           float DistortionThreshold,
                                           float PurityThreshold,
                                           float StabilityThreshold,
                                           float DirectionThreshold)
{
    if (FMath::Abs(A.Magnitude - B.Magnitude) > MagnitudeThreshold) return false;
    if (FMath::Abs(A.Meta.Distortion - B.Meta.Distortion) > DistortionThreshold) return false;
    if (FMath::Abs(A.Meta.Purity - B.Meta.Purity) > PurityThreshold) return false;
    if (FMath::Abs(A.Meta.Stability - B.Meta.Stability) > StabilityThreshold) return false;

    float DirDiff = Distance(A.Direction, B.Direction);
    if (DirDiff > DirectionThreshold * 2.0f) return false;

    return true;
}

void HerbalistCore::Math::BlendRealStatesForStack(FRealState& Target, const FRealState& Source, int32 TargetCount, int32 AddedCount)
{
    const int32 NewCount = TargetCount + AddedCount;
    if (NewCount <= 0) return;
    const float OldWeight = (float)TargetCount / NewCount;
    const float NewWeight = (float)AddedCount / NewCount;

    FRealState& T = Target;
    const FRealState& S = Source;

    T.Magnitude = T.Magnitude * OldWeight + S.Magnitude * NewWeight;

    T.Direction.Body   = T.Direction.Body   * OldWeight + S.Direction.Body   * NewWeight;
    T.Direction.Mind   = T.Direction.Mind   * OldWeight + S.Direction.Mind   * NewWeight;
    T.Direction.Spirit = T.Direction.Spirit * OldWeight + S.Direction.Spirit * NewWeight;
    T.Direction.Nature = T.Direction.Nature * OldWeight + S.Direction.Nature * NewWeight;
    T.Direction.NormalizeSum();

    T.Meta.Distortion = T.Meta.Distortion * OldWeight + S.Meta.Distortion * NewWeight;
    T.Meta.Stability  = T.Meta.Stability  * OldWeight + S.Meta.Stability  * NewWeight;
    T.Meta.Purity     = T.Meta.Purity     * OldWeight + S.Meta.Purity     * NewWeight;
    T.Meta.Potency    = T.Meta.Potency    * OldWeight + S.Meta.Potency    * NewWeight;
    T.Meta.Resonance  = T.Meta.Resonance  * OldWeight + S.Meta.Resonance  * NewWeight;
    T.Meta.Corruption = T.Meta.Corruption * OldWeight + S.Meta.Corruption * NewWeight;

    const float DistortionDiff = FMath::Abs(T.Meta.Distortion - S.Meta.Distortion);
    T.Meta.Distortion = FMath::Clamp(T.Meta.Distortion + DistortionDiff * 0.15f, 0.0f, 1.0f);

    const float PurityDiff = FMath::Abs(T.Meta.Purity - S.Meta.Purity);
    T.Meta.Purity = FMath::Clamp(T.Meta.Purity - PurityDiff * 0.1f, 0.0f, 1.0f);
}
