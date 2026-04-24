---
tags:
  - glossary
  - context
status: ✅
---

# MemoryState

**GDD:** Накопленная история изменений клетки. Мир «помнит» действия игрока.

**Tech:** FMemoryState`: `AccumulatedDistortion`, `StabilityMemory`, `HistoryPurity`, `DistortionVelocity (Фаза 2), TimeOfLastDistortionChange (Фаза 2). Обновляется через ApplyDistortionDelta с saturation curve.

**Восприятие:** Не воспринимается напрямую. Проявляется через изменение поведения знакомых мест со временем.

**Связи:**
- [[GridWorldManager]]
- [[Alchemy]]