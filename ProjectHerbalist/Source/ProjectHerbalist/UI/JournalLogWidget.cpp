// JournalLogWidget.cpp
#include "UI/JournalLogWidget.h"
#include "Core/Journal/HerbalistJournalComponent.h"
#include "Core/Journal/JournalTypes.h"
#include "Core/Types/HerbalistNameUtils.h"
#include "Core/Types/BiomeTypes.h"
#include "Core/Subsystems/IngredientRegistrySubsystem.h"
#include "Core/World/GridWorldManager.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/SizeBox.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "Components/ComboBoxString.h"
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

void UJournalLogWidget::BindWorldManager(AGridWorldManager* InWorldManager)
{
    WorldManagerRef = InWorldManager;
    if (ClarityText)
    {
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
    TitleSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 4.0f));

    // Ясность (AGridWorldManager::GlobalPerceptionClarity, "Прогрессия/Заряна"
    // 2026-08-29) -- единственное число прогрессии во всей игре (06_Progression.md
    // прямо запрещает числовые статы/уровни), поэтому стоит рядом с заголовком,
    // не внутри списка записей: это состояние Травника целиком, не одна запись.
    ClarityText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("JournalClarity"));
    ClarityText->SetFont(FSlateFontInfo(FCoreStyle::GetDefaultFont(), 14));
    ClarityText->SetColorAndOpacity(FSlateColor(FLinearColor(0.6f, 0.65f, 0.7f)));
    UVerticalBoxSlot* ClaritySlot = Root->AddChildToVerticalBox(ClarityText);
    ClaritySlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 8.0f));

    // Подсветка закономерностей (07_UX §7.2.4/Фаза C п.8) -- фильтр по
    // ингредиенту/"Зелья", единственный способ увидеть несколько записей
    // одного и того же рядом без прокрутки всего лога вручную. Опции
    // заполняются в RefreshDisplay (зависят от того, что реально уже
    // записано), не здесь -- при первом открытии Травника записей ещё нет.
    IngredientFilterCombo = WidgetTree->ConstructWidget<UComboBoxString>(UComboBoxString::StaticClass(), TEXT("JournalFilter"));
    IngredientFilterCombo->OnSelectionChanged.AddDynamic(this, &UJournalLogWidget::OnFilterSelectionChanged);
    UVerticalBoxSlot* FilterSlot = Root->AddChildToVerticalBox(IngredientFilterCombo);
    FilterSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 12.0f));
    FilterSlot->SetHorizontalAlignment(HAlign_Left);

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

void UJournalLogWidget::OnFilterSelectionChanged(FString SelectedItem, ESelectInfo::Type SelectionType)
{
    // Direct -- вызвано программным SetSelectedOption изнутри
    // RefreshFilterOptions (пересинхронизация списка при новой записи), не
    // самим игроком; SelectedIngredientFilter там уже выставлен напрямую,
    // повторный RefreshDisplay тут не нужен и рискует повторным входом
    // (RefreshDisplay -> RefreshFilterOptions -> SetSelectedOption -> ...).
    if (SelectionType == ESelectInfo::Direct) return;

    if (const FName* Found = FilterLabelToIngredientID.Find(SelectedItem))
    {
        SelectedIngredientFilter = *Found;
    }
    else
    {
        SelectedIngredientFilter = NAME_None;
    }
    RefreshDisplay();
}

void UJournalLogWidget::RefreshDisplay()
{
    if (!JournalComponent || !EntryList) return;

    if (ClarityText)
    {
        const bool bBuyan = WorldManagerRef && WorldManagerRef->IsBuyanReached();
        const float Clarity = WorldManagerRef ? WorldManagerRef->GetGlobalPerceptionClarity() : 0.0f;
        ClarityText->SetText(bBuyan
            ? FText::FromString(TEXT("Ясность: 1.00 -- Буян достигнут"))
            : FText::FromString(FString::Printf(TEXT("Ясность: %.2f"), Clarity)));
    }

    UIngredientRegistrySubsystem* IngSub = nullptr;
    if (UGameInstance* GI = GetGameInstance())
    {
        IngSub = GI->GetSubsystem<UIngredientRegistrySubsystem>();
    }

    RefreshFilterOptions(IngSub);

    EntryList->ClearChildren();

    // Новые сверху -- тот же порядок, что у JournalWidget (RefreshDisplay).
    // Фильтр (SelectedIngredientFilter) отбирает подмножество тех же самых
    // записей -- ничего не считает и не подсказывает, просто убирает с
    // глаз то, что сейчас не сравнивается (подсветка закономерностей,
    // 07_UX §7.2.4).
    const TArray<FJournalEntry>& AllEntries = JournalComponent->GetEntries();
    TArray<int32> VisibleIndices;
    VisibleIndices.Reserve(AllEntries.Num());
    for (int32 i = 0; i < AllEntries.Num(); ++i)
    {
        if (SelectedIngredientFilter != NAME_None
            && (AllEntries[i].Type == EJournalEntryType::MemoryFragment || AllEntries[i].IngredientID != SelectedIngredientFilter))
        {
            continue;
        }
        VisibleIndices.Add(i);
    }

    if (VisibleIndices.Num() == 0)
    {
        UTextBlock* Empty = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
        Empty->SetText(AllEntries.Num() == 0
            ? FText::FromString(TEXT("Пока пусто -- собери или свари что-нибудь."))
            : FText::FromString(TEXT("Нет записей с этим отбором.")));
        Empty->SetColorAndOpacity(FSlateColor(FLinearColor(0.6f, 0.6f, 0.6f)));
        EntryList->AddChildToVerticalBox(Empty);
        return;
    }

    for (int32 i = VisibleIndices.Num() - 1; i >= 0; --i)
    {
        UTextBlock* Line = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
        Line->SetText(FormatEntry(AllEntries[VisibleIndices[i]], IngSub));
        Line->SetAutoWrapText(true);
        UVerticalBoxSlot* LineSlot = EntryList->AddChildToVerticalBox(Line);
        LineSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 8.0f));
    }
}

