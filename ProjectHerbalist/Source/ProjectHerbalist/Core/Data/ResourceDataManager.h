// ResourceDataManager.h
#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Engine/DataTable.h"
#include "Core/Types/ResourceBalanceRow.h"
#include "Core/Types/BiomeRow.h"
#include "Core/Types/HerbalistItemData.h"
#include "ResourceDataManager.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogResourceData, Log, All);

UCLASS(BlueprintType)
class PROJECTHERBALIST_API UResourceDataManager : public UObject
{
    GENERATED_BODY()

public:
    // Инициализация (BlueprintCallable оставляем)
    UFUNCTION(BlueprintCallable, Category = "ResourceData")
    void Initialize(
        UDataTable* InResourceBalanceTable,
        UDataTable* InBiomeTable,
        UDataTable* InWaterTable
    );

    // Доступ к таблице баланса ресурсов (только C++, убираем UFUNCTION)
    const FResourceBalanceRow* GetResourceBalanceRow(FName PrimaryAssetId) const;
    TArray<const FResourceBalanceRow*> GetAllResourceBalanceRows() const;

    // Доступ к таблице биомов (только C++)
    const FBiomeRow* GetBiomeRow(EBiomeType Biome) const;

    // Загрузка DataAsset (только C++, возвращает UObject*, можно UFUNCTION, но осторожно)
    UHerbalistItemData* GetItemData(FName PrimaryAssetId) const;

    // Вспомогательные методы (только C++)
    void GetSpawnableResources(EBiomeType Biome, ESeasonMask Season, ETimeOfDayMask TimeOfDay,
        TArray<FName>& OutResourceIds, TArray<int32>& OutWeights) const;

    // Синглтон
    static UResourceDataManager* GetInstance();

protected:
    UPROPERTY()
    TObjectPtr<UDataTable> ResourceBalanceTable = nullptr;

    UPROPERTY()
    TObjectPtr<UDataTable> BiomeTable = nullptr;

    UPROPERTY()
    TObjectPtr<UDataTable> WaterTable = nullptr;

    TMap<EBiomeType, const FBiomeRow*> BiomeRowCache;

    static UResourceDataManager* Instance;
};