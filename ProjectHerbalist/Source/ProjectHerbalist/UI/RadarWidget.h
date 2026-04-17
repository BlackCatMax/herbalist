#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Core/Types/HerbalistCoreTypes.h"
#include "RadarWidget.generated.h"

UCLASS()
class PROJECTHERBALIST_API URadarWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    // Установить направление (4 оси)
    void SetDirection(const FDirection& InDirection);
    
    // Установить цвет на основе чистоты (0..1) – зелёный (чистый) -> красный (грязный)
    void SetColorByPurity(float Purity);
    
    // Установить цвет на основе искажения (0..1) – зелёный (низкое) -> красный (высокое)
    void SetColorByDistortion(float Distortion);

    // Принудительно перерисовать
    void RefreshRadar();

protected:
    virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

private:
    FDirection CurrentDirection;
    FLinearColor CurrentColor = FLinearColor::White;

    // Вспомогательная: получить точку на графике для заданной оси (индекс 0..3) и значения (0..1)
    FVector2D GetAxisPoint(int32 AxisIndex, float Value, const FGeometry& Geometry) const;
};