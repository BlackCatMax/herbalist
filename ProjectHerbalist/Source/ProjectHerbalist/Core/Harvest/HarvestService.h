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
    // Статическая версия загрузки ингредиента (используется в статических контекстах, например Pipeline)
    static UHerbalistIngredient* LoadIngredientAssetStatic(FName IngredientID);

    // Нестатическая версия (для использования через экземпляр сервиса)
    UFUNCTION(BlueprintCallable, Category = "Harvest")
    UHerbalistIngredient* LoadIngredientAsset(FName IngredientID) const;

    // Получает базовые параметры ингредиента
    UFUNCTION(BlueprintCallable, Category = "Harvest")
    FRealState GetBaseResourceParams(FName IngredientID) const;

    // Вычисляет итоговое состояние собранного ресурса
    UFUNCTION(BlueprintCallable, Category = "Harvest")
    FRealState Harvest(FName IngredientID, const FRealState& BiomeState, const FConditionModifier& Conditions) const;

    // Вычисляет состояние собранной воды
    UFUNCTION(BlueprintCallable, Category = "Harvest")
    FRealState HarvestWater(const FRealState& WaterState, const FConditionModifier& Conditions) const;
};
