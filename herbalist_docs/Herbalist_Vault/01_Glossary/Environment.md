---
tags: [glossary, context]
status: 🟡 частично
---

# Environment

**GDD:** Параметры среды, не контролируемые игроком напрямую: токсичность, плодородие, влажность.

**Tech:** `FEnvironment` с полями: `Toxicity ∈ [0,1]`, `Fertility ∈ [0,1]`,
`Moisture ∈ [0,1]`. Проверено 2026-09-06: `Toxicity`/`Moisture` нигде не
читаются и не пишутся (см. [[Toxicity]]/[[Moisture]]); `Fertility` —
единственное живое поле из трёх, но однонаправленно (только пишется
`ApplyFertilizerToCell`, нигде не читается, см. [[Fertility]]). На
`Harvest`/`Alchemy` структура в целом сейчас не влияет.

**Восприятие:** «Поганое место», «живая земля», «сырость» — задумано, не
реализовано (кроме однонаправленной записи `Fertility`).

**Связи:**
- [[Toxicity]]
- [[Fertility]]
- [[Moisture]]
- [[Harvest]]