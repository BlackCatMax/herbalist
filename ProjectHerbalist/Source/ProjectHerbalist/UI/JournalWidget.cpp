// JournalWidget.cpp
#include "UI/JournalWidget.h"
#include "UI/JournalEntryRowWidget.h"
#include "Components/VerticalBox.h"
#include "Core/Journal/HerbalistJournalComponent.h"
#include "Core/Subsystems/IngredientRegistrySubsystem.h"
#include "Kismet/GameplayStatics.h"

void UJournalWidget::BindJournal(UHerbalistJournalComponent* InJournal)
{
    if (JournalComponent)
    {
        JournalComponent->OnJournalEntryAdded.RemoveDynamic(this, &UJournalWidget::OnJournalEntryAdded);
    }
    JournalComponent = InJournal;
    if (JournalComponent)
    {
        JournalComponent->OnJournalEntryAdded.AddDynamic(this, &UJournalWidget::OnJournalEntryAdded);
        RefreshDisplay();
    }
}

void UJournalWidget::NativeConstruct()
{
    Super::NativeConstruct();
}

void UJournalWidget::OnJournalEntryAdded()
{
    RefreshDisplay();
}

void UJournalWidget::RefreshDisplay()
{
    if (!JournalComponent || !EntryContainer || !RowWidgetClass)
        return;

    EntryContainer->ClearChildren();

    UIngredientRegistrySubsystem* IngSub = nullptr;
    if (UGameInstance* GI = GetGameInstance())
    {
        IngSub = GI->GetSubsystem<UIngredientRegistrySubsystem>();
    }

    // Новые сверху — самые свежие наблюдения обычно интереснее при сравнении
    // "что я только что собрал/сварил похожего раньше".
    const TArray<FJournalEntry>& Entries = JournalComponent->GetEntries();
    for (int32 i = Entries.Num() - 1; i >= 0; --i)
    {
        UJournalEntryRowWidget* Row = CreateWidget<UJournalEntryRowWidget>(GetWorld(), RowWidgetClass);
        if (Row)
        {
            Row->InitializeRow(Entries[i], IngSub);
            EntryContainer->AddChildToVerticalBox(Row);
        }
    }
}
