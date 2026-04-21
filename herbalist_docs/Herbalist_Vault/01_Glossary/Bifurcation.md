---
tags: [glossary, mechanic]
status: ✅
---

# Bifurcation

**GDD:** Резкое изменение состояния зелья при критическом искажении — либо коллапс в хаос, либо очищение.

**Tech:** В `GridWorldManager::ApplyAlchemyResult`: порог `Distortion > 0.85`. **Collapse:** `Distortion = 0.2`, `Stability +0.1`. **Purification:** `Distortion = 0.4`, `Stability` и `Purity` растут.

**Восприятие:** Драматический момент варки — зелье либо «схлопывается» в мутную жижу, либо внезапно очищается и начинает сиять.

**Связи:**
- [[Distortion]]
- [[Alchemy]]
- [[Stability]]
- [[Purity]]