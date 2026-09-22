---
tags: [technical, current, zaryana]
gdd: "[[19_Rosa_Signal]]"
version: 1.0
based_on: ProjectHerbalist source, 2026-09-22
---
# 19. Чёрная роса — техническая сторона

Пара к главе [[19_Rosa_Signal|19. Чёрная роса — Заряна как измерительный
прибор]], номера разделов совпадают. Код — `GridWorldManagerZaryana.cpp`.
Проверка — `docs/verification/pie/07_Zaryana_Journey.md` («Роса»).

## §19.2 Три слоя

- Клетка Заряны — у котла (`SetZaryanaCellIfUnset`) или расставлена
  вручную.
- Слой 1 — воспринятое состояние клетки: `GetZaryanaPerceivedState`
  (шум по глобальной ясности); честное — `GetZaryanaTrueState`
  (Зеркальце при прогреве, [[21_Journey_And_Artifacts_Tech]]).
- Слой 2 — первый ложный сигнал: `UpdateRosaSignal`, флаг
  `SetRosaFirstFalseSignalShown` (сохраняется).
- Слой 3 — сигнал расширяется от клетки к миру.

## §19.4a Первый кадр

`SeedRosaCorruptedCircle` — при первом размещении клетки Заряны круг порчи
радиусом `RosaCorruptedCircleRadiusMeters` (пики —
`RosaCorruptedCirclePeakDistortion`, `…PeakCorruption`), текст — через
`ShowMemoryRevealText`. Единственная известная запись в мир мимо
`ApplyStateDelta` ([[13_World_Pipeline_Tech]] §13.5–§13.8).

Регрессия: `Herbalist.Zaryana.*`.
