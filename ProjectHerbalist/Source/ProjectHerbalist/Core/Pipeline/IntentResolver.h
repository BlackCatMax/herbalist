// IntentResolver.h
#pragma once
#include "CoreMinimal.h"
#include "Core/Types/HerbalistCoreTypes.h"
#include "Core/Pipeline/AlchemyTypes.h"

namespace HerbalistCore
{
    /**
     * Вычисляет Coherence (0-1) на основе порядка ингредиентов, их осей, чистоты,
     * классов ингредиентов и глобального Distortion.
     * OrderedNonWaterAtoms — ингредиенты в порядке добавления в котёл (без воды).
     * WaterAtoms — вода, даёт небольшой бонус к чистоте.
     * GlobalDistortion — AccumulatedDistortion из Memory клетки.
     * 
     * Коэффициенты больше не читаются из UHerbalistSettings внутри функции,
     * а передаются как аргументы (чистая функция).
     */
    float ComputeIntentCoherence(
        const TArray<FAlchemyAtom>& OrderedNonWaterAtoms,
        const TArray<FAlchemyAtom>& WaterAtoms,
        float GlobalDistortion,
        float FoldWeightDecay,      // из настроек (по умолчанию 0.8)
        float CatalystBonus,        // из настроек (по умолчанию 0.1)
        float UnknownPenalty,       // из настроек (по умолчанию 0.15)
        float EssenceBonus,         // из настроек (по умолчанию 0.05)
        float WaterBonusFactor      // из настроек (по умолчанию 0.2)
    );
}
