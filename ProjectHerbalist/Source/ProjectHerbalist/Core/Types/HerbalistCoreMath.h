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

    // Триггер Шмитта: порог входа выше порога выхода на 2×Margin, чтобы Value,
    // колеблющееся у самой границы Threshold, не переключало bCurrentlyActive
    // каждый тик. Раньше жил локально в GridWorldManagerEntities.cpp (проявление
    // сущностей) — вынесено сюда при появлении второго потребителя (бистабильная
    // релаксация клетки, GridWorldManagerCore.cpp), чтобы не заводить третью копию.
    inline bool PassesHysteresisThreshold(bool bCurrentlyActive, float Value, float Threshold, float Margin)
    {
        return bCurrentlyActive ? (Value > Threshold - Margin) : (Value > Threshold + Margin);
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
    bool AreStatesSimilar(const FRealState& A, const FRealState& B,
        float MagnitudeThreshold = 0.15f,
        float DistortionThreshold = 0.2f,
        float PurityThreshold = 0.2f,
        float StabilityThreshold = 0.2f,
        float DirectionThreshold = 0.15f);
}
