// IntentResolver.h
#pragma once
#include "CoreMinimal.h"
#include "Core/Types/HerbalistCoreTypes.h"
#include "Core/Pipeline/AlchemyTypes.h"

namespace HerbalistCore
{
    // Вычисляет Coherence (0-1) на основе порядка ингредиентов, их осей и чистоты.
    // OrderedNonWaterAtoms — ингредиенты в порядке добавления в котёл (без воды).
    // WaterAtoms — вода, даёт небольшой бонус к чистоте, но не обязательна.
    float ComputeIntentCoherence(const TArray<FAlchemyAtom>& OrderedNonWaterAtoms,
                                 const TArray<FAlchemyAtom>& WaterAtoms);
}