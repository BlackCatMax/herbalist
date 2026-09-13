// Source/ProjectHerbalist/Core/Types/HerbalistCellCoord.h
//
// Координата клетки «не задана» (2026-09-13, разметка мира, этап 6).
//
// Прежде это была (-1, -1). С глобальными координатами клеток от начала сетки
// World Partition (решение пользователя 13, DESIGN_World_Layout.md §4) у клеток
// к западу и югу от начала координаты отрицательные, и (-1, -1) -- настоящая
// клетка рядом с ним. Место, поставленное туда, считалось бы «не размещённым».

#pragma once

#include "CoreMinimal.h"

namespace HerbalistCore
{
    // -2^29 по обеим осям. Настоящие клетки до ±2^28 (268 000 км при клетке
    // 1 м) лежат далеко от этого значения, и разность координат с ним в
    // Чебышёвских расстояниях не переполняет int32.
    inline constexpr int32 InvalidCellCoord = -(1 << 29);

    // Функция, а не глобальная константа FIntPoint: конструктор FIntPoint не
    // constexpr, и глобал инициализировался бы динамически -- в порядке,
    // который C++ между единицами трансляции не гарантирует. Значения по
    // умолчанию полей клеток читаются в конструкторах, в том числе статических
    // объектов, и могли бы получить нули.
    inline FIntPoint InvalidCell()
    {
        return FIntPoint(InvalidCellCoord, InvalidCellCoord);
    }

    inline bool IsValidCell(const FIntPoint& Cell)
    {
        return Cell != InvalidCell();
    }

    // Координата для журнала и логов: у незаданной клетки «—», а не
    // (-536870912,-536870912) (найдено ревью этапа 6а).
    inline FString CellToDisplayString(const FIntPoint& Cell)
    {
        return IsValidCell(Cell) ? FString::Printf(TEXT("(%d,%d)"), Cell.X, Cell.Y) : FString(TEXT("—"));
    }

    // Деление с округлением вниз -- номер чанка, тайла или страницы клетки
    // (этапы 6-8): у отрицательных координат обычное деление тянет к нулю и
    // склеивает номер -1 с номером 0.
    inline constexpr int32 FloorDivCoord(int32 Value, int32 Divisor)
    {
        const int32 Quotient = Value / Divisor;
        return (Value % Divisor != 0 && (Value < 0) != (Divisor < 0)) ? Quotient - 1 : Quotient;
    }
}
