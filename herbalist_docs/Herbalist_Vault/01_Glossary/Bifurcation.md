---
tags: [glossary, mechanic]
status: ✅
---

# Bifurcation

**GDD:** Резкое изменение состояния зелья при критическом искажении — либо коллапс в хаос, либо очищение.

**Tech:** Логика живёт в `PipelineV2.cpp`, в `ComputeApplyResult` (~строки 493, 729-780), не в `GridWorldManager::ApplyAlchemyResult` (такой функции с этой логикой нет). Механика выросла за пределы одного порога: несколько уровней опасности, зависящих от количества ингредиентов (`GuaranteedCatastropheCount`), исключение для ритуальной варки (`bIsRitual`), и отдельный исход `Purified` (не только Catastrophe/ничего). **Collapse:** `Distortion = 0.2`, `Stability +0.1`. **Purification:** `Distortion = 0.4`, `Stability` и `Purity` растут.

**Восприятие:** Драматический момент варки — зелье либо «схлопывается» в мутную жижу, либо внезапно очищается и начинает сиять.

**Связи:**
- [[Distortion]]
- [[Alchemy]]
- [[Stability]]
- [[Purity]]