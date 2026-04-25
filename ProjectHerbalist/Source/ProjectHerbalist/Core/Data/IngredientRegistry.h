// IngredientRegistry.h
#pragma once

#include "CoreMinimal.h"
#include "IngredientTableRow.h"

class UDataTable;

struct PROJECTHERBALIST_API FIngredientRegistry
{
public:
    static void Initialize(UDataTable* IngredientTable);
    static const FIngredientTableRow* GetRow(FName IngredientID);
    static EIngredientClass Classify(FName IngredientID);
    static bool IsWater(FName IngredientID);
    static bool IsKnown(FName IngredientID);
    static TArray<FName> GetResourcesForBiome(EBiomeType Biome);
    static FName GetRandomResourceForBiome(EBiomeType Biome, FRandomStream& Rng);
    static void Reset();

private:
    static TMap<FName, FIngredientTableRow> Rows;
    static bool bIsInitialized;
};