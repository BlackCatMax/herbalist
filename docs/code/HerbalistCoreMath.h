#pragma once
#include "HerbalistCoreTypes.h"
#include <cmath>

namespace HerbalistCore::Math
{
    inline void Normalize(FDirection& Dir)
    {
        float len = std::sqrt(
            Dir.Body*Dir.Body +
            Dir.Mind*Dir.Mind +
            Dir.Spirit*Dir.Spirit +
            Dir.Nature*Dir.Nature
        );

        if (len < 1e-6f)
        {
            Dir = GetS0().Direction;
            return;
        }

        float inv = 1.0f / len;
        Dir.Body   *= inv;
        Dir.Mind   *= inv;
        Dir.Spirit *= inv;
        Dir.Nature *= inv;
    }

    inline float Distance(const FDirection& A, const FDirection& B)
    {
        float db = A.Body - B.Body;
        float dm = A.Mind - B.Mind;
        float ds = A.Spirit - B.Spirit;
        float dn = A.Nature - B.Nature;
        return std::sqrt(db*db + dm*dm + ds*ds + dn*dn);
    }

    inline float Dot(const FDirection& A, const FDirection& B)
    {
        return A.Body*B.Body + A.Mind*B.Mind + A.Spirit*B.Spirit + A.Nature*B.Nature;
    }

    inline float Clamp01(float v)
    {
        return std::max(0.0f, std::min(1.0f, v));
    }

    inline float Clamp(float v, float min, float max)
    {
        return std::max(min, std::min(max, v));
    }

    inline float Distance(const FMeta& A, const FMeta& B)
    {
        return std::abs(A.Potency - B.Potency)
             + std::abs(A.Purity - B.Purity)
             + std::abs(A.Stability - B.Stability)
             + std::abs(A.Resonance - B.Resonance)
             + std::abs(A.Corruption - B.Corruption)
             + std::abs(A.Distortion - B.Distortion);
    }

    inline float DistanceToS0(const FRealState& State)
    {
        const FRealState S0 = GetS0();
        return 0.5f * Distance(State.Direction, S0.Direction)
             + 0.2f * std::abs(State.Magnitude - S0.Magnitude)
             + 0.3f * Distance(State.Meta, S0.Meta);
    }
}