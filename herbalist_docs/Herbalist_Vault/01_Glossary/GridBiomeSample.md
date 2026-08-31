---
tags: [glossary, biomes, technical]
status: ✅
---

# GridBiomeSample

**GDD:** Контрактная структура для передачи данных из клеточного мира (`GridWorldManager`) в биомный граф. Агрегированное состояние всех клеток, относящихся к одному биому, включая усреднённые поля `Morok`, `Zaryana`, а также параметры среды и памяти.

**Tech:** Структура `FGridBiomeSample` (определена в `BiomeGraphTypes.h`, строки 9-21). Содержит всего 3 поля: `BiomeID`, `MorokValue`, `ZaryanaValue`. Заполняется в `UBiomeGraphSubsystem::RecalculateFieldsFromGrid` вызовом `AGridWorldManager::GetBiomeSamples()`.

**Восприятие:** Не воспринимается напрямую. Обеспечивает связь между видимым клеточным миром и скрытой графовой симуляцией.

**Связи:**
- [[GridWorldManager]]
- [[Biome Graph]]
- [[Morok Field]]
- [[Zaryana Field]]