#include "UI/ItemTooltipWidget.h"
#include "Components/TextBlock.h"
#include "Core/Types/HerbalistCoreTypes.h"
#include "Core/Types/HerbalistNameUtils.h"

void UItemTooltipWidget::SetItem(const FInventoryItem& Item)
{
    if (Item.IsEmpty()) return;

    // --- Имя предмета ---
    FString Name;
    if (Item.IngredientID == FName(TEXT("Potion")))
        Name = GeneratePotionName(Item.State).ToString();
    else if (Item.IngredientID == FName(TEXT("Ash")))
        Name = TEXT("Зола");
    else if (Item.IngredientID == FName(TEXT("BoiledWater")))
        Name = TEXT("Кипячёная вода");
    else if (Item.IngredientID == FName(TEXT("Water")))
        Name = TEXT("Вода");
    else
        Name = Item.IngredientID.ToString();  // временно, потом заменим на DisplayName

    if (NameText) NameText->SetText(FText::FromString(Name));
    if (TypeText) TypeText->SetText(FText::FromString(TEXT("Ингредиент")));

    // Item уже искажён (S_perceived) — просто отображаем, без второго слоя шума
    // и без сравнения с реальным значением: игрок не должен иметь возможность
    // сверить искажённое число с настоящим.
    auto SetLine = [](UTextBlock* TextBlock, float Value, const FString& Label)
    {
        if (!TextBlock) return;
        TextBlock->SetText(FText::FromString(FString::Printf(TEXT("%s: %.2f"), *Label, Value)));
    };

    SetLine(MagnitudeText,    Item.State.Magnitude,          TEXT("Сила"));
    SetLine(DistortionText,   Item.State.Meta.Distortion,    TEXT("Искажение"));
    SetLine(PurityText,       Item.State.Meta.Purity,        TEXT("Чистота"));
    SetLine(StabilityText,    Item.State.Meta.Stability,     TEXT("Стабильность"));
    SetLine(PotencyText,      Item.State.Meta.Potency,       TEXT("Мощность"));
    SetLine(ResonanceText,    Item.State.Meta.Resonance,     TEXT("Резонанс"));
    SetLine(CorruptionText,   Item.State.Meta.Corruption,    TEXT("Порча"));
    SetLine(DirectionBodyText,   Item.State.Direction.Body,   TEXT("Тело"));
    SetLine(DirectionMindText,   Item.State.Direction.Mind,   TEXT("Разум"));
    SetLine(DirectionSpiritText, Item.State.Direction.Spirit, TEXT("Дух"));
    SetLine(DirectionNatureText, Item.State.Direction.Nature, TEXT("Природа"));
}
