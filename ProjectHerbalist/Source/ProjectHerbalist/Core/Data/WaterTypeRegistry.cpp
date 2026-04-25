#include "WaterTypeRegistry.h"
#include "Engine/DataTable.h"
#include "ProjectHerbalist.h"

TMap<FName, FWaterTypeRow> FWaterTypeRegistry::WaterTypeMap;
bool FWaterTypeRegistry::bIsInitialized = false;

void FWaterTypeRegistry::Initialize(UDataTable* WaterTypeTable)
{
    if (bIsInitialized)
    {
        UE_LOG(LogHerbalist, Warning, TEXT("[Herbalist] FWaterTypeRegistry already initialized"));
        return;
    }

    if (!WaterTypeTable)
    {
        UE_LOG(LogHerbalist, Error, TEXT("[Herbalist] FWaterTypeRegistry: WaterTypeTable is null"));
        bIsInitialized = true;
        return;
    }

    if (WaterTypeTable->GetRowStruct() != FWaterTypeRow::StaticStruct())
    {
        UE_LOG(LogHerbalist, Error, TEXT("[Herbalist] FWaterTypeRegistry: Row structure mismatch"));
        bIsInitialized = true;
        return;
    }

    const FString ContextStr(TEXT("FWaterTypeRegistry::Initialize"));
    TArray<FName> RowNames = WaterTypeTable->GetRowNames();

    for (const FName& RowName : RowNames)
    {
        if (const FWaterTypeRow* Row = WaterTypeTable->FindRow<FWaterTypeRow>(RowName, ContextStr))
        {
            WaterTypeMap.Add(Row->WaterTypeID, *Row);
        }
    }

    bIsInitialized = true;
    UE_LOG(LogHerbalist, Log, TEXT("[Herbalist] FWaterTypeRegistry initialized with %d water types"), WaterTypeMap.Num());
}

const FWaterTypeRow* FWaterTypeRegistry::GetWaterType(FName WaterTypeID)
{
    if (!bIsInitialized)
    {
        UE_LOG(LogHerbalist, Error, TEXT("[Herbalist] FWaterTypeRegistry::GetWaterType called before Initialize!"));
        return nullptr;
    }

    return WaterTypeMap.Find(WaterTypeID);
}

FName FWaterTypeRegistry::GetRandomWaterType(EBiomeType Biome, FRandomStream& Rng)
{
    if (!bIsInitialized)
    {
        return NAME_None;
    }

    TArray<FName> Candidates;
    TArray<float> Weights;

    for (const auto& Pair : WaterTypeMap)
    {
        if (Pair.Value.AllowedBiomes.Contains(Biome))
        {
            Candidates.Add(Pair.Key);
            Weights.Add(Pair.Value.Rarity);
        }
    }

    if (Candidates.Num() == 0)
    {
        return NAME_None;
    }

    float TotalWeight = 0.0f;
    for (float W : Weights) TotalWeight += W;

    float Random = Rng.FRand() * TotalWeight;
    float Accumulated = 0.0f;
    for (int32 i = 0; i < Candidates.Num(); ++i)
    {
        Accumulated += Weights[i];
        if (Random <= Accumulated)
        {
            return Candidates[i];
        }
    }

    return Candidates.Last();
}

bool FWaterTypeRegistry::IsValidWaterType(FName WaterTypeID)
{
    return bIsInitialized && WaterTypeMap.Contains(WaterTypeID);
}

TArray<FName> FWaterTypeRegistry::GetWaterTypesForBiome(EBiomeType Biome)
{
    TArray<FName> Result;
    if (!bIsInitialized) return Result;

    for (const auto& Pair : WaterTypeMap)
    {
        if (Pair.Value.AllowedBiomes.Contains(Biome))
        {
            Result.Add(Pair.Key);
        }
    }
    return Result;
}

int32 FWaterTypeRegistry::GetWaterTypeCount()
{
    return WaterTypeMap.Num();
}

void FWaterTypeRegistry::Reset()
{
    WaterTypeMap.Empty();
    bIsInitialized = false;
}