#pragma once

#include "CoreMinimal.h"
#include "Types/HerbalistCoreTypes.h"
#include "Harvest/HerbalistHarvest.h"   // <-- добавить эту строку
#include "Math/RandomStream.h"
#include "BiomeTypes.generated.h"

UENUM(BlueprintType)
enum class EBiomeType : uint8
{
    MixedForest,
    Swamp,
    Steppe,
    Floodplain
};

struct PROJECTHERBALIST_API FBiomeDefaults
{
    static FRealState GetDefaultState(EBiomeType Biome);
    static FEnvironment GetDefaultEnvironment(EBiomeType Biome);
    static EResourceType GetRandomResourceForBiome(EBiomeType Biome, FRandomStream& Rng);
};