// BiomeTypes.h
#pragma once

#include "CoreMinimal.h"
#include "Core/Types/HerbalistCoreTypes.h"
#include "Math/RandomStream.h"

struct PROJECTHERBALIST_API FBiomeDefaults
{
    static FRealState GetDefaultState(EBiomeType Biome);
    static FEnvironment GetDefaultEnvironment(EBiomeType Biome);
    static EResourceType GetRandomResourceForBiome(EBiomeType Biome, FRandomStream& Rng);
    static FRealState GetDefaultWaterState(EBiomeType Biome);
};