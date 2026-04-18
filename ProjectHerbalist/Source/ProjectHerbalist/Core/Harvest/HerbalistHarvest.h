// HerbalistHarvest.h
#pragma once

#include "CoreMinimal.h"
#include "Core/Types/HerbalistCoreTypes.h"

class PROJECTHERBALIST_API FHerbalistHarvest
{
public:
    // Получить базовые параметры ресурса из DataAsset (через ResourceDataManager)
    static FRealState GetBaseResourceParams(EResourceType Type);

    // Собрать ресурс: базовые параметры + модификации от биома и условий
    static FRealState Harvest(EResourceType Type, const FRealState& BiomeState, const FConditionModifier& Conditions = FConditionModifier());

    // Получить отображаемое имя ресурса (из DataAsset)
    static FString GetResourceName(EResourceType Type, bool bEnglish = false);
};