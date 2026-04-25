// HarvestService.h
#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Core/Types/HerbalistCoreTypes.h"
#include "HarvestService.generated.h"

class UHerbalistIngredient;

UCLASS(BlueprintType)
class PROJECTHERBALIST_API UHarvestService : public UObject
{
    GENERATED_BODY()

public:
    static UHerbalistIngredient* LoadIngredientAssetStatic(FName IngredientID);

    UFUNCTION(BlueprintCallable, Category = "Harvest")
    UHerbalistIngredient* LoadIngredientAsset(FName IngredientID) const;

    UFUNCTION(BlueprintCallable, Category = "Harvest")
    FRealState GetBaseResourceParams(FName IngredientID) const;

    UFUNCTION(BlueprintCallable, Category = "Harvest")
    FRealState Harvest(FName IngredientID, const FRealState& BiomeState, const FConditionModifier& Conditions) const;

    UFUNCTION(BlueprintCallable, Category = "Harvest")
    FRealState HarvestWater(const FRealState& WaterState, const FConditionModifier& Conditions) const;

    UHarvestService* GetHarvestService() { return this; }
};