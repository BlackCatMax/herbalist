---
tags: [technical, current, biomes]
gdd: "[[10_Biomes_Reference]]"
version: 1.0
based_on: ProjectHerbalist source, 2026-09-22
---
# 10. Биомы, ингредиенты и вода — техническая сторона

Пара к главе [[10_Biomes_Reference|10. Биомы, ингредиенты и вода]].

## Общая структура биома

- Карточки биомов — `04_Compendium/Биомы/` (источник правды), живая
  таблица — `DT_BiomeDefaults` (ряд `FBiomeRow`, `Core/Types/BiomeRow.h`; помощник `FBiomeDefaults`, `Core/Types/BiomeTypes.h`):
  состояние земли и воды по умолчанию, `StressRecoveryMultiplier`,
  `EntityActivityBase`. Выравнивание таблицы по карточкам —
  `-run=BiomeDefaultsSync` (числа — в общем заголовке коммандлета и
  теста, `docs/reference/TOOLS_REFERENCE.md`); скрипт —
  `tools/data_extraction/extract_biomes.py`.
- Узлы графа биомов — [[14_Biome_Graph_Tech]].
- Вода — `DT_WaterTypes` (`tools/data_extraction/extract_water.py`),
  [[08_Content_Tech]] §8.3.
- Ингредиенты — [[05_Systems_Tech]] §5 Система ресурсов.
- На карте — регионы биомов и воды ([[Level_Assembly]]).
