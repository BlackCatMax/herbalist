---
tags: [technical, current, intent]
gdd: "[[11_Intent_Evolution]]"
version: 1.0
based_on: ProjectHerbalist source, 2026-09-22
---
# 11. Намерение и эволюция мира — техническая сторона

Пара к главе [[11_Intent_Evolution|11. Намерение и эволюция мира]], номера
разделов совпадают.

## §11.1–§11.2 Намерение

Намерение системы — `FIntent::Coherence`, считается из самих ингредиентов
варки (`ComputeIntentCoherence`, `PipelineV2.cpp`): вес по порядку закладки,
согласие ведущих осей, качество, вода. Порядок закладки в котёл — порядок
жестов ([[07_UX_Tech]] §7.13.4).

## §11.5 Связь с прогрессией

История намерения места — `FMemoryState::AverageCoherence` (EMA применений
на клетку), расстояние до Алатыря с историей — `DistanceWithHistory`
([[15_Cycles_And_Shrines_Tech]] §15.5.1).

## §11.7 Намерение и капища

Надбавка к Coherence в радиусе капища — `ShrineCoherenceBonus`
([[15_Cycles_And_Shrines_Tech]] §15.5).
