---
tags: [glossary, technical, implemented]
status: ✅
---

# RngState (Генератор случайных чисел)

**GDD:** Детерминированный источник случайности.

**Tech:** `FRngState` с полем `Seed`. Функция `Random01` — линейный конгруэнтный генератор.

Используется в:
- [[Alchemy]] (шум [[Morok]])
- [[GridWorldManager]] (генерация ресурсов)