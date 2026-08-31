---
tags: [glossary, system]
status: ✅
---

# GridWorldManager

**GDD:** Управляет пространственной сеткой мира: хранит состояние ячеек, предоставляет доступ к ним по координатам и оповещает системы об изменениях.

**Tech:** `AGridWorldManager`. Размер сетки `GridSizeX × GridSizeY`. Каждая клетка — `FGridCell` с `Biome`, `State`, `TargetState`, `Environment`, `Memory`, `HarvestStress`. Реальный механизм иной, чем интерполяция состояний в `Tick` и распространение через BFS: фиксированный шаг симуляции `RunSimulationStep` (Command → Snapshot → Pipeline → Delta) плюс отдельная релаксация `RegenerateCellParameters(DeltaTime)` к `TargetState` каждый кадр; заражение соседей порчей — точечный нудж `TargetState` четырёх соседних клеток по сетке (`GridWorldManagerCore.cpp`, ~строка 785+), не BFS-функция с ограничением по глубине.

**Восприятие:** Не воспринимается напрямую. Обеспечивает плавные переходы между состояниями биомов.

**Связи:**
- [[BiomeState]]
- [[Harvest]]
- [[MemoryState]]