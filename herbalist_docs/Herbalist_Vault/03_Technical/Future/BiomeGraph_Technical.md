---
tags: [technical, biomes, graph, final]
status: implemented
---

# Biome Graph — Техническая спецификация (v1)

## Обзор

Biome Graph — подсистема, реализующая мир как граф взаимосвязанных биомов с распространением влияний, памятью и следами игрока. Внедрена в `UBiomeGraphSubsystem`.

## Архитектура

```
UBiomeGraphSubsystem : public UWorldSubsystem
├── TMap<FName, FBiomeGraphNode> Nodes   // runtime-узлы
├── TArray<FBiomeGraphEdge> Edges        // рёбра
├── TMap<FName, TArray<int32>> AdjacencyList
├── TMap<FName, FVector> CachedBiomeCenters
└── float TimeAccumulator (для фиксированного шага)
```

## Ключевые структуры (см. `BiomeGraphTypes.h`)

- `FBiomeGraphNode` – авторские параметры + runtime-поля (`MorokField`, `ZaryanaField`) + `FBiomeMemory`.
- `FBiomeGraphEdge` – `FromBiome`, `ToBiome`, `MorokLeak`, `ZaryanaFlow`.
- `FBiomeGraphNodeEntry` – запись для DataAsset (связь `BiomeID` → `FBiomeGraphNode`).
- `FGridBiomeSample` – контракт для передачи данных из Grid в Graph.

## Инициализация

1. `UBiomeGraphAsset` загружается в `GameMode::BeginPlay`.
2. `InitializeFromAsset` преобразует `TArray<FBiomeGraphNodeEntry>` в `TMap<FName, FBiomeGraphNode>`.
3. Строится `AdjacencyList`.

## Tick-модель

- **Фиксированный шаг:** `FixedTimeStep` (по умолчанию 0.2 сек).
- Внутренний цикл `InternalStep`:
  1. `RecalculateFieldsFromGrid` – агрегация `FGridBiomeSample` от `AGridWorldManager`.
  2. `PropagateWaves` – delta-based распространение.
  3. `ApplyFieldsToGrid` – вызов `AGridWorldManager::ApplyBiomeInfluences`.
  4. `UpdateMemories` – decay памяти.

## Контракт с GridWorldManager

- `GetBiomeSamples()` – возвращает массив `FGridBiomeSample` для всех клеток.
- `ApplyBiomeInfluences(MorokFields, ZaryanaFields, GlobalScale)` – применяет поля к `TargetState` клеток.
- `GetBiomeCenters()` – возвращает средние позиции биомов (для визуализации).

## Footprint

Вызывается из `AGridWorldManager::ApplyAlchemyResult`:
```cpp
Graph->RecordFootprint(BiomeID, Delta.Meta.Distortion, 1-Delta.Meta.Distortion, AxisDelta, 1.0f);
```

Обновляет Memory узла.

## Консольные команды

- `Herbalist.Graph.Print` – состояние всех узлов.
- `Herbalist.Graph.Step` – принудительный шаг симуляции.
- `Herbalist.Graph.Reset` – сброс полей и памяти.
- `Herbalist.Graph.ToggleVis` – визуализация графа (Editor).
- `Herbalist.Debug.ToggleCellDistortion` – оверлей Distortion на клетках.

## Статус

✅ Реализовано и протестировано в PIE.
❌ Collapse/Rebirth – зарезервировано.
⚠️ Визуализация требует калибровки координат.
