---
tags: [glossary, mechanic]
status: ✅
---

# Harvest

**GDD:** Сбор — извлечение локальной конфигурации состояния из среды. Ресурс формируется в момент взаимодействия, а не существует заранее.

**Tech:** `FHerbalistHarvest::Harvest` в коде не существует. Реальная точка входа — `GenerateHarvestResult` в `PipelineV2.cpp`: `Base` из DataAsset + `k_biome * (BiomeState - S₀)` + `k_condition * ConditionModifier`. Нормализация по сумме, клиппинг. Увеличивает `HarvestStress` клетки.

**Восприятие:** Одинаковые ресурсы в разных местах и условиях дают разный результат. Игрок учится выбирать «правильные» места и время.

**Связи:**
- [[S0]]
- [[GridWorldManager]]
- [[Environment]]