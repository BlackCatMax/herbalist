#pragma once

#include "CoreMinimal.h"
#include "Core/Types/HerbalistCoreTypes.h"

DECLARE_LOG_CATEGORY_EXTERN(LogHerbalist, Log, All);

namespace HerbalistCore
{
    class Pipeline
    {
    public:

        static FRealState ApplyMorok(
            const FRealState& A,
            const FRealState& B,
            const FEnvironment& Env,
            const FMemoryState& Memory,
            const FIntent& Intent,
            FRngState& Rng
        );
    };
}