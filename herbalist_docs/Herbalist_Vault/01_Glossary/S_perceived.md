---
tags: [glossary, perception]
status: ✅
---
# S_perceived

**GDD:** Воспринимаемое состояние мира. Формируется как искажённая версия [[S_real]] через Morok.

**Tech:** `Perception::PerceiveValue(RealValue, GlobalDistortion, Random)` — мультипликативное искажение значений, используется в `ItemTooltipWidget`. **Реализовано.** `Perception::PerceiveClass` (вероятностная подмена класса предмета) в коде не существует — не реализовано.

**Связи:** [[S_real]], [[Morok]], [[Perception]], ItemTooltipWidget