---
tags: [glossary, context, implemented]
status: ✅
---

# Environment (Среда)

**GDD:** Параметры, не контролируемые игроком напрямую: токсичность, плодородие, влажность.

**Tech:** `FEnvironment` с полями:
- `Toxicity ∈ [0,1]`
- `Fertility ∈ [0,1]`
- `Moisture ∈ [0,1]`

Используется в:
- [[Harvest]]
- [[Alchemy]] (через `Toxicity` влияет на `Distortion`)

**Восприятие:** «Поганое место», «живая земля», «сырость».