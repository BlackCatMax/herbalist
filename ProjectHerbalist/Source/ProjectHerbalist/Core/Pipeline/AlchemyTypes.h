// AlchemyTypes.h
#pragma once
#include "CoreMinimal.h"
#include "Core/Types/HerbalistCoreTypes.h"
#include "AlchemyTypes.generated.h"

USTRUCT(BlueprintType)
struct PROJECTHERBALIST_API FAlchemyAtom
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    FName SourceID = NAME_None;

    UPROPERTY(BlueprintReadOnly)
    FRealState State;

    UPROPERTY(BlueprintReadOnly)
    bool bIsWater = false;

    UPROPERTY(BlueprintReadOnly)
    FName OriginBiome = NAME_None;

    FAlchemyAtom() = default;
    FAlchemyAtom(const FInventoryItem& Item, FName Biome = NAME_None);
};