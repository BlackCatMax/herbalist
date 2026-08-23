// HerbalistCoreTypes.cpp
#include "HerbalistCoreTypes.h"
#include "Math/UnrealMathUtility.h"

// S0 (Алатырь) — эталон, а не средняя точка: по каждой оси, где у "хорошо" и
// "плохо" есть чёткая полярность, берётся ГРАНИЧНОЕ значение идеала (0 или 1),
// не середина диапазона. Distortion/Corruption -> 0 (минимум зла), Stability/
// Purity/Potency/Resonance/Magnitude -> 1 (максимум блага) — Алатырь как
// предельно согласованное, предельно полное состояние, а не усреднённое.
//
// Единственное исключение — Direction: у него нет полюса "хорошо/плохо", есть
// четыре РАВНОПРАВНЫЕ качества (Тело/Разум/Дух/Природа). Идеал здесь — не
// граница, а точное равновесие между ними.
//
// L1, не L2: раньше Direction нормализовался по L2 (сумма компонент = 2.0),
// тогда как весь остальной пайплайн живёт на L1 (FDirection::NormalizeSum(),
// сумма = 1.0) — рассогласование шкал, из-за которого S0 был непригоден как
// точка отсчёта (см. AUDIT_AND_REFACTORING_PLAN §2.3). NormalizeSum() ниже
// даёт ровно L1.
const FRealState FAlatyr::S0 = []()
    {
        FRealState S;
        S.Magnitude = 1.0f;

        S.Direction.Body = 1.0f;
        S.Direction.Mind = 1.0f;
        S.Direction.Spirit = 1.0f;
        S.Direction.Nature = 1.0f;
        S.Direction.NormalizeSum();   // -> 0.25 по каждой оси, сумма 1.0 (L1)

        S.Meta.Distortion = 0.0f;
        S.Meta.Stability  = 1.0f;
        S.Meta.Purity     = 1.0f;
        S.Meta.Potency    = 1.0f;
        S.Meta.Resonance  = 1.0f;
        S.Meta.Corruption = 0.0f;

        return S;
    }();
