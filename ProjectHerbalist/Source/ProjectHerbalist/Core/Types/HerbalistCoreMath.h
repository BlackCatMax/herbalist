// HerbalistCoreMath.h
#pragma once

#include "CoreMinimal.h"
#include "Core/Types/HerbalistCoreTypes.h"

namespace HerbalistCore::Math
{
    inline float Clamp01(float v)
    {
        return FMath::Clamp(v, 0.0f, 1.0f);
    }

    inline float Clamp(float v, float Min, float Max)
    {
        return FMath::Clamp(v, Min, Max);
    }

    inline float Dot(const FDirection& A, const FDirection& B)
    {
        return A.Body * B.Body +
            A.Mind * B.Mind +
            A.Spirit * B.Spirit +
            A.Nature * B.Nature;
    }

    inline float Distance(const FDirection& A, const FDirection& B)
    {
        float dx = A.Body - B.Body;
        float dy = A.Mind - B.Mind;
        float dz = A.Spirit - B.Spirit;
        float dw = A.Nature - B.Nature;
        return FMath::Sqrt(dx * dx + dy * dy + dz * dz + dw * dw);
    }

    // Полное евклидово расстояние по всем 11 скалярным осям состояния —
    // Direction (4) + Magnitude + шесть мета-осей. Не нормализовано в [0,1]
    // (максимум ~sqrt(11) при полностью противоположных состояниях по всем
    // осям сразу) — конкретная нормировка появится вместе с первым реальным
    // потребителем (Distance(S_real, S0) как метрика прогрессии/капищ,
   // 00_Core_Lock §1.1, ROADMAP §2.3), а не придумывается заранее без него.
    inline float Distance(const FRealState& A, const FRealState& B)
    {
        const float DirSq = FMath::Square(Distance(A.Direction, B.Direction));
        const float dMag  = A.Magnitude       - B.Magnitude;
        const float dDist = A.Meta.Distortion - B.Meta.Distortion;
        const float dStab = A.Meta.Stability  - B.Meta.Stability;
        const float dPur  = A.Meta.Purity     - B.Meta.Purity;
        const float dPot  = A.Meta.Potency    - B.Meta.Potency;
        const float dRes  = A.Meta.Resonance  - B.Meta.Resonance;
        const float dCor  = A.Meta.Corruption - B.Meta.Corruption;
        return FMath::Sqrt(DirSq + dMag*dMag + dDist*dDist + dStab*dStab
                          + dPur*dPur + dPot*dPot + dRes*dRes + dCor*dCor);
    }

    // Distance_итог — метрика мира с историей, не только снимок
    // (15_Cycles_And_Shrines.md §15.5.1, реализовано 2026-09-06, прямой
    // запрос пользователя). 11_Intent_Evolution §11.5: "Intent становится
    // инструментом управления расстоянием до Алатыря" — метрика обязана
    // учитывать не только текущие числа клетки, но и то, КАК она к ним
    // пришла (та же путь-зависимость, что уже требует 09_Entities §9.3,
    // см. META_AUDIT.md §5). Два места с одинаковым State могут быть на
    // разном Distance_итог от S0 — если оно достигнуто согласованными
    // варками (AverageCoherence=1), штрафа нет; если тем же хаосом
    // (AverageCoherence=0), расстояние удваивается. Множитель ограничен
    // [1,2] явным клампом — AverageCoherence сама по себе не гарантированно
    // лежит строго в [0,1] (EffectiveIntent.Coherence, из которого она
    // копится, клампится по месту вычисления, а не здесь).
    inline float DistanceWithHistory(const FRealState& State, float AverageCoherence)
    {
        const float CoherencePenalty = FMath::Clamp(2.0f - AverageCoherence, 1.0f, 2.0f);
        return Distance(State, FAlatyr::S0) * CoherencePenalty;
    }

    // Триггер Шмитта: порог входа выше порога выхода на 2×Margin, чтобы Value,
    // колеблющееся у самой границы Threshold, не переключало bCurrentlyActive
    // каждый тик. Раньше жил локально в GridWorldManagerEntities.cpp (проявление
    // сущностей) — вынесено сюда при появлении второго потребителя (бистабильная
    // релаксация клетки, GridWorldManagerCore.cpp), чтобы не заводить третью копию.
    inline bool PassesHysteresisThreshold(bool bCurrentlyActive, float Value, float Threshold, float Margin)
    {
        return bCurrentlyActive ? (Value > Threshold - Margin) : (Value > Threshold + Margin);
    }

