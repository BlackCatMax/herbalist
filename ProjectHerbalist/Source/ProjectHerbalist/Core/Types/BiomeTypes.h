#pragma once

#include "CoreMinimal.h"
#include "Core/Types/HerbalistCoreTypes.h"
#include "BiomeTypes.generated.h"

UENUM(BlueprintType)
enum class EBiomeType : uint8
{
    MixedForest,      // смешанный лес
    Swamp,            // болото
    Steppe,           // степь
    Floodplain        // пойма
};

// Дефолтные параметры для биомов (из таблицы 8.1.1 GDD, упрощённо)
struct PROJECTHERBALIST_API FBiomeDefaults
{
    static FRealState GetDefaultState(EBiomeType Biome);
    static FEnvironment GetDefaultEnvironment(EBiomeType Biome);
};