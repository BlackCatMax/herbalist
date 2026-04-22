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