// UI/HerbalistWidgetSizing.h
#pragma once

#include "CoreMinimal.h"

class UWidgetTree;

namespace HerbalistUI
{
    // Окна растут под текст (2026-09-14). В макетах WBP окна котла, сумки, их
    // слотов и подсказки размер задан SizeBox с WidthOverride/HeightOverride:
    // длинное имя зелья или строка статуса обрезались. Здесь заданный размер
    // становится минимальным -- окно не меньше задуманного, но раздвигается
    // под текст. Максимум меньше этого минимума снимается, не меньший остаётся
    // пределом роста. Ассеты не меняются: правка на экземпляре, из NativeConstruct.
    // Возвращает число изменённых SizeBox; повторный вызов ничего не меняет.
    PROJECTHERBALIST_API int32 LetSizeBoxesGrowWithContent(UWidgetTree* WidgetTree);
}
