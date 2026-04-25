#pragma once

#include "CoreMinimal.h"
#include "WaterTypeRow.h"

class UDataTable;

struct PROJECTHERBALIST_API FWaterTypeRegistry
{
public:
    static void Initialize(UDataTable* WaterTypeTable);

    static const FWaterTypeRow* GetWaterType(FName WaterTypeID);

    static FName GetRandomWaterType(EBiomeType Biome, FRandomStream& Rng);

    static bool IsValidWaterType(FName WaterTypeID);

    static TArray<FName> GetWaterTypesForBiome(EBiomeType Biome);

    static int32 GetWaterTypeCount();

    static void Reset();

private:
    static TMap<FName, FWaterTypeRow> WaterTypeMap;
    static bool bIsInitialized;
};
