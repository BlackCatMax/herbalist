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
  `WorldChanges` (итог шага по клетке), `CellApplications` (состояние,
  оставленное каждым зельем на клетке, — по ним считаются подношения капищу
  и хозяину места), `InventoryOps`, `BiomeActivations`, `TargetStateNudges`
  (мягкая цель релаксации), `Footprints` (след в графе).
- **Несколько команд на одну клетку за шаг** (с 2026-09-26): каждая видит
  клетку уже изменённой предыдущими командами шага (`FindCellForCommand`),
  а не снимок на начало шага — два сбора истощают клетку дважды, два зелья
  на капище — два подношения. Порядок — порядок команд в пакете, так что
  повтор тика детерминирован.
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

## PCG: сетка в графе и граф в сетке

PCG в проекте — редакторный и рантаймовый инструмент растительности; сама
симуляция от PCG не зависит (регионы биомов — обычные акторы, менеджер
находит их при `BeginPlay`). Связь двусторонняя: граф читает состояние
клеток и пишет места для ресурсов.

### Свои узлы (`Core/PCG/`)

| Узел | Вход → выход | Настройки | Когда нужен |
|---|---|---|---|
| **Get Herbalist Grid** (`UPCGHerbalistGridSettings`) | ничего → по точке на клетку | `bExcludeWaterCells` (нет), `bOnlyCellsClaimedByBiomeRegions` (да), `bOnlyActiveCells` (да — только клетки вокруг игрока) | правила «от состояния»: вытоптанное редеет, у капища гуще, под Мороком не растёт |
| **Sample Herbalist Cell** (`UPCGHerbalistSampleCellSettings`) | чужие точки → те же точки с состоянием клетки под ними | `HealthyMeshKey` («Healthy»), `DegradingMeshKey` («Degrading»), `bDropPointsOutsideGrid` (нет), доли сезона `Spring/Summer/Autumn/WinterDensity` (1 / 1 / 0.8 / 0.4) | трава и кусты, которые меняют меш по порче и сезону |
| **Write Herbalist Resource Slots** (`UPCGHerbalistWriteResourceSlotsSettings`) | точки мест → те же точки; запись в ассет `RS_<карта>` | `DefaultKind` (Land), `KindAttribute` («SlotKind») | места, где встают собираемые ресурсы; только редактор |

Атрибуты точек **Get Herbalist Grid**: `Distortion`, `Corruption`, `Purity`,
`Stability`, `HarvestStress`, `ShrineRestoration`, `Biome`, `bIsWater`,
`ManifestedEntity`. **Sample Herbalist Cell** пишет `Distortion`,
`Corruption`, `HarvestStress`, `Biome`, `bDegrading`, `MeshKey`
(по `bDegrading` — липкому флагу испорченного полюса, чтобы флора не
дребезжала у порога), `SeasonKey` (`Spring`…`Winter`) и `SeasonMeshKey`
(`Healthy_Winter`) — его и сопоставляет `PCGMeshSelectorByAttribute`
спавнера по строке. Прореживание по сезону вложенное: зимний набор —
подмножество осеннего, держать `WinterDensity ≤ AutumnDensity`.

Все три узла работают только на игровом потоке и не кэшируются. Первые два
видят клетки **только в игре**: в редакторе сетки нет, узел пишет
предупреждение в лог графа и пропускает точки как есть (или отдаёт ноль
точек). Поэтому граф, который читает сетку, — только с
**Generate at Runtime**.

Непрерывный отклик (пожелтение по мере порчи, примятая трава, сезонный
цвет) — не PCG, а материалы через карту состояния мира и функции
`MF_*` (`docs/reference/TOOLS_REFERENCE.md`, «Функции материалов»):
PCG перестраивает точки редко, материал читает карту каждый кадр.

### Трава PCG

- Граф висит PCG-компонентом в `BP_BiomeVolume`, три спавнера на GPU.
  Цепочка: точки по ландшафту → `World Raycast` → `Projection` (вес слоя
  `Ground` в атрибут) → фильтр «`Ground` < 0.8» (на покраске тропы травы
  нет) → **Sample Herbalist Cell** → `Attribute Noise` → спавнеры с выбором
  меша по `SeasonMeshKey`.
