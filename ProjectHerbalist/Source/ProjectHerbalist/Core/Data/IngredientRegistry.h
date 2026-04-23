// IngredientRegistry.h
#pragma once
#include "CoreMinimal.h"

UENUM(BlueprintType)
enum class EIngredientClass : uint8
{
    Water,
    Herb,
    Mushroom,
    Berry,
    Wood,
    Moss,
    Mineral,
    Essence,
    Catalyst,
    Unknown
};

class PROJECTHERBALIST_API FIngredientRegistry
{
public:
    static void Initialize();
    static EIngredientClass GetClass(FName IngredientID);
    static bool IsWater(FName IngredientID) { return GetClass(IngredientID) == EIngredientClass::Water; }

private:
    static TMap<FName, EIngredientClass> Registry;
};