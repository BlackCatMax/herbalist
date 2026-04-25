// IngredientRegistry.cpp
#include "IngredientRegistry.h"
#include "Engine/DataTable.h"
#include "ProjectHerbalist.h"

TMap<FName, FIngredientTableRow> FIngredientRegistry::Rows;
bool FIngredientRegistry::bIsInitialized = false;

void FIngredientRegistry::Initialize(UDataTable* IngredientTable)
{
    if (bIsInitialized) return;
    if (!IngredientTable) { UE_LOG(LogHerbalist, Error, TEXT("IngredientTable is null")); bIsInitialized = true; return; }
    const FString Context(TEXT("IngredientRegistry"));
    TArray<FName> RowNames = IngredientTable->GetRowNames();
    for (FName RowName : RowNames)
        if (auto* Row = IngredientTable->FindRow<FIngredientTableRow>(RowName, Context))
            Rows.Add(RowName, *Row);
    bIsInitialized = true;
    UE_LOG(LogHerbalist, Log, TEXT("IngredientRegistry loaded %d ingredients"), Rows.Num());
}

const FIngredientTableRow* FIngredientRegistry::GetRow(FName IngredientID)
{
    if (!bIsInitialized) return nullptr;
    return Rows.Find(IngredientID);
}

EIngredientClass FIngredientRegistry::Classify(FName IngredientID)
{
    auto* Row = GetRow(IngredientID);
    return Row ? Row->Class : EIngredientClass::Unknown;
}

bool FIngredientRegistry::IsWater(FName IngredientID)
{
    auto* Row = GetRow(IngredientID);
    return Row ? Row->bIsWater : false;
}

TArray<FName> FIngredientRegistry::GetResourcesForBiome(EBiomeType Biome)
{
    TArray<FName> Res;
    if (!bIsInitialized) return Res;
    for (auto& Pair : Rows)
        if (Pair.Value.AllowedBiomes.Contains(Biome))
            Res.Add(Pair.Key);
    return Res;
}

FName FIngredientRegistry::GetRandomResourceForBiome(EBiomeType Biome, FRandomStream& Rng)
{
    auto Candidates = GetResourcesForBiome(Biome);
    if (Candidates.Num() == 0) return NAME_None;
    if (Candidates.Num() == 1) return Candidates[0];
    int32 TotalWeight = 0;
    for (auto& ID : Candidates)
        if (auto* Row = GetRow(ID)) TotalWeight += Row->RarityWeight;
    if (TotalWeight == 0) return Candidates[0];
    int32 Roll = Rng.RandRange(1, TotalWeight);
    int32 Accum = 0;
    for (auto& ID : Candidates)
    {
        if (auto* Row = GetRow(ID)) Accum += Row->RarityWeight;
        if (Roll <= Accum) return ID;
    }
    return Candidates[0];
}

void FIngredientRegistry::Reset()
{
    Rows.Empty();
    bIsInitialized = false;
}

bool FIngredientRegistry::IsKnown(FName IngredientID)
{
    return Rows.Contains(IngredientID);
}