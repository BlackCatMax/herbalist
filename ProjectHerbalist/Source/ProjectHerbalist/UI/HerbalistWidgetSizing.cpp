// UI/HerbalistWidgetSizing.cpp
#include "UI/HerbalistWidgetSizing.h"
#include "Blueprint/WidgetTree.h"
#include "Components/SizeBox.h"

int32 HerbalistUI::LetSizeBoxesGrowWithContent(UWidgetTree* WidgetTree)
{
    if (!WidgetTree) return 0;

    int32 ChangedCount = 0;
    WidgetTree->ForEachWidget([&ChangedCount](UWidget* Widget)
    {
        USizeBox* SizeBox = Cast<USizeBox>(Widget);
        if (!SizeBox) return;

        bool bChanged = false;
        // Уже заданный в макете минимум больше фиксированного размера --
        // оставляем больший из двух. Максимум, пока размер был фиксирован, на
        // раскладку не действовал (SBox применяет Min и Max только без
        // фиксированного размера): меньший нового минимума сжал бы окно ниже
        // макета и не дал расти -- снимаем; не меньший -- осознанный предел
        // роста, остаётся.
        if (SizeBox->IsWidthOverride())
        {
            const float MinWidth = SizeBox->IsMinDesiredWidthOverride()
                ? FMath::Max(SizeBox->GetMinDesiredWidth(), SizeBox->GetWidthOverride())
                : SizeBox->GetWidthOverride();
            SizeBox->ClearWidthOverride();
            SizeBox->SetMinDesiredWidth(MinWidth);
            if (SizeBox->IsMaxDesiredWidthOverride() && SizeBox->GetMaxDesiredWidth() < MinWidth)
            {
                SizeBox->ClearMaxDesiredWidth();
            }
            bChanged = true;
        }
        if (SizeBox->IsHeightOverride())
        {
            const float MinHeight = SizeBox->IsMinDesiredHeightOverride()
                ? FMath::Max(SizeBox->GetMinDesiredHeight(), SizeBox->GetHeightOverride())
                : SizeBox->GetHeightOverride();
            SizeBox->ClearHeightOverride();
            SizeBox->SetMinDesiredHeight(MinHeight);
            if (SizeBox->IsMaxDesiredHeightOverride() && SizeBox->GetMaxDesiredHeight() < MinHeight)
            {
                SizeBox->ClearMaxDesiredHeight();
            }
            bChanged = true;
        }
        if (bChanged)
        {
            ++ChangedCount;
        }
    });
    return ChangedCount;
}
