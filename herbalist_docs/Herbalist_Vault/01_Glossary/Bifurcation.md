---
tags: [glossary, mechanic, implemented]
status: ✅
---

# Bifurcation (Бифуркация)

**GDD:** Резкое изменение состояния при критическом искажении — коллапс или очищение.

**Tech:** В `GridWorldManager::ApplyAlchemyResult`:
- Порог `Distortion > 0.85`
- Вероятность зависит от `Instability` и `StabilityMemory`
- **Collapse:** `Distortion = 0.2`, `Stability +0.1`
- **Purification:** `Distortion = 0.4`, `Stability` и `Purity` растут.

**Восприятие:** Драматический момент — либо катастрофа, либо прорыв.