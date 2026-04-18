// HerbalistItemData.h
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Core/Types/HerbalistCoreTypes.h"
#include "HerbalistItemData.generated.h"

UCLASS(BlueprintType)
class PROJECTHERBALIST_API UHerbalistItemData : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    // UI
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI")
    FText DisplayName;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI")
    FText Description;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI")
    TSoftObjectPtr<UTexture2D> Icon;

    // Базовые параметры состояния (могут быть переопределены балансом, но хранятся здесь)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats")
    FRealState BaseState;

    // Тип ресурса (для быстрого доступа, без загрузки всего ассета)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats")
    EResourceType ResourceType = EResourceType::None;

    // PrimaryAssetId для асинхронной загрузки
    virtual FPrimaryAssetId GetPrimaryAssetId() const override;
};