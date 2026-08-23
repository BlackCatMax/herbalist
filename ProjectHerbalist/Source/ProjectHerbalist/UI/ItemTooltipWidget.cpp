#include "UI/ItemTooltipWidget.h"
#include "Components/TextBlock.h"
#include "Core/Types/HerbalistCoreTypes.h"
#include "Core/Types/HerbalistNameUtils.h"
#include "Player/HerbalistPlayerController.h"

void UItemTooltipWidget::SetItem(const FInventoryItem& Item)
{
    if (Item.IsEmpty()) return;

    // Раньше здесь были ещё раз захардкожены ветки Ash/BoiledWater/Water —
    // третье место с той же дублированной логикой (AUDIT_AND_REFACTORING_PLAN
    // §2.4 нашёл её в двух других виджетах, эта третья не была найдена и там,
    // и в мета-аудите). Общий хелпер вместо третьей копии.
    AHerbalistPlayerController* PC = Cast<AHerbalistPlayerController>(GetOwningPlayer());
    if (NameText) NameText->SetText(FText::FromString(GetItemDisplayName(Item, nullptr)));

    // Морочники (16_Entity_Manifestation §16.5) — раньше PC->CurrentGlobalDistortion
    // считался честно, но не был виден игроку нигде (AUDIT §1.4/META_AUDIT §6).
    // Минимальный шаг: назвать источник там, где он уже ощутимо искажает —
    // используем существующую строку типа предмета, без новых полей в Blueprint.
    FString TypeLine = TEXT("Ингредиент");
    if (PC && PC->CurrentGlobalDistortion > 0.4f)
    {
        TypeLine += TEXT(" · Морочники путают чувства");
    }
    if (TypeText) TypeText->SetText(FText::FromString(TypeLine));

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
