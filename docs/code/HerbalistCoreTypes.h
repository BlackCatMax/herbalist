#pragma once

#include <cstdint>
#include <vector>
#include <cmath>
#include <algorithm>

namespace HerbalistCore
{
    struct FDirection
    {
        float Body   = 0.5f;
        float Mind   = 0.5f;
        float Spirit = 0.5f;
        float Nature = 0.5f;

        void Normalize();
        FDirection Normalized() const;
        float Distance(const FDirection& Other) const;
        float Dot(const FDirection& Other) const;

        FDirection operator+(const FDirection& Other) const;
        FDirection& operator+=(const FDirection& Other);
        FDirection operator*(float Scalar) const;
    };

    struct FMeta
    {
        float Potency    = 0.5f;
        float Purity     = 0.7f;
        float Stability  = 0.6f;
        float Resonance  = 0.5f;
        float Corruption = 0.0f;
        float Distortion = 0.2f;

        float Distance(const FMeta& Other) const;
        void Clamp();

        FMeta operator+(const FMeta& Other) const;
        FMeta& operator+=(const FMeta& Other);
        FMeta operator*(float Scalar) const;
    };

    struct FRealState
    {
        FDirection Direction;
        float Magnitude = 0.5f;
        FMeta Meta;

        float DistanceToS0() const;

        FRealState operator+(const FRealState& Other) const;
        FRealState& operator+=(const FRealState& Other);
        FRealState operator*(float Scalar) const;

        static FRealState Zero();
    };

    inline FRealState GetS0()
    {
        FRealState S0;
        S0.Direction = {0.5f, 0.5f, 0.5f, 0.5f};
        S0.Direction.Normalize();
        S0.Magnitude = 0.5f;
        S0.Meta = {0.5f,1.0f,1.0f,0.5f,0.0f,0.0f};
        return S0;
    }

    struct FPerceivedState
    {
        FDirection Direction;
        float Magnitude = 0.5f;
        FMeta Meta;
        float PerceivedDistortion = 0.0f;
    };

    struct FRngState
    {
        uint32_t State = 123456789;

        FRngState() = default;
        explicit FRngState(uint32_t Seed) : State(Seed) {}

        float NextFloat()
        {
            State ^= State << 13;
            State ^= State >> 17;
            State ^= State << 5;
            return static_cast<float>(State >> 8) * (1.0f / 16777216.0f);
        }

        void SetSeed(uint32_t Seed) { State = Seed; }
    };

    inline uint32_t HashCombine(uint32_t A, uint32_t B)
    {
        return A * 1664525u + B * 1013904223u;
    }

    inline uint32_t Hash32(uint32_t x)
    {
        x ^= x >> 16;
        x *= 0x7feb352d;
        x ^= x >> 15;
        x *= 0x846ca68b;
        x ^= x >> 16;
        return x;
    }

    inline FRngState BranchRng(const FRngState& Parent, uint32_t UniqueId)
    {
        return FRngState(Hash32(Parent.State ^ Hash32(UniqueId)));
    }

    enum class EIntentType : uint8_t
    {
        Coherent,
        Neutral,
        Distorted
    };

    struct FAction
    {
        uint32_t BiomeIndex = 0;
        uint32_t LogicalTick = 0;
        std::vector<uint32_t> ResourceIndices;
        std::vector<uint32_t> Sequence;
    };

    struct FIntent
    {
        EIntentType Type = EIntentType::Neutral;
        FDirection DirectionBias;
        float Coherence = 0.5f;
    };

    struct FEnvironment
    {
        float Toxicity  = 0.2f;
        float Fertility = 0.5f;
        float Moisture  = 0.5f;
    };

    struct FBiomeMemory
    {
        float AccumulatedDistortion = 0.0f;
        float StabilityMemory       = 0.5f;
        float HistoryPurity         = 0.7f;
    };

}