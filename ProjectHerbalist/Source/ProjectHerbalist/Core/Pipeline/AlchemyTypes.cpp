// AlchemyTypes.cpp
#include "AlchemyTypes.h"
#include "Core/Data/IngredientRegistry.h"   // <-- добавлено

FAlchemyAtom::FAlchemyAtom(const FInventoryItem& Item, FName Biome)
{
    SourceID = Item.IngredientID;
    State = Item.State;
    OriginBiome = Biome;

    bIsWater = FIngredientRegistry::IsWater(Item.IngredientID);
}