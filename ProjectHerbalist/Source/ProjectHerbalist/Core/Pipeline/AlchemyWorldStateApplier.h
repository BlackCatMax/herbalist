// AlchemyWorldStateApplier.h
#pragma once

#include "CoreMinimal.h"
#include "Core/Types/HerbalistCoreTypes.h"
#include "AlchemySemanticResolver.h"
#include "AlchemyPhysicsPipeline.h"

class FAlchemyWorldStateApplier
{
public:
    /** Применить результат алхимии к миру. */
    static FRealState Apply(
        const FAlchemySemanticResult& Semantic,
        const FAlchemyPhysicsResult& Physics,
        FRngState& Rng);

    /**
     * Применить дельту Distortion к FMemoryState с saturation curve.
     * Гарантирует непрерывность: Distortion никогда не меняется скачком.
     *
     * @param Memory   Состояние памяти клетки (изменяется).
     * @param Delta    Запрашиваемое изменение AccumulatedDistortion.
     * @param CurrentTime Текущее игровое время.
     */
    static void ApplyDistortionDelta(
        FMemoryState& Memory,
        float Delta,
        float CurrentTime);

private:
    /** Saturation factor: замедляет рост Distortion при приближении к порогу катастрофы. */
    static float GetDistortionSaturation(float CurrentDistortion);
};