- Густота — `Interior Sample Spacing` у сэмплера сплайна региона (не у того,
  что идёт во вход `Differences` — тот вырезает зону): 8 см с 2026-09-27
  (было 10), точек на м² — (100 / шаг)². На глаз густоту даёт и масштаб
  групп `Transform Points`: пятая часть травы — 0.2–0.4, почти не видна.
  Осенью и зимой редеет по долям сезона **Sample Herbalist Cell**.
- Узлы сезона и тропы вставляет `-run=PcgGrassSeasonSetup` (идемпотентно,
  узлы ищет по типам — граф можно править руками).
- Рантайм: `-run=WorldPartitionBuilderCommandlet /Game/Maps/L_TestDev
  -Builder=PcgGrassRuntimeBuilder` — у шаблона компонента и экземпляров на
  карте `GenerateAtRuntime` с разбиением (сетка 64 м, радиус 128 м), у PCG
  World Actor кэш ландшафта `SerializeOnlyAtCook`. Без последнего в PIE
  `Get Landscape Data` пуст и травы нет вовсе. Из Git Bash — с
  `MSYS_NO_PATHCONV=1`.
- Сплайн региона берёт `Get Spline Data` с фильтром актора **`Original`**
  (владелец исходного компонента). Не `Parent`: в игре граф считается
  ячейками на временных `PCGPartitionActor`, и `Parent` у них — сам
  партиционный актор; в редакторе работало, в игре травы не было
  (2026-09-27). Правит `-run=PcgGrassSeasonSetup`.
- Подводные камни: `GenerateOnLoad` запекает граф в редакторе без сетки —
  сезона и порчи не будет; сменили сезон — новые клетки получат его при
  следующей генерации (уйти за радиус и вернуться).

### Деревья PCG

Решение пользователя (2026-09-28): отдельный граф на том же `BP_BiomeVolume` и
сплайне, запекание в редакторе. Трава генерируется в игре по состоянию клеток,
деревьям это не нужно: сезон у них — материал (`MF_LeafDrop`), а
запечённые деревья получают HLOD, коллизию и свет.

- Второй PCG-компонент `PCG_Trees` в `BP_BiomeVolume`, `GenerateOnDemand`,
  без разбиения. Собирает `-run=PcgTreesSetup` (таблица, граф, компонент;
  каждый шаг — только если не сделан).
- Цепочка: сплайн региона (`Get Spline Data`, фильтр `Original`, тег
  компонента `Biome`) → `Create Surface From Spline` → `Surface Sampler`
  (4 точки на 100 м² мира; выборка по внутренности сплайна считала бы шаг в
  локальных единицах, а объёмы растянуты ×8.5) → `Projection` на ландшафт
  (только позиция) → `Ground < 0.8`
  (на покраске тропы деревьев нет) → **Herbalist Biome Trees** → `Self
  Pruning` → `Static Mesh Spawner` (меш из атрибута `TreeMesh`).
- **Herbalist Biome Trees** (`Core/PCG/PCGHerbalistBiomeTrees.h`): биом —
  у объёма-владельца (`GetOriginalComponent`: при разбиении владелец раздела
  — не объём), набор — строки `/Game/Data/DT_BiomeTrees`
  (`FHerbalistBiomeTreeRow`: биом, меш, `TreesPer100SquareMeters`,
  `ScaleMin/Max`, `SpacingMeters`). Точка остаётся с долей «плотность биома /
  плотность сэмплера», умноженной на затухание к краю региона
  (`DensityFalloffStrength` объёма, как у ресурсов); вид — по плотностям
  строк; масштаб, поворот вокруг вертикали, границы точки радиусом
  `SpacingMeters / 2` — по ним `Self Pruning` разводит стволы.
- **Вода** (2026-09-28, по PIE: деревья в пруду). Точку над любым
  `AWaterRegionVolume` мира отбрасывает сам узел — `IsPointInside` по X-Y, в
  логе «над водой отброшено N»; видит только загруженные объёмы воды (билдер
  L_TestDev грузит всю карту). Прежнее вычитание поверхности пруда
  (`Difference`) в графе не срабатывало: оно сравнивает точки в 3D, а сплайн
  пруда и сплайн региона на разной высоте. Старый граф без пересоздания
  ассета: `-run=PcgTreesSetup -rebuildgraph`.
