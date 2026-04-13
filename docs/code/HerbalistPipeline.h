#pragma once

#include "HerbalistCoreTypes.h"
#include <vector>

namespace HerbalistCore
{
    struct FContextCoefficients
    {
        float AxisModifiers[4];
        float MetaModifiers[6];
    };

    class Pipeline
    {
    public:
        // Агрегация ресурсов (НЕ гарантирует нормализацию)
        static FRealState Fold(const std::vector<FRealState>& Resources,
                               const std::vector<float>& Weights);

        // Применение контекста
        static FRealState ApplyContext(const FRealState& Aggregated,
                                       const FEnvironment& Env,
                                       const FBiomeMemory& Memory);

        // Формирование дельты
        static FRealState ComputeDelta(const FRealState& Aggregated,
                                       const FIntent& Intent);

        // Морок (детерминированный, БЕЗ сайд-эффектов)
        static FRealState ApplyMorok(const FRealState& Delta,
                                     float MorokIntensity,
                                     float CorruptionLevel,
                                     FRngState Rng); // ← ВАЖНО

        // Заряна (стабилизация)
        static FRealState ApplyZaryana(const FRealState& State,
                                       float ZaryanaPower);

        // Восприятие (pure function)
        static FPerceivedState DistortPerception(const FRealState& Real,
                                                 float MorokIntensity,
                                                 float ZaryanaClarity,
                                                 FRngState Rng); // ← ВАЖНО

        // Интерпретация действия
        static FIntent ComputeIntent(const FAction& Action,
                                     const FPerceivedState& Perceived);

    private:
        static void ApplyAxisSignModifiers(FRealState& State);
        static void ApplyInteractionRules(FRealState& State);
    };

} // namespace HerbalistCore