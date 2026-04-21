---
tags: [glossary, system]
status: ✅
---

# GridWorldManager

**GDD:** Управляет пространственной сеткой мира: хранит состояние ячеек, предоставляет доступ к ним по координатам и оповещает системы об изменениях.

**Tech:** `AGridWorldManager`. Размер сетки `GridSizeX × GridSizeY`. Каждая клетка — `FGridCell` с `Biome`, `State`, `TargetState`, `Environment`, `Memory`, `HarvestStress`. Интерполяция состояний в `Tick`. Распространение эффектов через BFS.

**Восприятие:** Не воспринимается напрямую. Обеспечивает плавные переходы между состояниями биомов.

**Связи:**
- [[BiomeState]]
- [[Harvest]]
- [[MemoryState]]