// IntentResolver.h
#pragma once
#include "CoreMinimal.h"
#include "Core/Types/HerbalistCoreTypes.h"
#include "Core/Pipeline/AlchemyTypes.h"

namespace HerbalistCore
{
    // Вычисляет Coherence (0-1) на основе порядка ингредиентов, их осей, чистоты,
    // классов ингредиентов и глобального Distortion.
    // OrderedNonWaterAtoms — ингредиенты в порядке добавления в котёл (без воды).
    // WaterAtoms — вода, даёт небольшой бонус к чистоте.
    // GlobalDistortion — AccumulatedDistortion из Memory клетки.
    float ComputeIntentCoherence(const TArray<FAlchemyAtom>& OrderedNonWaterAtoms,
                                 const TArray<FAlchemyAtom>& WaterAtoms,
                                 float GlobalDistortion);
}