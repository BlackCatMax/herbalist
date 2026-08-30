// JournalLogWidget.h
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "JournalLogWidget.generated.h"

class UHerbalistJournalComponent;
class UIngredientRegistrySubsystem;
class UVerticalBox;
class UScrollBox;
class UTextBlock;
class AGridWorldManager;
struct FJournalEntry;

// Травник как читаемый лог (2026-08-29, по прямому решению пользователя:
// "Прогрессию (Травник) предлагаю писать пока в виде лога отдельного, чтобы
// можно было его открыть и почитать"). JournalWidget/JournalEntryRowWidget
// (UI/JournalWidget.h) не тронуты — та более богатая раскладка (отдельные
// текстовые поля на каждую ось) остаётся рабочим путём для будущего WBP в
// редакторе, C++ не может создать сам .uasset. Этот виджет решает ту же
// задачу без единого BindWidget вовсе: строит своё дерево целиком в
// NativeConstruct через WidgetTree->ConstructWidget, значит не нуждается ни
// в каком .uasset и работает "из коробки" — CreateWidget<UJournalLogWidget>
// с голым StaticClass() уже достаточно.
UCLASS()
class PROJECTHERBALIST_API UJournalLogWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    void BindJournal(UHerbalistJournalComponent* InJournal);

    // Ясность (AGridWorldManager::GetGlobalPerceptionClarity, "Прогрессия/
    // Заряна" 2026-08-29) читается отсюда живьём в RefreshDisplay, не
    // кэшируется отдельным полем на момент открытия — иначе значение
    // застыло бы, пока экран открыт, а сбор фрагмента как раз может
    // произойти в любой момент (тут не произойдёт, экран модальный, но
    // отдельная сущность истины лучше молчаливого допущения).
    void BindWorldManager(AGridWorldManager* InWorldManager);

protected:
    virtual void NativeConstruct() override;

private:
    UPROPERTY()
    UHerbalistJournalComponent* JournalComponent = nullptr;

    UPROPERTY()
    AGridWorldManager* WorldManagerRef = nullptr;

    UPROPERTY()
    UVerticalBox* EntryList = nullptr;

    UPROPERTY()
    UTextBlock* ClarityText = nullptr;

    UFUNCTION()
    void OnJournalEntryAdded();

    void BuildLayout();
    void RefreshDisplay();
    FText FormatEntry(const FJournalEntry& Entry, UIngredientRegistrySubsystem* IngredientRegistry) const;
};
