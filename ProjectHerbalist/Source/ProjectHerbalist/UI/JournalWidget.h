// JournalWidget.h
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "JournalWidget.generated.h"

class UVerticalBox;
class UJournalEntryRowWidget;
class UHerbalistJournalComponent;

// Экран Травника (07_UX §7.2.4) — плоский список записей, новые сверху.
// v1: без группировки по ингредиенту и без сортировки/фильтра — тот же
// вертикальный срез, что и у самого HerbalistJournalComponent ("минимальная
// реализация... без подсветки закономерностей и сравнения" — здесь именно
// сравнение и появляется, но в самом простом виде: глазами, по списку).
UCLASS()
class PROJECTHERBALIST_API UJournalWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    void BindJournal(UHerbalistJournalComponent* InJournal);

protected:
    virtual void NativeConstruct() override;

    UPROPERTY(meta = (BindWidget))
    UVerticalBox* EntryContainer;

    UPROPERTY(EditDefaultsOnly, Category = "Journal")
    TSubclassOf<UJournalEntryRowWidget> RowWidgetClass;

private:
    UPROPERTY()
    UHerbalistJournalComponent* JournalComponent = nullptr;

    UFUNCTION()
    void OnJournalEntryAdded();

    void RefreshDisplay();
};
