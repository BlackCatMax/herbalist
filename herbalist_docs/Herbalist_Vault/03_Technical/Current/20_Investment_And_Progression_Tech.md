---
tags: [technical, current, progression]
gdd: "[[20_Investment_And_Progression]]"
version: 1.0
based_on: ProjectHerbalist source, 2026-09-22
---
# 20. Зачем варить — техническая сторона

Пара к главе [[20_Investment_And_Progression|20. Зачем варить — инвестиция и
прогрессия]], номера разделов совпадают.

## §20.3 Прогрессия: якорь и отклик

- Глобальная ясность — `AGridWorldManager::GetGlobalPerceptionClarity`,
  пересчёт — `RecomputeGlobalPerceptionClarity` (`GridWorldManagerZaryana.cpp`):
  якорь из собранных фрагментов (`RecomputeClarityAnchorFromFragments`,
  вес `ClarityGain` фрагмента, глава [[23_Journey_Order]] §23.6) плюс
  сглаженный отклик на состояние мира (`ClarityResponse*` в настройках).
  Якорь и сглаженный отклик сохраняются.

## §20.4 Что ясность делает

Ясность — шум восприятия (`Simulation::FPerceptionService`): сила шума
тултипов, строки ощущения, `PerceiveClass` ([[05_Systems_Tech]] §5 Инвентарь),
воспринятая роса Заряны ([[19_Rosa_Signal_Tech]]).
