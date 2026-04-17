#include "UI/RadarWidget.h"
#include "Rendering/DrawElements.h"

void URadarWidget::SetDirection(const FDirection& InDirection)
{
    CurrentDirection = InDirection;
    RefreshRadar();
}

void URadarWidget::SetColorByPurity(float Purity)
{
    CurrentColor = FLinearColor::LerpUsingHSV(FLinearColor::Red, FLinearColor::Green, Purity);
    RefreshRadar();
}

void URadarWidget::SetColorByDistortion(float Distortion)
{
    CurrentColor = FLinearColor::LerpUsingHSV(FLinearColor::Green, FLinearColor::Red, Distortion);
    RefreshRadar();
}

void URadarWidget::RefreshRadar()
{
    InvalidateLayoutAndVolatility();
}

FVector2D URadarWidget::GetAxisPoint(int32 AxisIndex, float Value, const FGeometry& Geometry) const
{
    const float Angles[4] = { 0.0f, 90.0f, 180.0f, 270.0f };
    float AngleRad = FMath::DegreesToRadians(Angles[AxisIndex]);
    float Radius = Geometry.GetLocalSize().GetMin() * 0.4f * Value;
    FVector2D Center = Geometry.GetLocalSize() * 0.5f;
    return Center + FVector2D(FMath::Cos(AngleRad), FMath::Sin(AngleRad)) * Radius;
}

int32 URadarWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
    if (AllottedGeometry.GetLocalSize().GetMin() <= 0.0f)
        return LayerId;

    const float Values[4] = {
        CurrentDirection.Body,
        CurrentDirection.Mind,
        CurrentDirection.Spirit,
        CurrentDirection.Nature
    };
    FVector2D Points[4];
    for (int32 i = 0; i < 4; ++i)
    {
        Points[i] = GetAxisPoint(i, Values[i], AllottedGeometry);
    }

    // Рисуем контур (линии)
    TArray<FVector2D> LinePoints = { Points[0], Points[1], Points[2], Points[3], Points[0] };
    FSlateDrawElement::MakeLines(
        OutDrawElements,
        LayerId,
        AllottedGeometry.ToPaintGeometry(),
        LinePoints,
        ESlateDrawEffect::None,
        CurrentColor.ToFColor(true),
        true
    );

    return LayerId + 1;
}