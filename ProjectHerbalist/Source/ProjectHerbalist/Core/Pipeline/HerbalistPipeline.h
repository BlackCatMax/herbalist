#pragma once

#include "CoreMinimal.h"
#include "Core/Types/HerbalistCoreTypes.h"

namespace HerbalistCore
{
    class Pipeline
    {
    public:
        static FRealState Fold(const TArray<FRealState>& Inputs);
        static FRealState ComputeDelta(const FRealState& Aggregated, const FRealState& CurrentBiomeState);
        static void ApplyContext(FRealState& Delta, const FEnvironment& Env);
        static FRealState ApplyMorok(
            const TArray<FRealState>& Inputs,
            const FRealState& CurrentBiomeState,
            const FEnvironment& Env,
            const FMemoryState& Memory,
            const FIntent& Intent,
            FRngState& Rng
        );
    };
}