    // "Выдержано N секунд" (2026-09-02, разведка нашла три места, каждое
    // приближённое до мгновенного порога с явным комментарием "в проекте
    // нет механизма длительности" — TISHINA_LESA/OJIDANIE_BURI/KHLEB_SOL,
    // MemoryFragmentDefinitions.h/17_Hero_And_Community §17.6-17.7:
    // "устойчиво"/"длительная"/"удержанной долго") — временной аналог
    // PassesHysteresisThreshold выше, тот же принцип "вынесено сюда при
    // появлении второго потребителя", здесь сразу три. Не Tick()-приводимый
    // (DeltaTime каждый кадр) — вызывающая сторона опрашивает периодически
    // (TrySpawnStateBasedFragment, раз в MemoryFragmentStateCheckInterval),
    // поэтому шаг — интервал опроса, не DeltaTime кадра. Условие держится
    // непрерывно -> накапливает; однажды сорвалось -> сбрасывается в ноль
    // целиком (не частично) — то же "всё или ничего", что и подразумевает
    // слово "устойчиво"/"удержанной": один провал должен заставить ждать
    // заново, не просто затормозить накопление.
    inline bool TickSustainedCondition(float& AccumulatedSeconds, bool bConditionHolds, float PollDeltaSeconds, float RequiredSeconds)
    {
        if (!bConditionHolds)
        {
            AccumulatedSeconds = 0.0f;
            return false;
        }
        AccumulatedSeconds += PollDeltaSeconds;
        return AccumulatedSeconds >= RequiredSeconds;
    }

    // Передозировка (обсуждение в сессии 2026-08-24, компендиум: "сок его —
    // сильное сердечное зелье, но и яд лютый" — Ландыш, тот же паттерн у
    // Полярного мака и Чистотела: сила лекарства и его яд — одна ось, не
    // отдельное "злое" растение против "доброго"). Чрезмерная Potency при
    // применении зелья не лечит линейно до бесконечности — после порога
    // начинает вредить месту вместо того, чтобы его исцелить, тем же
    // "испорченным" профилем осей, что и бистабильная деградация клетки
    // (GridWorldManagerCore.cpp) — единый язык для "вреда" по всему проекту.
    // Мутирует State на месте; вызывающий код решает, к чему её применяют.
    inline void ApplyOverdosePenalty(FRealState& State, float Threshold, float Penalty)
    {
        if (State.Meta.Potency <= Threshold) return;
        const float Overdose = (State.Meta.Potency - Threshold) / FMath::Max(1.0f - Threshold, KINDA_SMALL_NUMBER);
        const float P = Overdose * Penalty;
        State.Meta.Stability  = Clamp01(State.Meta.Stability  - P);
        State.Meta.Purity     = Clamp01(State.Meta.Purity     - P);
        State.Meta.Distortion = Clamp01(State.Meta.Distortion + P);
        State.Meta.Corruption = Clamp01(State.Meta.Corruption + P);
    }

    // Сравнение двух состояний с заданными допусками
    PROJECTHERBALIST_API bool AreStatesSimilar(const FRealState& A, const FRealState& B,
        float MagnitudeThreshold = 0.15f,
        float DistortionThreshold = 0.2f,
        float PurityThreshold = 0.2f,
        float StabilityThreshold = 0.2f,
        float DirectionThreshold = 0.15f);

    // Взвешенное смешение двух FRealState при слиянии стопок (вынесено из
    // UHerbalistInventoryComponent::MergeStack, аудит 2026-09-05: у котла
    // UAlchemySlotWidget::AddItem раньше молча отбрасывал State любой второй
    // и последующей единицы травы, добавленной в уже занятый слот — Pipeline
    // потом варил Count копий ПЕРВОЙ единицы, а не честную смесь. Одна и та
    // же формула для обоих мест, не две разные копии одной идеи — включает
    // не только линейную интерполяцию по весам количества, но и намеренный
    // "разброс от смешивания" (расхождение Distortion/Purity исходных
    // состояний не гасится усреднением целиком, а частично остаётся —
    // смешивать разнородные травы должно быть немного грязнее, чем усреднять
    // идентичные).
    PROJECTHERBALIST_API void BlendRealStatesForStack(FRealState& Target, const FRealState& Source, int32 TargetCount, int32 AddedCount);
}
