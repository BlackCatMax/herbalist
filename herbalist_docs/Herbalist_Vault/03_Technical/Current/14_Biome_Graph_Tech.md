---
tags: [technical, current, biomes, graph]
gdd: "[[14_Biome_Graph]]"
version: 2.0
based_on: ProjectHerbalist source, 2026-09-22
---
# 14. Биомный граф — техническая сторона

Пара к главе [[14_Biome_Graph|14. Биомный граф и экосистемная симуляция]],
номера разделов совпадают. Прежнее имя — BiomeGraph_Technical
(переименован 2026-09-22). Математика полей, диффузии и «дырявого ведра» —
`docs/reference/MATH_REFERENCE.md` §3, §6. Проверка —
`docs/verification/pie/04_World_State.md` (граф биомов).

## §14.1–§14.2 Узлы и рёбра

`UBiomeGraphSubsystem : UWorldSubsystem` (`Core/BiomeGraph/`):

```
UBiomeGraphSubsystem
├── TMap<FName, FBiomeGraphNode> Nodes     // авторские параметры + MorokField, ZaryanaField, Memory
├── TArray<FBiomeGraphEdge> Edges          // FromBiome, ToBiome, MorokLeak, ZaryanaFlow
├── TMap<FName, TArray<int32>> AdjacencyList
└── TMap<FName, FVector> CachedBiomeCenters
```

- Данные — `UBiomeGraphAsset` (`FBiomeGraphNodeEntry` на узел), загружается в
  `AProjectHerbalistGameModeBase::BeginPlay` → `InitializeFromAsset`
  (ребро к неизвестному узлу пропускается с ошибкой в логе). Экспорт и
  импорт ассета — `-run=BiomeGraphExport` / `-run=BiomeGraphImport`
  (`docs/reference/TOOLS_REFERENCE.md`).
- `MorokField`/`ZaryanaField` — знаковое отклонение [−1, 1] от природы
  биома; абсолютный уровень — `GetAmbientMorok(BiomeID)`.

## §14.3 Влияние на пайплайн

Снимок `FBiomeSnapshot` уходит в `Simulation::ExecutePipeline`;
`ComputeApplyResult` подмешивает поля узла и `Memory.AxisDrift` перед
Мороком ([[05_Systems_Tech]] §5 Biome Context Injection).

## §14.4 Распространение

Фиксированный шаг `FixedTimeStep` (0,2 с), внутри:

1. `RecalculateFieldsFromGrid` — суммы по биому из сводок чанков
   `AGridWorldManager::GetBiomeFieldSums()` (`FHerbalistBiomeFieldSum`,
   `Core/World/ChunkSummaryTypes.h`); живые чанки пересчитываются, спящие —
   из кэша.
2. `PropagateWaves` — консервативная диффузия (сумма по графу сохраняется).
3. `ApplyFieldsToGrid` → `AGridWorldManager::ApplyBiomeInfluences` — сдвиг
   `TargetState` клеток; клетки в бистабильном полюсе пропускаются.
4. `UpdateMemories` — затухание памяти.

## §14.5 Память биома

`FBiomeMemory` узла: `MorokHistory`, `ZaryanaHistory`, `Instability`,
`AxisDrift` — затухает в `UpdateMemories`.

## §14.6 След игрока

Пайплайн кладёт след в `FStateDelta::Footprints`; после записи дельты
`RunSimulationStep` (`GridWorldManagerTick.cpp`) зовёт
`UBiomeGraphSubsystem::RecordFootprint(BiomeID, MorokImpact, ZaryanaImpact,
AxisDelta, dt)` — вне детерминированного пайплайна, как и Травник.

## §14.7 Коллапс и возрождение

Не реализовано (`CollapseThreshold` есть в снимке, механизма нет).

## §14.9 Отладка

Консоль: `Herbalist.Graph.Print` (узлы), `Herbalist.Graph.Step`
(принудительный шаг), `Herbalist.Graph.Reset`, `Herbalist.Graph.ToggleVis`
(визуализация, требует калибровки координат),
`Herbalist.Debug.ToggleCellDistortion` (оверлей искажения клеток).
