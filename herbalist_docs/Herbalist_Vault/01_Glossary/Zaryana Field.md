---
tags: [glossary, biomes, graph]
status: ✅
---

# Zaryana Field

**GDD:** Runtime-поле узла графа, отражающее текущую силу стабилизации [[Zaryana]] в биоме. Усиливает стабилизацию при алхимическом преобразовании. Противодействует искажениям, повышает предсказуемость и устойчивость результата.

**Tech:** Поле `ZaryanaField` в структуре `FBiomeGraphNode` (тип `float`). Обновляется аналогично `MorokField` — агрегацией из `FGridBiomeSample` и распространением через рёбра с коэффициентом `ZaryanaFlow`. Используется на этапе `Biome Context Injection` до применения [[Morok]] и [[Zaryana]].

**Восприятие:** Игрок ощущает стабильные, «чистые» зелья в биомах с высоким `ZaryanaField`. Такие места кажутся «благословенными» или «спокойными».

**Связи:**
- [[Zaryana]]
- [[Biome Graph]]
- [[Propagation]]
- [[GridBiomeSample]]