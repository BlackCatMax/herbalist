#pragma once

#include "Core/Types/HerbalistCoreTypes.h"
#include <cmath>

namespace HerbalistCore::Math
{
    inline float Clamp01(float v)
    {
        return std::fmax(0.f, std::fmin(1.f, v));
    }

    inline float Clamp(float v, float min, float max)
    {
        return std::fmax(min, std::fmin(max, v));
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

        return std::sqrt(dx * dx + dy * dy + dz * dz + dw * dw);
    }
}