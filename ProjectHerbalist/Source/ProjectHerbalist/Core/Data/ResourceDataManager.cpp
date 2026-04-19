// ResourceDataManager.cpp
#include "Core/Data/ResourceDataManager.h"
#include "ProjectHerbalist.h"
#include "Engine/DataTable.h"
#include "Core/Types/BiomeTypes.h"

DEFINE_LOG_CATEGORY(LogResourceData);

UResourceDataManager* UResourceDataManager::Instance = nullptr;

void UResourceDataManager::Initialize(
    UDataTable* InResourceBalanceTable,
    UDataTable* InBiomeTable,
    UDataTable* InWaterTable)
{
    ResourceBalanceTable = InResourceBalanceTable;
    BiomeTable = InBiomeTable;
    WaterTable = InWaterTable;
    Instance = this;

    BiomeRowCache.Empty();
    if (BiomeTable)
    {
        TArray<FName> RowNames = BiomeTable->GetRowNames();
        for (const FName& RowName : RowNames)
        {
            FBiomeRow* Row = BiomeTable->FindRow<FBiomeRow>(RowName, TEXT("Initialize"));
            if (Row)
            {
                EBiomeType Biome = FBiomeDefaults::NameToBiomeType(RowName);
                BiomeRowCache.Add(Biome, Row);
            }
        }
    }

    UE_LOG(LogResourceData, Log, TEXT("ResourceDataManager initialized. Balance rows: %d, Biome rows: %d"),
        ResourceBalanceTable ? ResourceBalanceTable->GetRowNames().Num() : 0,
        BiomeRowCache.Num());
}

const FResourceBalanceRow* UResourceDataManager::GetResourceBalanceRow(FName PrimaryAssetId) const
{
    if (!ResourceBalanceTable) return nullptr;
    return ResourceBalanceTable->FindRow<FResourceBalanceRow>(PrimaryAssetId, TEXT("GetResourceBalanceRow"));
}

TArray<const FResourceBalanceRow*> UResourceDataManager::GetAllResourceBalanceRows() const
{
    TArray<const FResourceBalanceRow*> Rows;
    if (!ResourceBalanceTable) return Rows;

    TArray<FName> RowNames = ResourceBalanceTable->GetRowNames();
    for (const FName& Name : RowNames)
    {
        const FResourceBalanceRow* Row = ResourceBalanceTable->FindRow<FResourceBalanceRow>(Name, TEXT("GetAll"));
        if (Row) Rows.Add(Row);
    }
    return Rows;
}

const FBiomeRow* UResourceDataManager::GetBiomeRow(EBiomeType Biome) const
{
    if (const FBiomeRow* const* Found = BiomeRowCache.Find(Biome))
        return *Found;
    return nullptr;
}

UHerbalistItemData* UResourceDataManager::GetItemData(FName PrimaryAssetId) const
{
    if (PrimaryAssetId == NAME_None) return nullptr;

    FString AssetName = PrimaryAssetId.ToString();
    FString Path = FString::Printf(TEXT("/Game/Data/Items/%s.%s"), *AssetName, *AssetName);
    UHerbalistItemData* Asset = LoadObject<UHerbalistItemData>(nullptr, *Path);

    if (!Asset)
    {
        UE_LOG(LogResourceData, Warning, TEXT("GetItemData: Failed to load '%s' from '%s'"), *AssetName, *Path);
    }
    else
    {
        UE_LOG(LogResourceData, Log, TEXT("GetItemData: Loaded '%s'"), *AssetName);
    }
    return Asset;
}

void UResourceDataManager::GetSpawnableResources(
    EBiomeType Biome,
    ESeasonMask Season,
    ETimeOfDayMask TimeOfDay,
    TArray<FName>& OutResourceIds,
    TArray<int32>& OutWeights) const
{
    OutResourceIds.Empty();
    OutWeights.Empty();

    if (!ResourceBalanceTable) return;

    int32 SeasonFlag = static_cast<int32>(Season);
    int32 TimeFlag = static_cast<int32>(TimeOfDay);

    TArray<FName> RowNames = ResourceBalanceTable->GetRowNames();
    for (const FName& Name : RowNames)
    {
        const FResourceBalanceRow* Row = ResourceBalanceTable->FindRow<FResourceBalanceRow>(Name, TEXT("GetSpawnable"));
        if (!Row || Row->RarityWeight <= 0) continue;

        // Проверка по массиву биомов
        if (!Row->AllowedBiomes.Contains(Biome)) continue;

        if (Row->SeasonMask != 0 && (Row->SeasonMask & SeasonFlag) == 0) continue;
        if (Row->TimeOfDayMask != 0 && (Row->TimeOfDayMask & TimeFlag) == 0) continue;

        OutResourceIds.Add(Row->PrimaryAssetId);
        OutWeights.Add(Row->RarityWeight);
    }
}

UResourceDataManager* UResourceDataManager::GetInstance()
{
    return Instance;
}