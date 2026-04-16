#pragma once

#include "CoreMinimal.h"
#include "Core/Types/HerbalistCoreTypes.h"   // теперь содержит FConditionModifier

class PROJECTHERBALIST_API FHerbalistHarvest
{
public:
    static FRealState GetBaseResourceParams(EResourceType Type);
    static FRealState Harvest(EResourceType Type, const FRealState& BiomeState, const FConditionModifier& Conditions = FConditionModifier());
    static FString GetResourceName(EResourceType Type, bool bEnglish = false);
};