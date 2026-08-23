#include "HerbalistNameUtils.h"
#include "Core/Subsystems/IngredientRegistrySubsystem.h"
#include "Core/Data/IngredientTableRow.h"

FText GeneratePotionName(const FRealState& State)
{
    struct FAxisInfo { FString Name; float Value; };
    FAxisInfo Axes[] = {
        {TEXT("Телесное"), State.Direction.Body},
        {TEXT("Разумное"), State.Direction.Mind},
        {TEXT("Духовное"), State.Direction.Spirit},
        {TEXT("Природное"), State.Direction.Nature}
    };
    int Best = 0;
    for (int i = 1; i < 4; ++i)
        if (Axes[i].Value > Axes[Best].Value) Best = i;

    FString MainAxis = Axes[Best].Name;
    float Distortion = State.Meta.Distortion;
    float Purity = State.Meta.Purity;

    FString Quality;
    if (Distortion < 0.3f)
        Quality = TEXT("Чистое");
    else if (Distortion < 0.6f)
        Quality = TEXT("Мутное");
    else
        Quality = TEXT("Искажённое");

    if (Distortion < 0.5f && Purity > 0.5f && State.Meta.Stability > 0.5f)
        Quality = TEXT("Очищенное");

    return FText::FromString(FString::Printf(TEXT("%s %s зелье"), *Quality, *MainAxis));
}

FString GetItemDisplayName(const FInventoryItem& Item, UIngredientRegistrySubsystem* Registry)
{
    if (Item.IngredientID == FName(TEXT("Potion")))
    {
        return GeneratePotionName(Item.State).ToString();
    }
    if (Item.IngredientID == FName(TEXT("Ash")))
    {
        return TEXT("Зола");
    }
    if (Item.IngredientID == FName(TEXT("BoiledWater")))
    {
        return TEXT("Кипячёная вода");
    }
    if (Item.IngredientID == FName(TEXT("Water")))
    {
        return TEXT("Вода");
    }
    if (Registry)
    {
        if (const FIngredientTableRow* Row = Registry->GetRow(Item.IngredientID))
        {
            return Row->DisplayName.ToString();
        }
    }
    return Item.IngredientID.ToString();
}
