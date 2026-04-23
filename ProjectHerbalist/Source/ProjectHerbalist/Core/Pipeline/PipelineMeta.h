// PipelineMeta.h
#pragma once
#include "CoreMinimal.h"
#include "Core/Types/HerbalistCoreTypes.h"

namespace HerbalistCore
{
    // Строит мета-состояние среды из Env, Memory, локального Distortion и силы Zaryana
    FMeta BuildEnvironmentMeta(
        const FEnvironment& Env,
        const FMemoryState& Memory,
        float LocalDistortion,
        float ZaryanaStrength);

    // Единый безопасный оператор загрязнения: R = 1 - (1 - Ingredient) * (1 - Clamp(Env * Weight, 0, 1))
    FORCEINLINE float BlendMeta(float Ingredient, float Env, float Weight)
    {
        float E = FMath::Clamp(Env * Weight, 0.0f, 1.0f);
        return 1.0f - (1.0f - Ingredient) * (1.0f - E);
    }
}