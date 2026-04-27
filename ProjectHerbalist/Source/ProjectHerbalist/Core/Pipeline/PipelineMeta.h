// Core/Pipeline/PipelineMeta.h
#pragma once
#include "CoreMinimal.h"
#include "Core/Types/HerbalistCoreTypes.h"

namespace HerbalistCore
{
    /**
     * Строит «средовую» мету (FMeta) на основе параметров окружения и памяти.
     * Используется для влияния биома на алхимический процесс.
     */
    FMeta BuildEnvironmentMeta(const FEnvironment& Env, const FMemoryState& Memory);

    /**
     * Взвешенное смешивание двух структур Meta.
     * Alpha = 0 -> чистая A, Alpha = 1 -> чистая B.
     */
    FMeta BlendMeta(const FMeta& A, const FMeta& B, float Alpha);

    // Обратно совместимая однокомпонентная версия
    FORCEINLINE float BlendMetaComponent(float Ingredient, float Env, float Weight)
    {
        float E = FMath::Clamp(Env * Weight, 0.0f, 1.0f);
        return 1.0f - (1.0f - Ingredient) * (1.0f - E);
    }
}