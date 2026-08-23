// JournalEntryRowWidget.h
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Core/Journal/JournalTypes.h"
#include "JournalEntryRowWidget.generated.h"

class UTextBlock;
class UIngredientRegistrySubsystem;

// Одна строка Травника (07_UX §7.2.4) — сравнение записей вручную ("это как
// селекция", обсуждение в сессии 2026-08-24): игрок сам решает, что взять за
// эталон, глядя на несколько искажённых показаний рядом. Поэтому виджет —
// чистое отображение уже замороженного PerceivedState (см. JournalTypes.h),
// без единой попытки посчитать "настоящее" значение или усреднить за игрока.
UCLASS()
class PROJECTHERBALIST_API UJournalEntryRowWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    void InitializeRow(const FJournalEntry& InEntry, UIngredientRegistrySubsystem* IngredientRegistry);

protected:
    UPROPERTY(meta = (BindWidgetOptional))
    UTextBlock* NameText;

    // Биом/время суток/номер клетки — контекст, по которому игрок сам находит
    // закономерности ("собирал ночью — искажение выше", и т.п.), не готовый вывод.
    UPROPERTY(meta = (BindWidgetOptional))
    UTextBlock* ContextText;

    UPROPERTY(meta = (BindWidgetOptional))
    UTextBlock* MagnitudeText;

    UPROPERTY(meta = (BindWidgetOptional))
    UTextBlock* DistortionText;

    UPROPERTY(meta = (BindWidgetOptional))
    UTextBlock* PurityText;

    UPROPERTY(meta = (BindWidgetOptional))
    UTextBlock* CorruptionText;
};
