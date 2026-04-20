---
tags: [glossary, core, implemented]
status: ✅
---

# S_real

**GDD:** Реальное (объективное) состояние мира, недоступное игроку напрямую.

**Tech:** `FRealState` в коде:
- `Magnitude` (float)
- `Direction` ([[Body]], [[Mind]], [[Spirit]], [[Nature]])
- `Meta` ([[Distortion]], [[Stability]], [[Purity]], [[Potency]], [[Resonance]], [[Corruption]])

Нормализация [[Direction]] выполняется методом `NormalizeSum()` — деление на сумму компонент после обрезания отрицательных значений.

Используется в:
- [[Harvest]]
- [[Alchemy]]
- [[GridWorldManager]]