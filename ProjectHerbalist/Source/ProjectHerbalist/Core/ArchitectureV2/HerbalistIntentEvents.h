#pragma once

#include "CoreMinimal.h"
#include "HerbalistIntentEvents.generated.h"

USTRUCT(BlueprintType)
struct FHerbalistIntentHarvestResource
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FIntPoint Cell = FIntPoint::ZeroValue;
};

USTRUCT(BlueprintType)
struct FHerbalistIntentCollectWater
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FIntPoint Cell = FIntPoint::ZeroValue;
};

USTRUCT(BlueprintType)
struct FHerbalistIntentTransferItem
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 FromContainerID = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 ToContainerID = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FName IngredientID = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 Count = 1;
};
