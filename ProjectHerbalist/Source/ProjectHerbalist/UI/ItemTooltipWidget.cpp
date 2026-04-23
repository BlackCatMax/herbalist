// ItemTooltipWidget.cpp
#include "UI/ItemTooltipWidget.h"
#include "Components/TextBlock.h"
#include "Core/Types/HerbalistCoreTypes.h"
#include "Core/Pipeline/AlchemySemantics.h"
#include "Core/Data/IngredientRegistry.h"

float PerceiveValue(float RealValue, float Distortion, FRandomStream& Random)
{
    float Noise = (Random.FRand() - 0.5f) * 2.0f * Distortion * 0.3f;
    return FMath::Clamp(RealValue + Noise, 0.0f, 1.0f);
}

FText GeneratePotionName(const FRealState& State)
{
    // Ищем доминирующую ось без сортировки
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

void UItemTooltipWidget::SetItem(const FInventoryItem& Item)
{
    if (Item.IsEmpty()) return;

    TArray<FInventoryItem> DummyInput = { Item };
    EAlchemyOutcome Outcome = HerbalistCore::ClassifyOutcome(DummyInput);

    FString Name;
    if (Item.IngredientID == FName(TEXT("Potion")))
    {
        Name = GeneratePotionName(Item.State).ToString();
    }
    else if (Item.IngredientID == FName(TEXT("Water")))
    {
        Name = TEXT("Вода");
    }
    else
    {
        Name = Item.IngredientID.ToString();
    }
    if (NameText) NameText->SetText(FText::FromString(Name));

    if (TypeText) TypeText->SetText(FText::FromString(Item.IngredientID.ToString()));

    FRandomStream Random(Item.State.Meta.Distortion * 1000.0f + Item.State.Magnitude * 500.0f);

    auto SetDistortedText = [&](UTextBlock* TextBlock, float RealValue, const FString& Label)
    {
        if (!TextBlock) return;
        float Perceived = PerceiveValue(RealValue, Item.State.Meta.Distortion, Random);
        TextBlock->SetText(FText::FromString(FString::Printf(TEXT("%s: %.3f (%.3f)"), *Label, Perceived, RealValue)));
    };

    SetDistortedText(MagnitudeText, Item.State.Magnitude, TEXT("Сила"));
    SetDistortedText(DistortionText, Item.State.Meta.Distortion, TEXT("Искажение"));
    SetDistortedText(PurityText, Item.State.Meta.Purity, TEXT("Чистота"));
    SetDistortedText(StabilityText, Item.State.Meta.Stability, TEXT("Стабильность"));
    SetDistortedText(PotencyText, Item.State.Meta.Potency, TEXT("Мощность"));
    SetDistortedText(ResonanceText, Item.State.Meta.Resonance, TEXT("Резонанс"));
    SetDistortedText(CorruptionText, Item.State.Meta.Corruption, TEXT("Порча"));

    SetDistortedText(DirectionBodyText, Item.State.Direction.Body, TEXT("Тело"));
    SetDistortedText(DirectionMindText, Item.State.Direction.Mind, TEXT("Разум"));
    SetDistortedText(DirectionSpiritText, Item.State.Direction.Spirit, TEXT("Дух"));
    SetDistortedText(DirectionNatureText, Item.State.Direction.Nature, TEXT("Природа"));
}