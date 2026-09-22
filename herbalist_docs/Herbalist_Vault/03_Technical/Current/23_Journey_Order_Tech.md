---
tags: [technical, current, journey]
gdd: "[[23_Journey_Order]]"
version: 1.0
based_on: ProjectHerbalist source, 2026-09-22
---
# 23. Порядок пути — техническая сторона

Пара к главе [[23_Journey_Order|23. Порядок пути]], номера разделов
совпадают. Код маршрута записан в самой главе (§23.7) — здесь то, что
изменилось после и что нужно в редакторе.

## §23.3 Клубочек

Сделан 2026-09-21 (в главе — «отложен»): у лица и взаимодействие, база —
строкой выбора ([[21_Journey_And_Artifacts_Tech]] §21.2).

## §23.6 Якорь ясности

Вес фрагмента — `ClarityGain` ряда `DT_MemoryFragments`
(`-run=MemoryFragmentsCreate -sync`), пересчёт —
`RecomputeClarityAnchorFromFragments` ([[20_Investment_And_Progression_Tech]]).

## §23.7 Код и карта

- БРОД — `TrySpawnStateBasedFragment`, `BrodSustainedSeconds`.
- **Маршрут на картах не собран:** на `L_TestDev` регионов два (Болото и
  Широколиственный лес), Лесостепь и Пойма — заглушки; ПЕРВАЯ ВАРКА и
  ПОДНОШЕНИЕ недостижимы. Решение пользователя 2026-09-19 — перестроить
  карту под маршрут ([[Level_Assembly]], «Карта и мир»).
- Регрессия: `Herbalist.JourneyOrder.*`.
