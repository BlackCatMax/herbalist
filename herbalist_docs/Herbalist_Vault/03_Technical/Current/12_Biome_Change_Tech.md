---
tags: [technical, current, biome-change]
gdd: "[[12_Biome_Change]]"
version: 1.0
based_on: ProjectHerbalist source, 2026-09-22
---
# 12. Изменение биомов — техническая сторона

Пара к главе [[12_Biome_Change|12. Изменение биомов и эволюция мира]],
номера разделов совпадают. Формулы и темпы — `docs/reference/MATH_REFERENCE.md`.

## §12.2–§12.4 Источники и инерция

Клетка (`FGridCell`) хранит состояние, цель (`TargetState`), память
(`FMemoryState`) и стресс сбора (`HarvestStress`). Изменения приходят
дельтой пайплайна ([[13_World_Pipeline_Tech]] §13.5–§13.8) и полями графа
(`ApplyBiomeInfluences`, [[14_Biome_Graph_Tech]] §14.4).

## §12.10 Стабилизация

`RegenerateCellParameters` (`GridWorldManagerCore.cpp`): релаксация к цели,
зарастание после сбора (`GetStressDecay`, сезонный и биомный множители),
бистабильность порчи (`FMemoryState::bDegrading` — испорченный полюс не
лечится пассивно). Инвариант: пассивная симуляция не опускает биом ниже
его собственной нормы.

## §12.14 Влияние через граф

[[14_Biome_Graph_Tech]].

Проверка — `docs/verification/pie/04_World_State.md` (снимки порчи),
долгий прогон — `docs/verification/ENGINE_VERIFICATION_GUIDE.md`.
