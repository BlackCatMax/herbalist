// AlchemySemantics.h
#pragma once
#include "CoreMinimal.h"
#include "Core/Types/HerbalistCoreTypes.h"
#include "Core/Data/IngredientTableRow.h"

namespace HerbalistCore
{
    EAlchemyOutcome ClassifyOutcome(const TArray<FInventoryItem>& Inputs);

    FRealState ApplyAshTransform(const FMeta& CoreMeta);
    FRealState ApplyBoiledWaterTransform(const TArray<FRealState>& WaterStates);
    FRealState ApplyCatastropheTransform(FRealState& InState, bool bCollapse, FRngState& Rng);

    // Классификаторы ингредиентов через реестр
    bool IsWaterIngredient(FName IngredientName);
    bool IsPlantIngredient(FName IngredientName);
    bool IsMineralIngredient(FName IngredientName);
    bool IsFungusIngredient(FName IngredientName);
    bool IsCatalystIngredient(FName IngredientName);
    bool IsEssenceIngredient(FName IngredientName);
    bool IsKnownIngredient(FName IngredientName);
    EIngredientClass GetIngredientClass(FName IngredientName);
}