// Список опций фильтра отражает то, что реально уже записано (порядок
// появления, не алфавит -- проще увидеть только что собранное). Восстанавливает
// текущий выбор, если он всё ещё существует, иначе откатывает на "Все" --
// иначе перерисовка на каждую новую запись (OnJournalEntryAdded) сбрасывала
// бы фильтр игрока при любом новом событии где угодно в мире.
void UJournalLogWidget::RefreshFilterOptions(UIngredientRegistrySubsystem* IngredientRegistry)
{
    if (!IngredientFilterCombo || !JournalComponent) return;

    const FString PreviousSelection = IngredientFilterCombo->GetSelectedOption();

    IngredientFilterCombo->ClearOptions();
    FilterLabelToIngredientID.Empty();

    const FString AllLabel = TEXT("Все");
    IngredientFilterCombo->AddOption(AllLabel);
    FilterLabelToIngredientID.Add(AllLabel, NAME_None);

    TSet<FName> SeenIDs;
    for (const FJournalEntry& Entry : JournalComponent->GetEntries())
    {
        if (Entry.Type == EJournalEntryType::MemoryFragment) continue;
        if (SeenIDs.Contains(Entry.IngredientID)) continue;
        SeenIDs.Add(Entry.IngredientID);

        FString Label;
        if (Entry.IngredientID == FName(TEXT("Potion")))
        {
            // "Potion" -- общий IngredientID у ЛЮБОГО сваренного зелья
            // (см. GridWorldManagerTick.cpp): имя каждой конкретной варки
            // генерируется из State (GeneratePotionName) и потому разное у
            // разных записей -- честная метка фильтра не может быть именем
            // одного зелья, только общая категория ("сравнение зелий",
            // Фаза C п.8).
            Label = TEXT("Зелья");
        }
        else
        {
            FInventoryItem NameLookup;
            NameLookup.IngredientID = Entry.IngredientID;
            Label = GetItemDisplayName(NameLookup, IngredientRegistry);
        }

        if (FilterLabelToIngredientID.Contains(Label)) continue;   // на случай совпавших меток
        IngredientFilterCombo->AddOption(Label);
        FilterLabelToIngredientID.Add(Label, Entry.IngredientID);
    }

    FString TargetSelection = AllLabel;
    if (!PreviousSelection.IsEmpty() && FilterLabelToIngredientID.Contains(PreviousSelection))
    {
        TargetSelection = PreviousSelection;
    }
    else
    {
        SelectedIngredientFilter = NAME_None;
    }

    if (IngredientFilterCombo->GetSelectedOption() != TargetSelection)
    {
        IngredientFilterCombo->SetSelectedOption(TargetSelection);
    }
}

// Тот же язык/подписи (Сила/Искажение/Чистота/Порча), что уже устоялся в
// JournalEntryRowWidget::InitializeRow -- один читаемый абзац вместо шести
// отдельных текстовых полей, для лога этого достаточно. PerceivedState --
// всегда искажённое (S_Perceived) состояние, замороженное в момент записи
// (см. предупреждение в JournalTypes.h), не переcчитывается здесь заново.
FText UJournalLogWidget::FormatEntry(const FJournalEntry& Entry, UIngredientRegistrySubsystem* IngredientRegistry) const
{
    // Фрагмент памяти -- отдельная ветка, не подходит под "ингредиент × N со
    // стат-блоком": сам текст воспоминания важнее, чем где/когда его нашли.
    if (Entry.Type == EJournalEntryType::MemoryFragment)
    {
        const FString TypeLabel = Entry.bFragmentWasTrue ? TEXT("Воспоминание") : TEXT("Искажённое воспоминание");
        return FText::FromString(FString::Printf(
            TEXT("[%s] \"%s\"\n     (%d,%d), %s"),
            *TypeLabel, *Entry.FragmentText.ToString(), Entry.Cell.X, Entry.Cell.Y,
            Entry.bWasNight ? TEXT("ночь") : TEXT("день")));
    }

    FInventoryItem NameLookup;
    NameLookup.IngredientID = Entry.IngredientID;
    NameLookup.State = Entry.PerceivedState;
    NameLookup.BrewOutcome = Entry.BrewOutcome;
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
