// SensationLineWidget.cpp
#include "UI/SensationLineWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/TextBlock.h"

void USensationLineWidget::NativeConstruct()
{
    Super::NativeConstruct();
    if (!WidgetTree || LineText) return;

    // Во весь экран, но прозрачно и без ввода: строка у нижнего края, как
    // субтитр, мир за ней продолжает жить.
    UBorder* Root = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("SensationRoot"));
    Root->SetBrushColor(FLinearColor::Transparent);
    Root->SetHorizontalAlignment(HAlign_Center);
    Root->SetVerticalAlignment(VAlign_Bottom);
    Root->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 90.0f));
    Root->SetVisibility(ESlateVisibility::HitTestInvisible);
    WidgetTree->RootWidget = Root;

    LineText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("SensationLine"));
    LineText->SetFont(FSlateFontInfo(FCoreStyle::GetDefaultFont(), 20));
    LineText->SetColorAndOpacity(FSlateColor(FLinearColor(0.92f, 0.88f, 0.76f)));
    LineText->SetShadowOffset(FVector2D(1.0f, 1.0f));
    LineText->SetShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.8f));
    Root->SetContent(LineText);

    if (!PendingLine.IsEmpty())
    {
        LineText->SetText(FText::FromString(PendingLine));
    }
}

void USensationLineWidget::SetLine(const FString& Line)
{
    PendingLine = Line;
    if (LineText)
    {
        LineText->SetText(FText::FromString(Line));
    }
}
