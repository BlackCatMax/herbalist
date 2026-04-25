// HerbalistCoreMath.h
#pragma once

#include "CoreMinimal.h"
#include "Core/Types/HerbalistCoreTypes.h"

namespace HerbalistCore::Math
{
    inline float Clamp01(float v)
    {
        return FMath::Clamp(v, 0.0f, 1.0f);
    }

    inline float Clamp(float v, float Min, float Max)
    {
        return FMath::Clamp(v, Min, Max);
    }

    inline float Dot(const FDirection& A, const FDirection& B)
    {
        return A.Body * B.Body +
            A.Mind * B.Mind +
            A.Spirit * B.Spirit +
            A.Nature * B.Nature;
    }

    inline float Distance(const FDirection& A, const FDirection& B)
    {
        float dx = A.Body - B.Body;
        float dy = A.Mind - B.Mind;
        float dz = A.Spirit - B.Spirit;
        float dw = A.Nature - B.Nature;
        return FMath::Sqrt(dx * dx + dy * dy + dz * dz + dw * dw);
    }

    // Сравнение двух состояний с заданными допусками
    bool AreStatesSimilar(const FRealState& A, const FRealState& B,
        float MagnitudeThreshold = 0.15f,
        float DistortionThreshold = 0.2f,
        float PurityThreshold = 0.2f,
        float StabilityThreshold = 0.2f,
        float DirectionThreshold = 0.15f);
}
