#pragma once

#include "CoreMinimal.h"
#include "Engine/PrimaryDataAsset.h"
#include "Engine/DataTable.h"
#include "HerbalistAssetCatalog.generated.h"

UCLASS(BlueprintType)
class PROJECTHERBALIST_API UHerbalistAssetCatalog : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Herbalist|Catalog")
    TSoftObjectPtr<UDataTable> IngredientTable;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Herbalist|Catalog")
    TSoftObjectPtr<UDataTable> WaterTypeTable;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Herbalist|Catalog")
    TSoftObjectPtr<UDataTable> BiomeDefaultsTable;
};
