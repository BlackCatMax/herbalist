---
tags: [glossary, biomes, graph]
status: ✅
---

# Footprint

**GDD:** След игрока, оставляемый в узле графа после каждого применения алхимии к клетке. Содержит информацию о вызванном искажении (`MorokImpact = Delta.Meta.Distortion`), стабилизации (`ZaryanaImpact = 1 - Delta.Meta.Distortion`) и изменении осей (`AxisDelta`). Обновляет память биома и влияет на будущие состояния.

**Tech:** Метод `UBiomeGraphSubsystem::RecordFootprint(BiomeID, MorokImpact, ZaryanaImpact, AxisDelta, Weight)`. Вызывается из `AGridWorldManager::ApplyAlchemyResult`. Обновляет `FBiomeMemory` узла: добавляет в `MorokHistory`, `ZaryanaHistory`, увеличивает `Instability`, накапливает `AxisDrift`.

**Восприятие:** Игрок не видит след напрямую, но со временем замечает, что повторные действия в одном биоме меняют его поведение — место «запоминает» воздействия.

**Связи:**
- [[Biome Memory]]
- [[Alchemy]]
- [[Delta]]
- [[Biome Graph]]