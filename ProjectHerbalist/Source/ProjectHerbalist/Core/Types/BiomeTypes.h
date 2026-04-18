// BiomeTypes.h
#pragma once

#include "CoreMinimal.h"
#include "Core/Types/HerbalistCoreTypes.h"
#include "Core/Types/ResourceBalanceRow.h"   // <-- добавлено для ESeasonMask, ETimeOfDayMask
#include "Math/RandomStream.h"

struct PROJECTHERBALIST_API FBiomeDefaults
{
    static FRealState GetDefaultState(EBiomeType Biome);
    static FEnvironment GetDefaultEnvironment(EBiomeType Biome);
    static FRealState GetDefaultWaterState(EBiomeType Biome);
    
    static EResourceType GetRandomResourceForBiome(EBiomeType Biome, FRandomStream& Rng,
        ESeasonMask Season = ESeasonMask::Summer, ETimeOfDayMask TimeOfDay = ETimeOfDayMask::Day);
    
    static FName BiomeTypeToName(EBiomeType Biome);
    static EBiomeType NameToBiomeType(FName Name);
    static TArray<EBiomeType> GetAllBiomeTypes();
};