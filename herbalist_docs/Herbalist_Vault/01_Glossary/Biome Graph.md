---
tags: [glossary, biomes, graph, core]
status: ✅
---

# Biome Graph

**GDD:** Ориентированный взвешенный граф, представляющий мир как сеть взаимосвязанных биомов. Узлы — биомы с параметрами (`MorokAffinity`, `ZaryanaAffinity`, `Stability`) и runtime-полями (`MorokField`, `ZaryanaField`, `Memory`). Рёбра — направленные связи с коэффициентами утечки (`MorokLeak`, `ZaryanaFlow`). Обеспечивает распространение влияний, память биомов и обратную связь с клеточным миром.

**Tech:** `UBiomeGraphSubsystem` (унаследован от `UWorldSubsystem`). Хранит `TMap<FName, FBiomeGraphNode> Nodes`, `TArray<FBiomeGraphEdge> Edges`, `TMap<FName, TArray<int32>> AdjacencyList`. Инициализируется из `UBiomeGraphAsset`. Работает с фиксированным временным шагом (`FixedTimeStep` = 0.2 сек). Взаимодействует с `AGridWorldManager` через контракт `FGridBiomeSample`.

**Восприятие:** Игрок не видит граф напрямую, но ощущает его через изменение биомов со временем — знакомые места постепенно меняют свойства, а влияние алхимии распространяется дальше одной клетки.

**Связи:**
- [[GridWorldManager]]
- [[Biome Memory]]
- [[Propagation]]
- [[Footprint]]