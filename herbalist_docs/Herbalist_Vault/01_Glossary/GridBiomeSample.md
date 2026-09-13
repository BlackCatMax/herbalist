---
tags: [glossary, biomes, technical]
status: ✅
---

# GridBiomeSample

**GDD:** Контракт передачи данных из клеточного мира (`GridWorldManager`) в биомный граф: по каждому биому — отклонения полей `Morok` и `Zaryana` от природы биома, усреднённые по его клеткам.

**Tech:** Удалено 2026-09-13 (разметка мира, этап 7). Раньше — структура `FGridBiomeSample` на клетку и `AGridWorldManager::GetBiomeSamples()`, обход всех клеток на каждом шаге графа. Теперь граф берёт `AGridWorldManager::GetBiomeFieldSums()` — суммы по биому (`FHerbalistBiomeFieldSum`: сумма Морока, сумма Заряны, сумма позиций, число клеток) из сводок чанков (`FHerbalistChunkSummary`, `Core/World/ChunkSummaryTypes.h`) — и делит на число клеток. Центры биомов — оттуда же.

**Восприятие:** Не воспринимается напрямую. Обеспечивает связь между видимым клеточным миром и скрытой графовой симуляцией.

**Связи:**
- [[GridWorldManager]]
- [[Biome Graph]]
- [[Morok Field]]
- [[Zaryana Field]]