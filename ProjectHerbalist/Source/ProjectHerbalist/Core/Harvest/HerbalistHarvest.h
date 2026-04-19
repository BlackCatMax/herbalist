// HerbalistHarvest.h
#pragma once

#include "CoreMinimal.h"
#include "Core/Types/HerbalistCoreTypes.h"

class PROJECTHERBALIST_API FHerbalistHarvest
{
public:
    static FRealState GetBaseResourceParams(EResourceType Type);
    static FRealState Harvest(EResourceType Type, const FRealState& BiomeState, const FConditionModifier& Conditions = FConditionModifier());
    static FString GetResourceName(EResourceType Type, bool bEnglish = false);
};

// Глобальная константа, используемая в нескольких файлах
extern const float k_condition;