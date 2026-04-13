#pragma once

#include "Core/Types/HerbalistCoreTypes.h"

namespace HerbalistCore
{
    class Pipeline
    {
    public:

        static FRealState ApplyMorok(
            const FRealState& Input,
            float MorokPower,
            FRngState& Rng
        );

        static FPerceivedState DistortPerception(
            const FRealState& Real,
            float Distortion,
            float Noise,
            FRngState& Rng
        );
    };
}