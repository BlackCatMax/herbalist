#pragma once

#include "CoreMinimal.h"
#include "HerbalistFactEvents.generated.h"

USTRUCT(BlueprintType)
struct FHerbalistFactInventoryChanged
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 ContainerID = 0;
};

USTRUCT(BlueprintType)
struct FHerbalistFactCellChanged
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FIntPoint Cell = FIntPoint::ZeroValue;
};
