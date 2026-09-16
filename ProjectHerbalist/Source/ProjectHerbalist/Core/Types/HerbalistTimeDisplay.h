// Core/Types/HerbalistTimeDisplay.h
//
// Время для материалов (2026-09-16, этап 1б docs/research/
// DESIGN_Living_Vegetation_Research.md §2): часы симуляции переводятся в
// готовые плавные веса, чтобы ни один материал не считал календарь сам и
// формулы не расходились между C++ и шейдерами. Чистые функции без
// состояния; часы в аргументы переводит AGridWorldManager, в
// MPC_WorldStateFields пишет AGridWorldManager::WriteTimeDisplayParameters.
//
// Сглаживание во времени (лаг) здесь не нужно: все величины -- непрерывные
// функции часов, и перемотка часов должна показываться сразу, без догона.
#pragma once

#include "CoreMinimal.h"
#include "Core/Types/HerbalistCoreTypes.h"

namespace HerbalistCore::TimeDisplay
{
    // Индекс сезона в шкале Ultra Dynamic Sky: 0 весна, 1 лето, 2 осень,
    // 3 зима. Порядок ESeason другой (Autumn добавлен последним), поэтому
    // перевод явный.
    inline int32 UDSSeasonIndex(ESeason Season)
    {
        switch (Season)
        {
        case ESeason::Spring: return 0;
        case ESeason::Summer: return 1;
        case ESeason::Autumn: return 2;
        default:              return 3;
        }
    }

    // Сезон числом 0..4 как у UDS (§2.1): целое -- середина сезона, дробное --
    // переход к соседнему. На границе двух сезонов обе стороны дают одно и
    // то же значение (x.5), поэтому кривая непрерывна, хотя сезоны разной
    // длины (92/92/91/90 суток).
    inline float SeasonUDW(ESeason Season, float SeasonProgress01)
    {
        float Value = static_cast<float>(UDSSeasonIndex(Season)) + FMath::Clamp(SeasonProgress01, 0.0f, 1.0f) - 0.5f;
        if (Value < 0.0f) Value += 4.0f;
        if (Value >= 4.0f) Value -= 4.0f;
        return Value;
    }

    // Веса сезонов R весна, G лето, B осень, A зима; сумма 1. Линейная смесь
    // двух соседних, как дробный сезон у UDS: чистый сезон -- только в его
    // середине, на границе -- поровну.
    inline FLinearColor SeasonWeights(float InSeasonUDW)
    {
        const float Wrapped = FMath::Fmod(FMath::Fmod(InSeasonUDW, 4.0f) + 4.0f, 4.0f);
        const int32 Index = FMath::Clamp(FMath::FloorToInt(Wrapped), 0, 3);
        const float Blend = FMath::Clamp(Wrapped - static_cast<float>(Index), 0.0f, 1.0f);

        float Weights[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
        Weights[Index] += 1.0f - Blend;
        Weights[(Index + 1) % 4] += Blend;
        return FLinearColor(Weights[0], Weights[1], Weights[2], Weights[3]);
    }

    // Доля опавшей листвы по шкале SeasonUDW (§2 плана: «0 летом, растёт
    // осенью, 1 зимой, падает весной»): 0 с середины весны до конца лета
    // (0..1.5), smoothstep до 1 от начала осени до середины зимы (1.5..3),
    // линейно обратно к 0 до середины весны (3..4). Смесь весов сезонов тут не
    // годится: она роняла бы четверть листвы уже 31 августа.
    inline float LeafDrop01(float InSeasonUDW)
    {
        const float Wrapped = FMath::Fmod(FMath::Fmod(InSeasonUDW, 4.0f) + 4.0f, 4.0f);
        if (Wrapped < 1.5f)
        {
            return 0.0f;
        }
        if (Wrapped < 3.0f)
        {
            return FMath::SmoothStep(1.5f, 3.0f, Wrapped);
        }
        return 4.0f - Wrapped;
    }

    // Близость к полнолунию: 1 в середине фазы FullMoon (третья из четырёх,
    // доля цикла 0.625), 0 в середине Новолуния, плавный косинус между ними.
    inline float MoonFull01(float MoonCycle01)
    {
        return 0.5f + 0.5f * FMath::Cos(2.0f * UE_PI * (MoonCycle01 - 0.625f));
    }

    // Веса фаз суток R рассвет, G день, B закат, A ночь; сумма 1. Границы --
    // таблица §15.2 в абсолютных минутах, те же, что IsDawn/IsDusk/IsNight:
    // рассвет первые 6 минут, закат и ночь -- последние 12 и 6. Переход
    // между соседними фазами -- smoothstep шириной BlendMinutes по центру
    // границы; вне перехода фаза чистая. Середина перехода совпадает с
    // моментом, когда IsNight и прочие меняют значение. Совпадение границ с
    // IsDusk/IsNight -- при сутках от 18 минут (сейчас 32): короче фазы в
    // GridWorldManagerEntities.cpp перекрываются, здесь -- обрезаются.
    inline FLinearColor DayPhaseWeights(float TimeOfDay01, float DayMinutes, float BlendMinutes)
    {
        const float Day = FMath::Max(DayMinutes, 1.0f);
        const float Minute = FMath::Clamp(TimeOfDay01, 0.0f, 1.0f) * Day;

        const float DawnEnd = FMath::Min(6.0f, Day);
        const float NightStart = FMath::Max(Day - 6.0f, DawnEnd);
        const float DuskStart = FMath::Max(Day - 12.0f, DawnEnd);

        // Граница, фаза до неё, фаза после неё.
        struct FBoundary { float Minute; int32 Before; int32 After; };
        const FBoundary Boundaries[4] = {
            { 0.0f,       3, 0 },
            { DawnEnd,    0, 1 },
            { DuskStart,  1, 2 },
            { NightStart, 2, 3 },
        };

        // Полуширина не больше половины самой короткой фазы -- переходы не
        // перекрываются, и сумма весов остаётся ровно 1.
        const float ShortestPhase = FMath::Max(FMath::Min3(DawnEnd, DuskStart - DawnEnd, Day - NightStart), 0.0f);
        const float Half = FMath::Clamp(BlendMinutes * 0.5f, 0.0f, ShortestPhase * 0.5f);

        float Weights[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
        if (Half > KINDA_SMALL_NUMBER)
        {
            for (const FBoundary& Boundary : Boundaries)
            {
                float Offset = Minute - Boundary.Minute;
                if (Offset >= Day * 0.5f) Offset -= Day;
                if (Offset < -Day * 0.5f) Offset += Day;
                if (FMath::Abs(Offset) < Half)
                {
                    const float Blend = FMath::SmoothStep(-Half, Half, Offset);
                    Weights[Boundary.Before] = 1.0f - Blend;
                    Weights[Boundary.After] = Blend;
                    return FLinearColor(Weights[0], Weights[1], Weights[2], Weights[3]);
                }
            }
        }

        const int32 Phase = Minute < DawnEnd ? 0 : Minute < DuskStart ? 1 : Minute < NightStart ? 2 : 3;
        Weights[Phase] = 1.0f;
        return FLinearColor(Weights[0], Weights[1], Weights[2], Weights[3]);
    }
}
