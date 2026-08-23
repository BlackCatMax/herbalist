// JournalEntryRowWidget.cpp
#include "UI/JournalEntryRowWidget.h"
#include "Components/TextBlock.h"
#include "Core/Types/HerbalistNameUtils.h"
#include "Core/Types/BiomeTypes.h"

void UJournalEntryRowWidget::InitializeRow(const FJournalEntry& InEntry, UIngredientRegistrySubsystem* IngredientRegistry)
{
    // GetItemDisplayName ждёт FInventoryItem — Травнику годится тот же
    // хелпер, что и остальным трём виджетам (см. AUDIT_AND_REFACTORING_PLAN
    // §2.4: раньше Ash/BoiledWater/Water трижды переизобретались по месту),
    // собираем временный item только для имени, не для чего-то ещё.
    FInventoryItem NameLookup;
    NameLookup.IngredientID = InEntry.IngredientID;
    NameLookup.State = InEntry.PerceivedState;
    const FString TypeLabel = InEntry.Type == EJournalEntryType::Harvest ? TEXT("собрано") : TEXT("сварено");
    const FString DisplayName = GetItemDisplayName(NameLookup, IngredientRegistry);

    if (NameText)
    {
        NameText->SetText(FText::FromString(FString::Printf(TEXT("%s (%s) x%d"), *DisplayName, *TypeLabel, InEntry.Count)));
    }

    if (ContextText)
    {
        const FString BiomeLabel = FBiomeDefaults::BiomeTypeToName(InEntry.Biome).ToString();
        ContextText->SetText(FText::FromString(FString::Printf(TEXT("(%d,%d) %s, %s, t=%.0f"),
            InEntry.Cell.X, InEntry.Cell.Y, *BiomeLabel,
            InEntry.bWasNight ? TEXT("ночь") : TEXT("день"), InEntry.GameTimeSeconds)));
    }

    // PerceivedState — как игрок это видел, не как оно было на самом деле
    // (JournalTypes.h). Сравнение нескольких таких строк по одному ингредиенту
    // и есть инструмент прогрессии (06_Progression: игрок сам решает, что
    // взять за эталон) — виджет ничего не усредняет и не подсказывает.
    const FRealState& S = InEntry.PerceivedState;
    auto SetLine = [](UTextBlock* TextBlock, float Value, const FString& Label)
    {
        if (!TextBlock) return;
        TextBlock->SetText(FText::FromString(FString::Printf(TEXT("%s: %.2f"), *Label, Value)));
    };
    SetLine(MagnitudeText,  S.Magnitude,          TEXT("Сила"));
    SetLine(DistortionText, S.Meta.Distortion,    TEXT("Искажение"));
    SetLine(PurityText,     S.Meta.Purity,        TEXT("Чистота"));
    SetLine(CorruptionText, S.Meta.Corruption,     TEXT("Порча"));
}
