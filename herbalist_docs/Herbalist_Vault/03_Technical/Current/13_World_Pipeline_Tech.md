---
tags: [technical, current, pipeline]
gdd: "[[13_World_Pipeline]]"
version: 3.0
based_on: ProjectHerbalist source, 2026-09-22
---
# 13. Полный пайплайн мира — техническая сторона

Пара к главе [[13_World_Pipeline|13. Полный пайплайн мира]], номера разделов
совпадают. Поглотил детерминированную часть прежнего снимка ядра
(Core_Current, 2026-09-22). Модель параметров, сбор и варка — в
[[05_Systems_Tech]], граф биомов — в [[14_Biome_Graph_Tech]], темпы и
формулы — `docs/reference/MATH_REFERENCE.md`, решения по разметке мира —
`docs/design/DESIGN_World_Layout.md`. Проверка —
`docs/verification/pie/04_World_State.md`.

## §13.1 Общая структура цикла

Цикл мира ведёт `AGridWorldManager` (`Core/World/GridWorldManager.h`,
реализация разложена по `GridWorldManager*.cpp`). Каждый `Tick`:

1. Накопитель гоняет фиксированные шаги симуляции (`SimulationFixedTimeStep`
   0,05 с) — `RunSimulationStep` (`GridWorldManagerTick.cpp`): команды из
   очереди (`QueueCommand`) собираются в `FCommandBatch`, снимки состояния
   (`CaptureState`), вызов `Simulation::ExecutePipeline`, запись
   `ApplyStateDelta`, следом внепайплайновые реакции (Травник, след в графе,
   `OnBrewCompleted`, подношения хозяевам и капищам, Роса Заряны).
2. Реже — медленные процессы: регенерация и релаксация клеток
   (`RegenerateCellParameters`), проявления сущностей, капища, заказы, погода,
   снимок порчи раз в 30 с.

Граф биомов тикает сам (`UBiomeGraphSubsystem`, свой шаг 0,2 с), связь —
[[14_Biome_Graph_Tech]].

## §13.5–§13.8 Сбор, преобразование, результат, применение

Все четыре — команды одного пайплайна:

- **Команды** (`Core/Simulation/Public/CommandTypes.h`): `FCommandBatch` —
  плоский список `FCommandEntry`, исполняется по порядку. Примитивы
  `ECommandPrimitive`: `Query`, `Transfer`, `Apply`, `Harvest`, `Wait`,
  `Talk` (`Query` и `Talk` дельты не дают).
- **Чистая функция** `Simulation::ExecutePipeline(FWorldSnapshot,
  FInventorySnapshot, FBiomeSnapshot, FCommandBatch, FRandomStream&)`
  (`Core/Simulation/Private/PipelineV2.cpp`): читает только снимки, пишет
  только возвращаемую дельту — поэтому гоняется в автотестах без мира.
  Сбор — `ProcessHarvestCommand`, варка и применение — `ProcessApplyCommand`
  / `ComputeApplyResult` (подробно — [[05_Systems_Tech]]).
- **Дельта** (`DeltaTypes.h`) `FStateDelta` — единственный канал изменений:
  `WorldChanges` (перезапись клетки), `InventoryOps`, `BiomeActivations`,
  `TargetStateNudges` (мягкая цель релаксации), `Footprints` (след в графе).
- **Single-Writer:** в мир пишет только `ApplyStateDelta`. Известное
  исключение — `SeedRosaCorruptedCircle` (первое размещение Заряны,
  `GridWorldManagerZaryana.cpp`).
- **Снимки** (`SnapshotTypes.h`): `FWorldSnapshot`, `FInventorySnapshot`,
  `FBiomeSnapshot` — замороженные срезы на момент тика.

## §13.9 Момент вычисления

