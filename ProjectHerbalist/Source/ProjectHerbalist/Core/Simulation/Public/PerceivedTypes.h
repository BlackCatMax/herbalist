// Core/Simulation/Public/PerceivedTypes.h
#pragma once

#include "CoreMinimal.h"
#include "Core/Types/HerbalistCoreTypes.h"
#include "PerceivedTypes.generated.h"

USTRUCT(BlueprintType)
struct PROJECTHERBALIST_API FPerceivedCell
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite)
    FIntPoint Coord;

    UPROPERTY(BlueprintReadWrite)
    FRealState PerceivedState;

    UPROPERTY(BlueprintReadWrite)
    bool bIsVisible = true;
};

USTRUCT(BlueprintType)
struct PROJECTHERBALIST_API FPerceivedWorld
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite)
    TMap<FIntPoint, FPerceivedCell> Cells;

    UPROPERTY(BlueprintReadWrite)
    int32 WorldSeed = 0;
};

// Обычная C++ структура, не видна UHT
struct FPerceivedInventory
{
    TMap<int32, TArray<FInventoryItem>> ContainerContents;
};