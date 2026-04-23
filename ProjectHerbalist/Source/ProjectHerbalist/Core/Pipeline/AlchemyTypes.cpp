// AlchemyTypes.cpp
#include "AlchemyTypes.h"
#include "Core/Types/HerbalistIngredient.h"
#include "Core/Harvest/HarvestService.h"

FAlchemyAtom::FAlchemyAtom(const FInventoryItem& Item, FName Biome)
{
    SourceID = Item.IngredientID;
    State = Item.State;
    OriginBiome = Biome;

    if (Item.IngredientID == FName(TEXT("Water")))
    {
        bIsWater = true;
    }
    else if (Item.IngredientID == FName(TEXT("Potion")) || Item.IngredientID.IsNone())
    {
        bIsWater = false;
    }
    else
    {
        UHerbalistIngredient* Ingredient = UHarvestService::LoadIngredientAssetStatic(Item.IngredientID);
        bIsWater = Ingredient && Ingredient->bIsWater;
    }
}