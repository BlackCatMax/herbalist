// Core/Types/HerbalistCalendar.h
//
// Игровой календарь (2026-09-16, решение пользователя): год 365 суток,
// обычные месяцы без високосных лет, четыре метеорологических сезона по три
// месяца -- как Meteorological Seasons у Ultra Dynamic Sky, чтобы дата и сезон
// неба совпадали с симуляцией. Игроку по лору сезонов три: осень -- часть
// Лета (LoreSeason). Отсчёт -- от 1 марта: игра начинается с начала весны, как
// и до календаря. Чистые функции без состояния; часы в день года переводит
// AGridWorldManager.
#pragma once

#include "CoreMinimal.h"
#include "Core/Types/HerbalistCoreTypes.h"

namespace HerbalistCore::Calendar
{
    inline constexpr int32 DaysPerYear = 365;

    // Первый месяц года отсчёта и длины месяцев начиная с него.
    inline constexpr int32 FirstMonth = 3;
    inline constexpr int32 MonthLengthsFromMarch[12] = { 31, 30, 31, 30, 31, 31, 30, 31, 30, 31, 31, 28 };

    struct FCalendarDate
    {
        int32 Month = FirstMonth;   // 1..12
        int32 Day = 1;              // 1..31
    };

    inline FCalendarDate DateFromDayOfYear(int32 DayOfYear)
    {
        int32 Remaining = ((DayOfYear % DaysPerYear) + DaysPerYear) % DaysPerYear;
        for (int32 Index = 0; Index < 12; ++Index)
        {
            if (Remaining < MonthLengthsFromMarch[Index])
            {
                FCalendarDate Date;
                Date.Month = (FirstMonth - 1 + Index) % 12 + 1;
                Date.Day = Remaining + 1;
                return Date;
            }
            Remaining -= MonthLengthsFromMarch[Index];
        }
        return FCalendarDate();   // недостижимо: сумма длин месяцев -- DaysPerYear
    }

    // День года от 1 марта; Month 1..12, Day 1..длина месяца.
    inline int32 DayOfYearFromDate(int32 Month, int32 Day)
    {
        const int32 MonthIndex = (Month - FirstMonth + 12) % 12;
        int32 DayOfYear = 0;
        for (int32 Index = 0; Index < MonthIndex; ++Index)
        {
            DayOfYear += MonthLengthsFromMarch[Index];
        }
        return DayOfYear + Day - 1;
    }

    inline ESeason SeasonForMonth(int32 Month)
    {
        switch (Month)
        {
        case 3: case 4: case 5:   return ESeason::Spring;
        case 6: case 7: case 8:   return ESeason::Summer;
        case 9: case 10: case 11: return ESeason::Autumn;
        default:                  return ESeason::Winter;
        }
    }

    // Как сезон называет игроку лор: осень -- часть Лета.
    inline ESeason LoreSeason(ESeason Season)
    {
        return Season == ESeason::Autumn ? ESeason::Summer : Season;
    }

    inline int32 SeasonStartDayOfYear(ESeason Season)
    {
        switch (Season)
        {
        case ESeason::Spring: return DayOfYearFromDate(3, 1);
        case ESeason::Summer: return DayOfYearFromDate(6, 1);
        case ESeason::Autumn: return DayOfYearFromDate(9, 1);
        default:              return DayOfYearFromDate(12, 1);
        }
    }

    inline int32 SeasonLengthDays(ESeason Season)
    {
        switch (Season)
        {
        case ESeason::Spring: return SeasonStartDayOfYear(ESeason::Summer) - SeasonStartDayOfYear(ESeason::Spring);
        case ESeason::Summer: return SeasonStartDayOfYear(ESeason::Autumn) - SeasonStartDayOfYear(ESeason::Summer);
        case ESeason::Autumn: return SeasonStartDayOfYear(ESeason::Winter) - SeasonStartDayOfYear(ESeason::Autumn);
        default:              return DaysPerYear - SeasonStartDayOfYear(ESeason::Winter);
        }
    }

    // Купальская ночь -- ночь на 24 июня (старый стиль, решение пользователя):
    // ночная фаза суток 23 июня (ночь -- последняя фаза суток, §15.2).
    inline int32 KupalaEveDayOfYear()
    {
        return DayOfYearFromDate(6, 23);
    }
}
