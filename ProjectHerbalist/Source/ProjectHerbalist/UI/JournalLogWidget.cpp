// JournalLogWidget.cpp
#include "UI/JournalLogWidget.h"
#include "Core/Journal/HerbalistJournalComponent.h"
#include "Core/Journal/JournalTypes.h"
#include "Core/Types/HerbalistNameUtils.h"
#include "Core/Types/BiomeTypes.h"
#include "Core/Subsystems/IngredientRegistrySubsystem.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/SizeBox.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "Kismet/GameplayStatics.h"

void UJournalLogWidget::BindJournal(UHerbalistJournalComponent* InJournal)
{
    if (JournalComponent)
    {
        JournalComponent->OnJournalEntryAdded.RemoveDynamic(this, &UJournalLogWidget::OnJournalEntryAdded);
    }
    JournalComponent = InJournal;
    if (JournalComponent)
    {
        JournalComponent->OnJournalEntryAdded.AddDynamic(this, &UJournalLogWidget::OnJournalEntryAdded);
        RefreshDisplay();
    }
}

void UJournalLogWidget::NativeConstruct()
{
    Super::NativeConstruct();
    BuildLayout();
    if (JournalComponent)
    {
        RefreshDisplay();
    }
}

// Дерево строится целиком тут -- ни один узел не приходит из .uasset (нет
// ни одного BindWidget), см. комментарий у класса в JournalLogWidget.h.
void UJournalLogWidget::BuildLayout()
{
    if (!WidgetTree) return;

    // Затемнение на весь экран -- корневой виджет UserWidget'а, добавленного
    // через AddToViewport(), сам растягивается на весь вьюпорт, дальше
    // выравниваем содержимое по центру через HAlign/VAlign самого Border.
    UBorder* Backdrop = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("JournalBackdrop"));
    Backdrop->SetBrushColor(FLinearColor(0.02f, 0.02f, 0.02f, 0.85f));
    Backdrop->SetHorizontalAlignment(HAlign_Center);
    Backdrop->SetVerticalAlignment(VAlign_Center);
    WidgetTree->RootWidget = Backdrop;

    // Панель фиксированного размера, чтобы текст не растягивался на весь
    // экран и оставался читаемым столбцом, как обычный лог/консоль.
    USizeBox* Panel = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("JournalPanel"));
    Panel->SetWidthOverride(760.0f);
    Panel->SetHeightOverride(620.0f);
    Backdrop->SetContent(Panel);

    UBorder* PanelBackground = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("JournalPanelBackground"));
    PanelBackground->SetBrushColor(FLinearColor(0.06f, 0.06f, 0.05f, 0.97f));
    PanelBackground->SetPadding(FMargin(20.0f));
    Panel->SetContent(PanelBackground);

    UVerticalBox* Root = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("JournalRoot"));
    PanelBackground->SetContent(Root);

    UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("JournalTitle"));
    Title->SetText(FText::FromString(TEXT("Травник")));
    Title->SetFont(FSlateFontInfo(FCoreStyle::GetDefaultFont(), 24));
    Title->SetColorAndOpacity(FSlateColor(FLinearColor(0.92f, 0.85f, 0.6f)));
    UVerticalBoxSlot* TitleSlot = Root->AddChildToVerticalBox(Title);
    TitleSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 12.0f));

    UScrollBox* Scroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("JournalScroll"));
    UVerticalBoxSlot* ScrollSlot = Root->AddChildToVerticalBox(Scroll);
    ScrollSlot->SetSize(ESlateSizeRule::Fill);

    EntryList = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("JournalEntryList"));
    Scroll->AddChild(EntryList);

    UTextBlock* Hint = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("JournalHint"));
    Hint->SetText(FText::FromString(TEXT("J — закрыть")));
    Hint->SetColorAndOpacity(FSlateColor(FLinearColor(0.55f, 0.55f, 0.5f)));
    UVerticalBoxSlot* HintSlot = Root->AddChildToVerticalBox(Hint);
    HintSlot->SetPadding(FMargin(0.0f, 10.0f, 0.0f, 0.0f));
}

void UJournalLogWidget::OnJournalEntryAdded()
{
    RefreshDisplay();
}

void UJournalLogWidget::RefreshDisplay()
{
    if (!JournalComponent || !EntryList) return;

    EntryList->ClearChildren();

    UIngredientRegistrySubsystem* IngSub = nullptr;
    if (UGameInstance* GI = GetGameInstance())
    {
        IngSub = GI->GetSubsystem<UIngredientRegistrySubsystem>();
    }

    // Новые сверху -- тот же порядок, что у JournalWidget (RefreshDisplay).
    const TArray<FJournalEntry>& Entries = JournalComponent->GetEntries();
    if (Entries.Num() == 0)
    {
        UTextBlock* Empty = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
        Empty->SetText(FText::FromString(TEXT("Пока пусто -- собери или свари что-нибудь.")));
        Empty->SetColorAndOpacity(FSlateColor(FLinearColor(0.6f, 0.6f, 0.6f)));
        EntryList->AddChildToVerticalBox(Empty);
        return;
    }

    for (int32 i = Entries.Num() - 1; i >= 0; --i)
    {
        UTextBlock* Line = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
        Line->SetText(FormatEntry(Entries[i], IngSub));
        Line->SetAutoWrapText(true);
        UVerticalBoxSlot* LineSlot = EntryList->AddChildToVerticalBox(Line);
        LineSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 8.0f));
    }
}

// Тот же язык/подписи (Сила/Искажение/Чистота/Порча), что уже устоялся в
// JournalEntryRowWidget::InitializeRow -- один читаемый абзац вместо шести
// отдельных текстовых полей, для лога этого достаточно. PerceivedState --
// всегда искажённое (S_Perceived) состояние, замороженное в момент записи
// (см. предупреждение в JournalTypes.h), не переcчитывается здесь заново.
FText UJournalLogWidget::FormatEntry(const FJournalEntry& Entry, UIngredientRegistrySubsystem* IngredientRegistry) const
{
    FInventoryItem NameLookup;
    NameLookup.IngredientID = Entry.IngredientID;
    NameLookup.State = Entry.PerceivedState;
    const FString DisplayName = GetItemDisplayName(NameLookup, IngredientRegistry);
    const FString TypeLabel = Entry.Type == EJournalEntryType::Harvest ? TEXT("Собрано") : TEXT("Сварено");
    const FString BiomeLabel = FBiomeDefaults::BiomeTypeToName(Entry.Biome).ToString();
    const FRealState& S = Entry.PerceivedState;

    return FText::FromString(FString::Printf(
        TEXT("[%s] %s ×%d — %s (%d,%d), %s, t=%.0f\n     Сила %.2f · Искажение %.2f · Чистота %.2f · Порча %.2f"),
        *TypeLabel, *DisplayName, Entry.Count, *BiomeLabel, Entry.Cell.X, Entry.Cell.Y,
        Entry.bWasNight ? TEXT("ночь") : TEXT("день"), Entry.GameTimeSeconds,
        S.Magnitude, S.Meta.Distortion, S.Meta.Purity, S.Meta.Corruption));
}
