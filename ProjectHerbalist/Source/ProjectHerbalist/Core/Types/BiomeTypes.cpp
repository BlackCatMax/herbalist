// BiomeTypes.cpp
#include "BiomeTypes.h"
#include "ProjectHerbalist.h"
#include "Core/Data/ResourceDataManager.h"
#include "Core/Types/BiomeRow.h"
#include "Core/Types/ResourceBalanceRow.h"

// BiomeTypes.cpp (фрагмент с маппингом)
static const TMap<EBiomeType, FName> BiomeToNameMap = {
    { EBiomeType::Tundra,          TEXT("Tundra") },
    { EBiomeType::Taiga,           TEXT("Taiga") },
    { EBiomeType::MixedForest,     TEXT("MixedForest") },
    { EBiomeType::BroadleafForest, TEXT("BroadleafForest") },
    { EBiomeType::ForestSteppe,    TEXT("ForestSteppe") },
    { EBiomeType::Steppe,          TEXT("Steppe") },
    { EBiomeType::Floodplain,      TEXT("Floodplain") },
    { EBiomeType::Bog,             TEXT("Bog") }
};

static const TMap<FName, EBiomeType> NameToBiomeMap = []()
    {
        TMap<FName, EBiomeType> Map;
        for (const auto& Pair : BiomeToNameMap)
            Map.Add(Pair.Value, Pair.Key);
        return Map;
    }();

FName FBiomeDefaults::BiomeTypeToName(EBiomeType Biome)
{
    if (const FName* Name = BiomeToNameMap.Find(Biome))
        return *Name;
    return TEXT("MixedForest");
}

EBiomeType FBiomeDefaults::NameToBiomeType(FName Name)
{
    if (const EBiomeType* Biome = NameToBiomeMap.Find(Name))
        return *Biome;
    return EBiomeType::MixedForest;
}

TArray<EBiomeType> FBiomeDefaults::GetAllBiomeTypes()
{
    TArray<EBiomeType> Types;
    BiomeToNameMap.GenerateKeyArray(Types);
    return Types;
}

FRealState FBiomeDefaults::GetDefaultState(EBiomeType Biome)
{
    UResourceDataManager* Manager = UResourceDataManager::GetInstance();
    if (!Manager) return FRealState();

    const FBiomeRow* Row = Manager->GetBiomeRow(Biome);
    if (!Row) return FRealState();

    FRealState State;
    State.Direction = Row->Direction;
    State.Magnitude = Row->Magnitude;
    State.Meta = Row->Meta;
    State.Direction.NormalizeSum();
    // клиппинг
    State.Magnitude = FMath::Clamp(State.Magnitude, 0.0f, 1.0f);
    State.Meta.Distortion = FMath::Clamp(State.Meta.Distortion, 0.0f, 1.0f);
    State.Meta.Stability = FMath::Clamp(State.Meta.Stability, 0.0f, 1.0f);
    State.Meta.Purity = FMath::Clamp(State.Meta.Purity, 0.0f, 1.0f);
    State.Meta.Potency = FMath::Clamp(State.Meta.Potency, 0.0f, 1.0f);
    State.Meta.Resonance = FMath::Clamp(State.Meta.Resonance, 0.0f, 1.0f);
    State.Meta.Corruption = FMath::Clamp(State.Meta.Corruption, 0.0f, 1.0f);
    return State;
}

FEnvironment FBiomeDefaults::GetDefaultEnvironment(EBiomeType Biome)
{
    UResourceDataManager* Manager = UResourceDataManager::GetInstance();
    if (!Manager) return FEnvironment();
    const FBiomeRow* Row = Manager->GetBiomeRow(Biome);
    if (!Row) return FEnvironment();
    return Row->Environment;
}

FRealState FBiomeDefaults::GetDefaultWaterState(EBiomeType Biome)
{
    UResourceDataManager* Manager = UResourceDataManager::GetInstance();
    if (!Manager) return FRealState();
    const FBiomeRow* Row = Manager->GetBiomeRow(Biome);
    if (!Row) return FRealState();
    return Row->DefaultWaterState;
}

EResourceType FBiomeDefaults::GetRandomResourceForBiome(EBiomeType Biome, FRandomStream& Rng,
    ESeasonMask Season, ETimeOfDayMask TimeOfDay)
{
    UResourceDataManager* Manager = UResourceDataManager::GetInstance();
    if (!Manager) return EResourceType::Nettle;

    TArray<FName> ResourceIds;
    TArray<int32> Weights;
    Manager->GetSpawnableResources(Biome, Season, TimeOfDay, ResourceIds, Weights);

    if (ResourceIds.Num() == 0) return EResourceType::Nettle;

    int32 TotalWeight = 0;
    for (int32 w : Weights) TotalWeight += w;
    if (TotalWeight <= 0) return EResourceType::Nettle;

    int32 Roll = Rng.RandRange(1, TotalWeight);
    int32 Accum = 0;
    for (int32 i = 0; i < ResourceIds.Num(); ++i)
    {
        Accum += Weights[i];
        if (Roll <= Accum)
        {
            const FResourceBalanceRow* Row = Manager->GetResourceBalanceRow(ResourceIds[i]);
            if (Row) return Row->ResourceType;   // теперь поле ResourceType существует
            break;
        }
    }
    return EResourceType::Nettle;
}