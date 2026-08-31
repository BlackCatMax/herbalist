---
tags: [glossary, system]
status: ✅
---

# Alchemy

**GDD:** Ключевая система преобразования ресурсов. Результат формируется динамически на основе состава, порядка, контекста и уровня [[Morok]]. Нет фиксированных рецептов.

**Tech:** `HerbalistCore::Pipeline::ApplyMorok` в коде не существует. Реальная точка входа — `Simulation::ExecutePipeline` / `PipelineV2.cpp`: `ComputeApplyResult` для команд Apply, `GenerateHarvestResult` для Harvest, `ComputeIntentCoherence` для Coherence. Этапы: Fold (агрегация с весами), учёт воды, построение Delta, модификаторы (Potency, Resonance, Corruption), Morok (нелинейное искажение), Zaryana (структурирование), Bifurcation (коллапс/очищение).

**Восприятие:** Игрок наблюдает за цветом, плотностью, звуками и согласованностью кипения. Оценка требует опыта и может быть искажена [[Morok]].

**Связи:**
- [[Fold]]
- [[Delta]]
- [[Morok]]
- [[Zaryana]]
- [[Bifurcation]]