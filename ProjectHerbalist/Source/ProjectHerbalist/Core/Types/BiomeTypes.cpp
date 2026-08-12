// BiomeTypes.cpp
#include "BiomeTypes.h"
#include "ProjectHerbalist.h"
#include "HerbalistLogChannels.h"
#include "Core/Types/HerbalistIngredient.h"
#include "Core/Types/BiomeRow.h"
#include "Engine/DataTable.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/AssetManager.h"

static UDataTable* BiomeDataTable = nullptr;

void FBiomeDefaults::SetBiomeTable(UDataTable* InTable)
{
    BiomeDataTable = InTable;
}

const FBiomeRow* FBiomeDefaults::GetBiomeRow(EBiomeType Biome)
{
    if (!BiomeDataTable) return nullptr;
    FName RowName = BiomeTypeToName(Biome);
    return BiomeDataTable->FindRow<FBiomeRow>(RowName, TEXT("GetBiomeRow"));
}

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
    const FBiomeRow* Row = GetBiomeRow(Biome);
    if (!Row) return FRealState();

    FRealState State;
    State.Direction = Row->Direction;
    State.Magnitude = Row->Magnitude;
    State.Meta = Row->Meta;
    State.Direction.NormalizeSum();
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
    const FBiomeRow* Row = GetBiomeRow(Biome);
    if (!Row) return FEnvironment();
    return Row->Environment;
}

FRealState FBiomeDefaults::GetDefaultWaterState(EBiomeType Biome)
{
    const FBiomeRow* Row = GetBiomeRow(Biome);
    if (!Row) return FRealState();
    return Row->DefaultWaterState;
}
