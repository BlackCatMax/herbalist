---
tags: [technical, current, core]
gdd: "[[00_Core_Lock]]"
version: 1.0
based_on: ProjectHerbalist source, 2026-09-22
---
# 00. Core Lock — техническая сторона

Пара к главе [[00_Core_Lock|00. Core Lock — Инварианты системы]]: где в коде
держится каждый инвариант, номера разделов совпадают.

| Инвариант | Где держится |
|---|---|
| §1 Единое состояние мира | клетки `FGridCell` менеджера `AGridWorldManager` и узлы графа — [[13_World_Pipeline_Tech]], [[14_Biome_Graph_Tech]] |
| §1.1 Алатырь S₀ | `FAlatyr::S0` — [[05_Systems_Tech]] §5 Параметрическая модель |
| §2–§3 Переход и единственный поток | команды → `Simulation::ExecutePipeline` → `FStateDelta` → `ApplyStateDelta` (Single-Writer) — [[13_World_Pipeline_Tech]] §13.5–§13.8 |
| §6 Причинность | детерминизм и повтор тика — `ReplayAndCompare`, [[13_World_Pipeline_Tech]] «Детерминизм и трасса» |
| §7 Восприятие | S_real видит только симуляция; игроку — воспринятое (`Simulation::FPerceptionService`, воспринятое состояние в котомке и Травнике, `PerceiveClass`) — [[05_Systems_Tech]], [[07_UX_Tech]] |
| §9 Локальность | радиус симуляции и страницы клеток — [[13_World_Pipeline_Tech]] §13.19 |
