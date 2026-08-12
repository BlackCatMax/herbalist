#include "IngredientRegistrySubsystem.h"
#include "Core/Subsystems/WaterTypeRegistrySubsystem.h"
#include "Engine/DataTable.h"
#include "ProjectHerbalist.h"
#include "HerbalistLogChannels.h"

void UIngredientRegistrySubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
}

void UIngredientRegistrySubsystem::Deinitialize()
{
    Reset();
    Super::Deinitialize();
}

void UIngredientRegistrySubsystem::LoadFromDataTable(UDataTable* IngredientTable)
{
    if (bInitialized) return;
    if (!IngredientTable)
    {
        UE_LOG(LogHerbalistData, Error, TEXT("IngredientTable is null"));
        bInitialized = true;
        return;
    }

    const FString Context(TEXT("IngredientRegistrySubsystem"));
    TArray<FName> RowNames = IngredientTable->GetRowNames();
    for (FName RowName : RowNames)
    {
        if (FIngredientTableRow* Row = IngredientTable->FindRow<FIngredientTableRow>(RowName, Context))
        {
            Rows.Add(RowName, *Row);
        }
    }

    BuildCache();
    bInitialized = true;
    UE_LOG(LogHerbalistData, Log, TEXT("IngredientRegistrySubsystem loaded %d ingredients"), Rows.Num());
}

void UIngredientRegistrySubsystem::BuildCache()
{
    CachedResourcesByBiome.Empty();
    CachedWeightsByBiome.Empty();

    for (const auto& Pair : Rows)
    {
        for (EBiomeType Biome : Pair.Value.AllowedBiomes)
        {
            CachedResourcesByBiome.FindOrAdd(Biome).Add(Pair.Key);
            CachedWeightsByBiome.FindOrAdd(Biome).Add(Pair.Value.RarityWeight);
        }
    }
}

const FIngredientTableRow* UIngredientRegistrySubsystem::GetRow(FName IngredientID) const
{
    if (!bInitialized) return nullptr;
    return Rows.Find(IngredientID);
}

EIngredientClass UIngredientRegistrySubsystem::Classify(FName IngredientID) const
{
    const FIngredientTableRow* Row = GetRow(IngredientID);
    if (Row) return Row->Class;

    // Проверяем, не является ли ID типом воды из WaterTypeRegistry
    if (UWaterTypeRegistrySubsystem* WaterReg = GetGameInstance()->GetSubsystem<UWaterTypeRegistrySubsystem>())
    {
        if (WaterReg->IsValidWaterType(IngredientID))
            return EIngredientClass::Water;
    }
    return EIngredientClass::Unknown;
}

bool UIngredientRegistrySubsystem::IsWater(FName IngredientID) const
{
    const FIngredientTableRow* Row = GetRow(IngredientID);
    if (Row) return Row->bIsWater;

    // Если строка не найдена, проверяем в реестре типов воды
    if (UWaterTypeRegistrySubsystem* WaterReg = GetGameInstance()->GetSubsystem<UWaterTypeRegistrySubsystem>())
    {
        return WaterReg->IsValidWaterType(IngredientID);
    }
    return false;
}

bool UIngredientRegistrySubsystem::IsKnown(FName IngredientID) const
{
    if (bInitialized && Rows.Contains(IngredientID))
        return true;

    // Также считаем известным, если это валидный тип воды
    if (UWaterTypeRegistrySubsystem* WaterReg = GetGameInstance()->GetSubsystem<UWaterTypeRegistrySubsystem>())
    {
        return WaterReg->IsValidWaterType(IngredientID);
    }
    return false;
}

TArray<FName> UIngredientRegistrySubsystem::GetResourcesForBiome(EBiomeType Biome) const
{
    if (!bInitialized) return TArray<FName>();
    const TArray<FName>* Found = CachedResourcesByBiome.Find(Biome);
    return Found ? *Found : TArray<FName>();
}

FName UIngredientRegistrySubsystem::GetRandomResourceForBiome(EBiomeType Biome, FRandomStream& Rng) const
{
    const TArray<FName>* Candidates = CachedResourcesByBiome.Find(Biome);
    if (!Candidates || Candidates->Num() == 0) return NAME_None;
    if (Candidates->Num() == 1) return (*Candidates)[0];

    const TArray<int32>* Weights = CachedWeightsByBiome.Find(Biome);
    if (!Weights || Weights->Num() != Candidates->Num()) return (*Candidates)[0];

    int32 TotalWeight = 0;
    for (int32 W : *Weights) TotalWeight += W;
    if (TotalWeight <= 0) return (*Candidates)[0];

    int32 Roll = Rng.RandRange(1, TotalWeight);
    int32 Accum = 0;
    for (int32 i = 0; i < Candidates->Num(); ++i)
    {
        Accum += (*Weights)[i];
        if (Roll <= Accum) return (*Candidates)[i];
    }
    return Candidates->Last();
}

void UIngredientRegistrySubsystem::Reset()
{
    Rows.Empty();
    CachedResourcesByBiome.Empty();
    CachedWeightsByBiome.Empty();
    bInitialized = false;
}