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
- На карте — регионы биомов и воды (ниже; порядок работы —
  [[Level_Assembly#Рецепт: новый биом на карте]]).

## Регион биома на карте

`ABiomeRegionVolume` (`Core/World/BiomeRegionVolume.h`), на карте —
Blueprint `BP_BiomeVolume` (в нём же PCG-компонент травы,
[[13_World_Pipeline_Tech#Трава PCG]]). Форма — замкнутый
сплайн, проверка «клетка внутри» идёт в плоскости XY по центру клетки.
Регион всегда загружен (World Partition его не выгружает, флаг заперт).

Как клетка получает биом: все регионы, накрывшие её центр, делят клетку
поровну (два — по 0.5, три — по 1/3) в `BiomeWeights`; доминирующий биом —
больший вес, при равенстве — меньший номер `EBiomeType`. Клетка вне всех
регионов не заселяется: ни ресурсов, ни хозяев мест. Состояние клетки при
старте — смесь норм биомов по долям (`DT_BiomeDefaults`).

| Параметр | По умолчанию | Что делает |
|---|---|---|
| **Biome** | | |
| `Biome` | MixedForest | тип: Tundra, Taiga, MixedForest, BroadleafForest, ForestSteppe, Steppe, Floodplain, Bog |
| `SplineResolution` | 64 | точек дискретизации сплайна (8–500); больше — точнее форма |
| **Biome\|Density** | | |
| `bSpawnResourcesFromGrid` | да | да — ресурсы сеет менеджер сетки; нет — регион отдан PCG-графу ([[13_World_Pipeline_Tech#Ресурсы, расставленные графом]]). Отрастание работает в обоих случаях |
| `MinResourcesPer100SquareMeters` / `Max…` | 1 / 3 | плотность ресурсов на 100 м² (не на клетку — размер клетки выводится из ландшафта) |
| `ResourceRegrowthTimeSeconds` | 420 | время отрастания собранного; на истощённой клетке дольше |
| **Biome\|Placement** — случайная трансформация каждого ресурса | | |
| `MinUniformScale` / `Max…` | 0.85 / 1.15 | масштаб одним числом на все оси |
| `MinYawDegrees` / `Max…` | 0 / 360 | поворот вокруг вертикали |
| `MinTiltDegrees` / `Max…` | −5 / 5 | наклон (Pitch и Roll независимо) |
| `MinPositionOffset` / `Max…` | 0 / 0 | доп. смещение по осям поверх разброса в клетке |
| `DensityFalloffStrength` | 0 | 0 — выкл; 1 — у границы плотность падает до нуля |
| `ScaleFalloffStrength` | 0 | то же для масштаба |

«Граница» для затухания — приближение: центроид вершин сплайна и самая
дальняя вершина; для сильно вытянутых регионов градиент у краёв неточен.
Коммандлетная сборка карты ставит точки сплайна `SetSplinePointsWorld`.

Регион передвинули — клетки пересчитываются при следующем старте игры;
данные норм биома правят карточки → `-run=BiomeDefaultsSync`, не регион.

## Регион воды на карте

`AWaterRegionVolume` (`Core/World/WaterRegionVolume.h`, Blueprint
`BP_WaterVolume`) — наследник региона биома ради той же геометрии. Клетка
внутри формы — вода безусловно (`bIsWater`); тип воды для сбора — от
доминирующего земляного биома клетки, состояние — смесь воды биомов по
долям. Поэтому под регионом воды **должен лежать** земляной регион.
Унаследованные `Biome`, плотность и отрастание здесь не действуют —
водную клетку засевает пул водных растений, плотность и отрастание — от
земляного региона. В `BP_WaterVolume` — PCG-компонент слотов ресурсов
([[13_World_Pipeline_Tech#Слоты ресурсов у воды]]).
