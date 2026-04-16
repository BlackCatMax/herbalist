// BiomeTypes.h
#pragma once

#include "CoreMinimal.h"
#include "Core/Types/HerbalistCoreTypes.h"   // для EBiomeType, FRealState, FEnvironment, EResourceType
#include "Math/RandomStream.h"

// UENUM(EBiomeType) УДАЛЁН – теперь в HerbalistCoreTypes.h

struct PROJECTHERBALIST_API FBiomeDefaults
{
    static FRealState GetDefaultState(EBiomeType Biome);
    static FEnvironment GetDefaultEnvironment(EBiomeType Biome);
    static EResourceType GetRandomResourceForBiome(EBiomeType Biome, FRandomStream& Rng);
};