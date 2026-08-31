---
tags: [glossary, biomes, graph]
status: ✅
---

# Biome Memory

**GDD:** Накопленная история воздействий в узле графа. Включает `MorokHistory`, `ZaryanaHistory` (массивы последних следов), `Instability` (общая нестабильность), `AxisDrift` (дрейф осей Body, Mind, Spirit, Nature). Память затухает со временем (decay), но сохраняет долгосрочные последствия действий игрока.

**Tech:** Структура `FBiomeMemory` внутри `FBiomeGraphNode`. Обновляется при вызове `RecordFootprint` (добавление новых следов) и в `UpdateMemories` (применение decay: умножение на `MemoryDecayRate`). `AxisDrift` используется внутри `PipelineV2.cpp` (`ComputeApplyResult`, шаг Biome Context Injection) для смещения осей результата при алхимии — не через отдельно названную функцию вроде `ResolveContext` (такой в коде нет).

**Восприятие:** Игрок замечает, что биом, в котором часто проводились опасные эксперименты, становится «нервным», непредсказуемым. Место «помнит» историю.

**Связи:**
- [[Footprint]]
- [[Propagation]]
- [[Biome Graph]]
- [[AxisDrift]]