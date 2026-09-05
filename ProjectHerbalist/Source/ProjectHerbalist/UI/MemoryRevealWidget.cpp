// MemoryRevealWidget.cpp
#include "UI/MemoryRevealWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Engine/World.h"
#include "TimerManager.h"

void UMemoryRevealWidget::NativeConstruct()
{
    Super::NativeConstruct();
    BuildLayout();
}

// Дерево строится целиком тут -- см. комментарий у класса в
// MemoryRevealWidget.h. Верх экрана, не центр (в отличие от JournalLogWidget,
// который открывается игроком осознанно и по праву занимает весь фокус) --
// попап всплывает САМ, посреди игры, пока игрок ещё смотрит на мир, не на UI.
void UMemoryRevealWidget::BuildLayout()
{
    // Идемпотентно (аудит 2026-09-05): раньше дерево строилось только в
    // NativeConstruct, который UMG реально запускает лишь при
    // AddToViewport()/RebuildWidget() -- но Show() ниже читал BodyText ДО
    // вызова AddToViewport() и на первом же обращении сразу после
    // CreateWidget() тихо выходил, так и не вызвав AddToViewport(): дерево
    // не строилось никогда, виджет был сломан весь сеанс. Теперь Show()
    // зовёт BuildLayout() напрямую, а NativeConstruct может вызвать её
    // ещё раз следом (через AddToViewport) -- защита от двойной
    // пересборки (и утечки уже показанного BodyText в осиротевшее дерево).
    if (!WidgetTree || BodyText) return;

    UBorder* Backdrop = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("MemoryRevealBackdrop"));
    Backdrop->SetBrushColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.0f));
    Backdrop->SetHorizontalAlignment(HAlign_Center);
    Backdrop->SetVerticalAlignment(VAlign_Top);
    Backdrop->SetPadding(FMargin(0.0f, 60.0f, 0.0f, 0.0f));
    WidgetTree->RootWidget = Backdrop;

    USizeBox* Panel = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("MemoryRevealPanel"));
    Panel->SetMaxDesiredWidth(620.0f);
    Backdrop->SetContent(Panel);

    UBorder* PanelBackground = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("MemoryRevealBackground"));
    PanelBackground->SetBrushColor(FLinearColor(0.05f, 0.04f, 0.08f, 0.9f));
    PanelBackground->SetPadding(FMargin(26.0f, 18.0f));
    Panel->SetContent(PanelBackground);

    BodyText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("MemoryRevealText"));
    BodyText->SetFont(FSlateFontInfo(FCoreStyle::GetDefaultFont(), 19));
    BodyText->SetColorAndOpacity(FSlateColor(FLinearColor(0.86f, 0.8f, 0.95f)));
    BodyText->SetAutoWrapText(true);
    BodyText->SetJustification(ETextJustify::Center);
    PanelBackground->SetContent(BodyText);
}

FText UMemoryRevealWidget::GetBodyTextForTest() const
{
    return BodyText ? BodyText->GetText() : FText::GetEmpty();
}

void UMemoryRevealWidget::Show(const FText& Text, float DisplaySeconds)
{
    BuildLayout();
    if (!BodyText) return;
    BodyText->SetText(Text);
    if (!IsInViewport())
    {
        AddToViewport();
    }

    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(DismissTimer);
        World->GetTimerManager().SetTimer(DismissTimer, [this]()
        {
            RemoveFromParent();
        }, DisplaySeconds, false);
    }
}
