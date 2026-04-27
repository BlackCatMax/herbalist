#include "WaterTypeRegistrySubsystem.h"
#include "Engine/DataTable.h"
#include "ProjectHerbalist.h"

void UWaterTypeRegistrySubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
}

void UWaterTypeRegistrySubsystem::Deinitialize()
{
    Reset();
    Super::Deinitialize();
}

void UWaterTypeRegistrySubsystem::LoadFromDataTable(UDataTable* WaterTypeTable)
{
    if (bInitialized) return;
    if (!WaterTypeTable)
    {
        UE_LOG(LogHerbalist, Error, TEXT("WaterTypeTable is null"));
        bInitialized = true;
        return;
    }

    if (WaterTypeTable->GetRowStruct() != FWaterTypeRow::StaticStruct())
    {
        UE_LOG(LogHerbalist, Error, TEXT("WaterTypeTable row structure mismatch"));
        bInitialized = true;
        return;
    }

    const FString Context(TEXT("WaterTypeRegistrySubsystem"));
    TArray<FName> RowNames = WaterTypeTable->GetRowNames();
    for (FName RowName : RowNames)
    {
        if (const FWaterTypeRow* Row = WaterTypeTable->FindRow<FWaterTypeRow>(RowName, Context))
        {
            WaterTypeMap.Add(Row->WaterTypeID, *Row);
        }
    }

    BuildCache();
    bInitialized = true;
    UE_LOG(LogHerbalist, Log, TEXT("WaterTypeRegistrySubsystem initialized with %d water types"), WaterTypeMap.Num());
}

void UWaterTypeRegistrySubsystem::BuildCache()
{
    CachedWaterTypesByBiome.Empty();
    CachedRarityByBiome.Empty();

    for (const auto& Pair : WaterTypeMap)
    {
        for (EBiomeType Biome : Pair.Value.AllowedBiomes)
        {
            CachedWaterTypesByBiome.FindOrAdd(Biome).Add(Pair.Key);
            CachedRarityByBiome.FindOrAdd(Biome).Add(Pair.Value.Rarity);
        }
    }
}

const FWaterTypeRow* UWaterTypeRegistrySubsystem::GetWaterType(FName WaterTypeID) const
{
    if (!bInitialized) return nullptr;
    return WaterTypeMap.Find(WaterTypeID);
}

FName UWaterTypeRegistrySubsystem::GetRandomWaterType(EBiomeType Biome, FRandomStream& Rng) const
{
    const TArray<FName>* Candidates = CachedWaterTypesByBiome.Find(Biome);
    if (!Candidates || Candidates->Num() == 0) return NAME_None;

    const TArray<float>* Weights = CachedRarityByBiome.Find(Biome);
    if (!Weights || Weights->Num() != Candidates->Num()) return (*Candidates)[0];

    float TotalWeight = 0.0f;
    for (float W : *Weights) TotalWeight += W;
    if (TotalWeight <= 0.0f) return (*Candidates)[0];

    float Roll = Rng.FRand() * TotalWeight;
    float Accum = 0.0f;
    for (int32 i = 0; i < Candidates->Num(); ++i)
    {
        Accum += (*Weights)[i];
        if (Roll <= Accum) return (*Candidates)[i];
    }
    return Candidates->Last();
}

bool UWaterTypeRegistrySubsystem::IsValidWaterType(FName WaterTypeID) const
{
    return bInitialized && WaterTypeMap.Contains(WaterTypeID);
}

TArray<FName> UWaterTypeRegistrySubsystem::GetWaterTypesForBiome(EBiomeType Biome) const
{
    if (!bInitialized) return TArray<FName>();
    const TArray<FName>* Found = CachedWaterTypesByBiome.Find(Biome);
    return Found ? *Found : TArray<FName>();
}

int32 UWaterTypeRegistrySubsystem::GetWaterTypeCount() const
{
    return WaterTypeMap.Num();
}

void UWaterTypeRegistrySubsystem::Reset()
{
    WaterTypeMap.Empty();
    CachedWaterTypesByBiome.Empty();
    CachedRarityByBiome.Empty();
    bInitialized = false;
}