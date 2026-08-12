// BiomeTypes.h
#pragma once

#include "CoreMinimal.h"
#include "Core/Types/HerbalistCoreTypes.h"

struct FBiomeRow;

class PROJECTHERBALIST_API FBiomeDefaults
{
public:
    static void SetBiomeTable(UDataTable* InTable);
    static const FBiomeRow* GetBiomeRow(EBiomeType Biome);

    static FRealState GetDefaultState(EBiomeType Biome);
    static FEnvironment GetDefaultEnvironment(EBiomeType Biome);
    static FRealState GetDefaultWaterState(EBiomeType Biome);

    static FName BiomeTypeToName(EBiomeType Biome);
    static EBiomeType NameToBiomeType(FName Name);
    static TArray<EBiomeType> GetAllBiomeTypes();
};
