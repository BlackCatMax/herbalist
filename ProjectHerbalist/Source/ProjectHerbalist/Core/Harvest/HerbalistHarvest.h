// HerbalistHarvest.h
#pragma once

#include "CoreMinimal.h"
#include "Core/Types/HerbalistCoreTypes.h"

UENUM(BlueprintType)
enum class EResourceType : uint8
{
    Nettle,      // крапива
    Fern,        // папоротник
    Mushroom,    // мухомор
    BirchBark,   // кора берёзы
    Moss,        // мох
    Cranberry,   // клюква
    BogOre       // болотная руда
};

// Модификаторы условий (погода, время суток, сезон) – пока заглушка
struct FConditionModifier
{
    FDirection DeltaDirection;
    float DeltaMagnitude = 0.0f;
    float DeltaDistortion = 0.0f;
    float DeltaStability = 0.0f;
    float DeltaPurity = 0.0f;

    FConditionModifier()
    {
        DeltaDirection.Body = 0.0f;
        DeltaDirection.Mind = 0.0f;
        DeltaDirection.Spirit = 0.0f;
        DeltaDirection.Nature = 0.0f;
    }
};

class PROJECTHERBALIST_API FHerbalistHarvest
{
public:
    static FRealState GetBaseResourceParams(EResourceType Type);
    static FRealState Harvest(EResourceType Type, const FRealState& BiomeState, const FConditionModifier& Conditions = FConditionModifier());
};