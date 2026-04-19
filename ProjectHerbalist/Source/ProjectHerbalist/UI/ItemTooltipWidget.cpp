#include "UI/ItemTooltipWidget.h"
#include "Components/TextBlock.h"
#include "Core/Harvest/HerbalistHarvest.h"

void UItemTooltipWidget::SetItem(const FInventoryItem& Item)
{
    if (Item.IsEmpty())
        return;

    // Имя предмета
    if (NameText)
    {
        FString Name = FHerbalistHarvest::GetResourceName(Item.Type, false);
        NameText->SetText(FText::FromString(Name));
    }

    // Тип (для отладки)
    if (TypeText)
    {
        FString TypeStr = UEnum::GetValueAsString(Item.Type);
        TypeText->SetText(FText::FromString(TypeStr));
    }

    const FRealState& S = Item.State;

    // Числовые параметры
    if (MagnitudeText)
        MagnitudeText->SetText(FText::FromString(FString::Printf(TEXT("Сила: %.3f"), S.Magnitude)));

    if (DistortionText)
        DistortionText->SetText(FText::FromString(FString::Printf(TEXT("Искажение: %.3f"), S.Meta.Distortion)));

    if (PurityText)
        PurityText->SetText(FText::FromString(FString::Printf(TEXT("Чистота: %.3f"), S.Meta.Purity)));

    if (StabilityText)
        StabilityText->SetText(FText::FromString(FString::Printf(TEXT("Стабильность: %.3f"), S.Meta.Stability)));

    if (PotencyText)
        PotencyText->SetText(FText::FromString(FString::Printf(TEXT("Мощность: %.3f"), S.Meta.Potency)));

    if (ResonanceText)
        ResonanceText->SetText(FText::FromString(FString::Printf(TEXT("Резонанс: %.3f"), S.Meta.Resonance)));

    if (CorruptionText)
        CorruptionText->SetText(FText::FromString(FString::Printf(TEXT("Порча: %.3f"), S.Meta.Corruption)));

    // Направления
    if (DirectionBodyText)
        DirectionBodyText->SetText(FText::FromString(FString::Printf(TEXT("Тело: %.3f"), S.Direction.Body)));

    if (DirectionMindText)
        DirectionMindText->SetText(FText::FromString(FString::Printf(TEXT("Разум: %.3f"), S.Direction.Mind)));

    if (DirectionSpiritText)
        DirectionSpiritText->SetText(FText::FromString(FString::Printf(TEXT("Дух: %.3f"), S.Direction.Spirit)));

    if (DirectionNatureText)
        DirectionNatureText->SetText(FText::FromString(FString::Printf(TEXT("Природа: %.3f"), S.Direction.Nature)));
}