Жест игрока ставит команду в очередь; результат приходит следующим шагом
симуляции — поэтому, например, зелье из котла появляется в котомке на
следующем тике, и котёл держит свободное место под каждую ещё не пришедшую
варку. Исключения, считающиеся на месте, — вне пайплайна: шаг ритуала
(`TryAdvanceRitual`), подношение общине, заказы, диалоги.

## §13.10 Жизненный цикл изменения

Клетка релаксирует к цели (`TargetState`) — `RegenerateCellParameters`
(`GridWorldManagerCore.cpp`); цель двигают `TargetStateNudges` и поля графа
(`ApplyBiomeInfluences`). Бистабильность порчи (`FMemoryState::bDegrading`)
и инвариант «биом не хуже своей нормы от пассивной симуляции» —
глава [[12_Biome_Change]], `MATH_REFERENCE.md`.

## §13.11 Роль капищ

[[15_Cycles_And_Shrines_Tech]] (`AShrineActor`, `GridWorldManagerShrine.cpp`).

## §13.13 Влияние Морока и Заряны

Морок и Заряна — поля узлов графа и множители в `ComputeApplyResult`
([[14_Biome_Graph_Tech]], [[05_Systems_Tech]] §5 Biome Context Injection);
восприятие — `Simulation::FPerceptionService` (`PerceptionService.cpp`),
глобальная ясность `GetGlobalPerceptionClarity` (глава [[19_Rosa_Signal]]).

## §13.19 Масштаб мира и разметка

Сетка менеджера увязана с World Partition (решения пользователя 2026-09-12,
`docs/design/DESIGN_World_Layout.md`):

- **Разметка** — `FHerbalistWorldLayout` (`Core/World/WorldLayout.h`) на
  акторе менеджера: исходные величины из ландшафта и разбиения стриминга
  (`FHerbalistWorldLayoutSource`), ручные поправки
  (`FHerbalistWorldLayoutOverrides`), расчёт — `FWorldLayoutSolver`. Клетка —
  целое число квадов, делящее компонент ландшафта; страница данных — ячейка
  стриминга; чанк активности — делитель страницы, покрывающий самый
  дальнобойный локальный механизм; координата клетки — от начала сетки World
  Partition (бывает отрицательной).
- **`L_TestDev`** (рабочая карта): клетка 9 м, сетка 224×224 от (−112, −112),
  страница 14 клеток (126 м), чанк 7 клеток, действующий радиус симуляции
  63 м. Строка `[Layout] …` при старте — `docs/verification/pie/01_Start.md`.
  Дефолт тестового менеджера (1 м, 20×20) — только для автотестов.
- **Всё физическое — в метрах**: плотность ресурсов на площадь, радиусы и
  скорость заражения пересчитываются по клетке.
- **Страницы клеток** (`FHerbalistCellPage`, `Core/World/CellPageTypes.h`):
  грузятся и выгружаются с землёй под ними, нетронутая страница
  пересчитывается из сида клетки (`MakeCellRandomStream`), тронутые клетки
  хранятся дельтой. Сводки чанков (`FHerbalistChunkSummary`) заменяют обходы
  всего мира, в том числе для графа.
- **Сейв** несёт отпечаток разметки (`FHerbalistSavedWorldLayout`);
  другая разметка — отказ загрузки ([[07_UX_Tech]] §7.13.7, `pie/08_Saves.md`).
- **В редакторе:** запечь разметку — кнопка «Сверить с World Partition» на
  менеджере или `-Builder=WorldLayoutSyncBuilder`
  (`docs/reference/TOOLS_REFERENCE.md`); регионы биомов — без
  пространственной загрузки. Сводка — [[Level_Assembly]].

## Детерминизм и трасса

`Simulation::ReplayAndCompare` (`TraceReplay.h`) повторяет записанный кадр
трассировки на том же сиде и сравнивает все пять полей дельты по значению.
Консоль: `ke * DumpTrace`, `ke * ReplayLastTick`
(`docs/verification/pie/04_World_State.md`).
