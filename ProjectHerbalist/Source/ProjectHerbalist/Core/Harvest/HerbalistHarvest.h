// HerbalistHarvest.h
#pragma once

#include "CoreMinimal.h"
#include "Core/Types/HerbalistCoreTypes.h"

class PROJECTHERBALIST_API FHerbalistHarvest
{
public:
    // Получить базовые параметры ингредиента по его ID (FName)
    static FRealState GetBaseResourceParams(FName IngredientID);

    // Собрать ресурс с учётом состояния биома и условий
    static FRealState Harvest(FName IngredientID, const FRealState& BiomeState, const FConditionModifier& Conditions = FConditionModifier());

    // Получить отображаемое имя ингредиента
    static FString GetResourceName(FName IngredientID, bool bEnglish = false);
};

// Глобальная константа, используемая в нескольких файлах
extern const float k_condition;