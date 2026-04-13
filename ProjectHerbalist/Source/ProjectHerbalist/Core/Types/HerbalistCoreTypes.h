#pragma once

namespace HerbalistCore
{
    struct FDirection
    {
        float Body = 0.f;
        float Mind = 0.f;
        float Spirit = 0.f;
        float Nature = 0.f;
    };

    struct FMeta
    {
        float Purity = 0.f;
        float Corruption = 0.f;
        float Stability = 0.f;
        float Distortion = 0.f;
        float Potency = 0.f;
    };

    struct FRealState
    {
        FDirection Direction;
        FMeta Meta;
        float Magnitude = 0.f;
    };

    struct FPerceivedState
    {
        FDirection Direction;
        FMeta Meta;
        float Magnitude = 0.f;
    };

    struct FRngState
    {
        uint32 Seed = 12345;
    };
}