- **Ресурсы под деревьями** (2026-09-28, по PIE: «нет ресурсов там, где
  растут деревья»). Проверка места при посадке ресурса
  (`AGridWorldManager::IsSpawnPointBlocked`, сфера `SpawnClearanceRadius`)
  цепляла капсулы ветвей и кроны. Экземпляр меша на объёме региона теперь
  занимает только ствол: `TreeTrunkClearanceCm` (50 см) × масштаб экземпляра
  от основания. Остальная геометрия объёмов (меш воды) занимает место как
  раньше. Ресурсы уже старого сохранения сидят на своих местах — видно на
  новом.
- **Густота** — строки таблицы (`herbalist_docs/CSV_tabs/biome_trees.json`,
  перезалить: `-run=PcgTreesSetup -refilltable`) или прямо в
  `DT_BiomeTrees` в редакторе. Сумма плотностей биома не выше 4 на 100 м²
  (плотности сэмплера); выше — предупреждение узла, лес реже задуманного.
- **Запекание**: `MSYS_NO_PATHCONV=1 UnrealEditor-Cmd.exe <uproject>
  -run=WorldPartitionBuilderCommandlet /Game/Maps/L_TestDev
  -Builder=PCGWorldPartitionBuilder -IncludeGraphNames=PCG_Trees
  -GenerateComponentEditingModeNormal -AllowCommandletRendering`. Сдвинул
  объём или сплайн, сменил таблицу — перезапечь (или Generate на компоненте
  в редакторе и сохранить карту).
- Не сделано: трава под стволами (граф травы мог бы вычесть точки деревьев
  через `Get PCG Component Data`); `Nanite` на деревьях включён, хотя для листопада маской план
  советует его выключать (`DESIGN_Living_Vegetation_Research.md` §3.2).

### Слоты ресурсов у воды

- Граф висит PCG-компонентом (`GenerateOnDemand`) в `BP_WaterVolume` и
  строит точки от сплайна своего актора: вода (0.3 на м²), кромка (сдвиг до
  1.5 м), суша (до 9 м), атрибут `SlotKind` = `Water` / `Shore` / `Land`, всё
  — в **Write Herbalist Resource Slots**. Собирает граф и компонент
  `-run=PcgResourceSlotsSetup` (непустой граф не трогает).
- Запекание в `/Game/Data/ResourceSlots/RS_<карта>` — движковым билдером,
  без `-nullrhi`: `-run=WorldPartitionBuilderCommandlet /Game/Maps/L_TestDev
  -Builder=PCGWorldPartitionBuilder -IncludeGraphNames=PCG_ResourceSlots
  -GenerateComponentEditingModeNormal -AllowCommandletRendering`; в логе
  `[Slots] <актор>: записано N слотов`. В редакторе узел перезаписывает
  набор своего актора при каждой генерации.
- В игре менеджер сетки сам грузит `RS_<карта>` при старте (в логе
  `[Slots] Слоты ресурсов …: N`) и ставит ресурсы клетки в её слоты: водные
  виды — в `Water`/`Shore`, остальные — в `Shore`/`Land`
  (`SlotSuitsSpecies`). Клетка без слотов — прежний разброс по клетке.
- Пруд передвинули или переименовали — перезапечь; набор удалённого актора
  в ассете остаётся (удалить ассет и перезапечь).

### Ресурсы, расставленные графом

Альтернатива заселению из C++: у региона снять `bSpawnResourcesFromGrid`
([[10_Biomes_Reference_Tech#Регион биома на карте]]), в графе —
`Spawn Actor` → `AHerbalistResourceActor` с override `IngredientID`
(строка `DT_IngredientClass`). Актор сам регистрируется на клетке при
`BeginPlay`, собирается и отрастает как обычный. Сценарий —
`docs/verification/pie/02_Harvest_Inventory.md`, «Расстановка графом PCG».

## Детерминизм и трасса

`Simulation::ReplayAndCompare` (`TraceReplay.h`) повторяет записанный кадр
трассировки на том же сиде и сравнивает все пять полей дельты по значению.
Консоль: `ke * DumpTrace`, `ke * ReplayLastTick`
(`docs/verification/pie/04_World_State.md`).
