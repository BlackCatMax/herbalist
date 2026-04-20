---
tags: [glossary, mechanic, implemented]
status: ✅
---

# Harvest (Сбор)

**GDD:** Извлечение локальной конфигурации состояния из среды.

**Tech:** `FHerbalistHarvest::Harvest`:
- `Base =` из DataAsset
- `+ k_biome * (BiomeState - S₀)`
- `+ k_condition * ConditionModifier`
- Нормализация по сумме, клиппинг.

Увеличивает `HarvestStress` клетки.

**Связи:**
- Использует [[S0]]
- Влияет на [[GridWorldManager]]