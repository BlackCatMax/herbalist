#include "UI/ItemTooltipWidget.h"
#include "Components/TextBlock.h"
#include "Core/Types/HerbalistCoreTypes.h"
#include "Core/Types/HerbalistNameUtils.h"

// Локальная функция искажения
static float PerceiveValue(float RealValue, float GlobalDistortion, FRandomStream& Rng)
{
    if (GlobalDistortion <= 0.0f) return RealValue;
    // Шум в диапазоне +/- GlobalDistortion * 0.5
    float Noise = Rng.FRandRange(-GlobalDistortion * 0.5f, GlobalDistortion * 0.5f);
    return FMath::Clamp(RealValue + Noise, 0.0f, 1.0f);
}

void UItemTooltipWidget::SetItem(const FInventoryItem& Item, float GlobalDistortion)
{
    if (Item.IsEmpty()) return;

    // Генератор случайных чисел с seed на основе ID предмета и искажения
    uint32 Seed = GetTypeHash(Item.IngredientID) ^ static_cast<uint32>(GlobalDistortion * 10000.0f);
    FRandomStream Rng(Seed);

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

    // Вспомогательная лямбда
    auto SetLine = [&](UTextBlock* TextBlock, float RealValue, const FString& Label)
    {
        if (!TextBlock) return;
        float Perceived = PerceiveValue(RealValue, GlobalDistortion, Rng);
        FString Text = FString::Printf(TEXT("%s: %.3f (%.3f)"), *Label, Perceived, RealValue);
        TextBlock->SetText(FText::FromString(Text));
        // Подсветка, если искажение сильное
        FLinearColor Color = FMath::Abs(Perceived - RealValue) > 0.05f 
            ? FLinearColor(1.0f, 0.3f, 0.3f) 
            : FLinearColor(0.9f, 0.9f, 0.9f);
        TextBlock->SetColorAndOpacity(Color);
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