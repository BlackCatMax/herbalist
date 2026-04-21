---
tags: [technical, current, core]
version: 1.0
based_on: prototype_2026-04
---
# Core — реализованная механика
## Структуры данных

### `FRealState`

struct FRealState {
    float Magnitude;
    FDirection Direction;
    FMeta Meta;
};

### `FDirection`

struct FDirection {
    float Body, Mind, Spirit, Nature;
    void NormalizeSum(); // Body+Mind+Spirit+Nature → 1
};

### `FMeta`

struct FMeta {
    float Distortion, Stability, Purity, Potency, Resonance, Corruption;
};

### `FEnvironment`

struct FEnvironment {
    float Toxicity, Fertility, Moisture;
};

### `FMemoryState`

struct FMemoryState {
    float AccumulatedDistortion, StabilityMemory, HistoryPurity;
};

### `FIntent`

struct FIntent {
    float Coherence; // всегда 0.5f в текущем коде
};

## Нормализация направления

Используется `NormalizeSum()`:

1. Обнуление отрицательных значений.
    
2. Деление каждого компонента на сумму четырёх.
    
3. Если сумма ≈ 0 → все компоненты = 0.25.
    

**Отличие от `technical_full.md`:** в спецификации предполагалась L2-нормализация, но реализована сумма.

## Рандомизация

`FRngState` содержит `int32 Seed`. Генератор — линейный конгруэнтный (`Seed = Seed * 196314165 + 907633515`).

## Эталонное состояние `S₀` ([[S0|Алатырь]])

static const FRealState S0 = []{
    FRealState S;
    S.Magnitude = 0.5f;
    S.Direction = {0.5f, 0.5f, 0.5f, 0.5f};
    S.Meta.Distortion = 0.0f;
    S.Meta.Stability = 1.0f;
    S.Meta.Purity = 1.0f;
    // остальные Meta = 0.0f
    return S;
}();

Используется в `Harvest` как база для расчёта отклонений.