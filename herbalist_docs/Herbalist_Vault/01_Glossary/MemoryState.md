---
tags: [glossary, context, implemented]
status: ⚠️ частично
---

# MemoryState (Память мира)

**GDD:** Накопленная история изменений клетки.

**Tech:** `FMemoryState` с полями:
- `AccumulatedDistortion` — используется в [[Alchemy]] и для цвета отладки.
- `StabilityMemory` — обновляется, но не влияет на механики.
- `HistoryPurity` — обновляется, но не используется.

Обновляется в `Tick` и при фиксации состояния.

**Связи:**
- Влияет на `Distortion` в алхимии
- Должен влиять на вероятность событий (не реализовано)