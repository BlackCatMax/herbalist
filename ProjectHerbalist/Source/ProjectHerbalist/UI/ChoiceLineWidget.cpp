// ChoiceLineWidget.cpp
#include "UI/ChoiceLineWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"

void UChoiceLineWidget::NativeConstruct()
{
    Super::NativeConstruct();
    if (!WidgetTree || PromptText) return;

    // Тот же приём, что у строки ощущения: во весь экран, прозрачно, без
    // ввода; реплика и ответы -- у нижнего края, как субтитр.
    UBorder* Root = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("ChoiceRoot"));
    Root->SetBrushColor(FLinearColor::Transparent);
    Root->SetHorizontalAlignment(HAlign_Center);
    Root->SetVerticalAlignment(VAlign_Bottom);
    Root->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 60.0f));
    Root->SetVisibility(ESlateVisibility::HitTestInvisible);
    WidgetTree->RootWidget = Root;

    UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("ChoiceColumn"));
    Root->SetContent(Column);

    PromptText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ChoicePrompt"));
    PromptText->SetFont(FSlateFontInfo(FCoreStyle::GetDefaultFont(), 20));
    PromptText->SetColorAndOpacity(FSlateColor(FLinearColor(0.92f, 0.88f, 0.76f)));
    PromptText->SetShadowOffset(FVector2D(1.0f, 1.0f));
    PromptText->SetShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.8f));
    PromptText->SetAutoWrapText(true);
    UVerticalBoxSlot* PromptSlot = Column->AddChildToVerticalBox(PromptText);
    PromptSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 10.0f));

    OptionBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("ChoiceOptions"));
    Column->AddChildToVerticalBox(OptionBox);

    Refresh();
}

void UChoiceLineWidget::SetContent(const FString& Prompt, const TArray<FString>& Options, int32 Selected)
{
    PendingPrompt = Prompt;
    PendingOptions = Options;
    PendingSelected = Selected;
    Refresh();
}

void UChoiceLineWidget::Refresh()
{
    if (!PromptText || !OptionBox || !WidgetTree) return;
    PromptText->SetText(FText::FromString(PendingPrompt));
    OptionBox->ClearChildren();
    for (int32 Index = 0; Index < PendingOptions.Num(); ++Index)
    {
        const bool bSelected = Index == PendingSelected;
        UTextBlock* Line = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
        Line->SetText(FText::FromString(FString::Printf(TEXT("%s %s"), bSelected ? TEXT("›") : TEXT(" "), *PendingOptions[Index])));
        Line->SetFont(FSlateFontInfo(FCoreStyle::GetDefaultFont(), 17));
        Line->SetColorAndOpacity(FSlateColor(bSelected ? FLinearColor(1.0f, 0.93f, 0.7f) : FLinearColor(0.62f, 0.6f, 0.55f)));
        Line->SetShadowOffset(FVector2D(1.0f, 1.0f));
        Line->SetShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.8f));
        OptionBox->AddChildToVerticalBox(Line);
    }
}
