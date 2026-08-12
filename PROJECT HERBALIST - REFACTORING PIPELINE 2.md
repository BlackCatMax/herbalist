Ниже представлен **итоговый архитектурный документ**, который аккумулирует все результаты аудита, согласованную целевую модель, формальную алгебру команд, тиковую модель, спецификацию причинности, систему трассировки/повторов и пошаговый план миграции в виде независимых PR. Документ самодостаточен и предназначен для продолжения работы в новых сессиях.

---

# 🏛️ PROJECT HERBALIST — DETERMINISTIC SIMULATION ENGINE

## Финальный архитектурный документ и план рефакторинга

**Статус:** утверждён  
**Версия:** 1.0  
**Дата:** 2026-04-28

---

## Содержание

1. [Контекст и цель](#контекст-и-цель)
2. [Результаты аудита](#результаты-аудита)
3. [Целевая архитектура](#целевая-архитектура)
4. [Слои системы](#слои-системы)
5. [Command Algebra](#command-algebra)
6. [Tick Execution Model](#tick-execution-model)
7. [Causal Execution Spec](#causal-execution-spec)
8. [Execution Trace & Replay Engine](#execution-trace--replay-engine)
9. [Структура папок](#структура-папок)
10. [Критические структуры данных](#критические-структуры-данных)
11. [План миграции (PR план)](#план-миграции-pr-план)
12. [Критерии готовности](#критерии-готовности)
13. [Фактический статус реализации](#фактический-статус-реализации)
14. [Глоссарий](#глоссарий)

---

## Контекст и цель

**Project Herbalist** — игра на Unreal Engine 5.7, центральной механикой которой является алхимическая трансформация ресурсов, зависящая от состояния мира (биомов, искажений). Текущая реализация содержит сильное архитектурное ядро, но страдает от:

- нарушения единственного источника истины (I‑4),
- скрытых зависимостей от глобальных подсистем и настроек,
- отсутствия детерминизма,
- смешения симуляции и отображения.

**Цель:** превратить проект в **детерминированную замкнутую симуляционную машину** с формально определённой причинностью и полным разделением вычислений, состояния мира и интерфейса.

---

## Результаты аудита

Аудит 78 файлов выявил следующие ключевые проблемы:

- **K1 (I‑4):** `FRealState` не является единственным источником истины. Ссылки на `GetHerbalistSettings()`, Subsystems и DataTable внутри Pipeline.
- **K2:** `FRngState` не всегда контролируется → недетерминизм.
- **K3:** `HarvestService` использует глобальную `ResourceAvailabilityMap` вместо `FRealState` клетки.
- **K4:** `GameModeBase` и `AlchemySubsystem` дублируют загрузку DataTable.
- **K5:** `TActorIterator` и `LoadObject` в рантайме.
- **K6:** Отсутствует слой `S_Perceived` (восприятие).
- **K7:** UI‑виджеты напрямую вызывают игровые сервисы.

**Соответствие GDD:** ~85% по архитектурному ядру, но нарушен инвариант I‑4 и полностью отсутствует слой восприятия. Остальное либо в планах, либо не реализовано.

---

## Целевая архитектура

Система перестраивается в **детерминированный конвейер симуляции**, где вся игровая логика выражена через преобразования состояний.

**Принцип:**
```
Output = Pipeline(Snapshot, CommandGraph, RNG)
```

- **Snapshot** — полный снимок мира на начало тика.
- **CommandGraph** — ориентированный граф без циклов (или линейный список) примитивных команд.
- **RNG** — фиксированный seed из контекста.

Состояние мира изменяется **только** через `FStateDelta`, полученный от Pipeline, и применяемый в фазе World Apply.

---

## Слои системы

| Слой | Назначение | Допустимые действия |
|------|------------|---------------------|
| **L0 Command Layer** | Ввод игрока → команды | Создание `FCommand`, отправка в `CommandBus` |
| **L1 Simulation Core** | Вычисление `Delta` | Чистая функция от `Snapshot` и `CommandGraph`; без сайд-эффектов |
| **L2 World State** | Хранение и применение состояния | `ApplyDelta()`, `CaptureSnapshot()`; никакой логики |
| **L3 View (UI)** | Отображение `S_Perceived` | Только генерация команд; не читает `S_real` напрямую |

**Ключевое правило:** данные движутся строго в одном направлении —  
`Command → Snapshot → Pipeline → Delta → World → (Macro) → Perception → UI`

---

## Command Algebra

Любое игровое действие выражается композицией **пяти примитивов**:

| Примитив | Обозначение | Смысл |
|----------|--------------|-------|
| Query | Q | Чтение из Snapshot, возврат данных |
| Transform | T | Чистое преобразование `State → State` |
| Delta Apply | D | Операция, порождающая изменение в `Delta` |
| Sequence | S | Последовательное выполнение команд |
| Branch | B | Условное ветвление на основе Snapshot |

**Структура команды:**
```cpp
struct FCommand {
    EPrimitive Type;
    FCommandPayload Payload;
    TArray<FCommand> Children;
};
```
**Запрещено:** прямое указание игровых действий (Harvest, MoveItem). Всё реализуется через композиции примитивов.

**Пример (Harvest):**
```
S([
    Q(CellState),
    B(IsHarvestable,
        S([ T(ComputeYield), D(AddToInventory), D(UpdateCellState) ]),
        D(NoOp)
    )
])
```

---

## Tick Execution Model

Один тик симуляции разбит на **семь неизменных фаз**:

1. **Command Intake** — сбор команд из UI/PlayerController, построение графа.
2. **Snapshot Capture** — атомарный захват `World`, `Inventory`, `Biome`, RNG.
3. **Command Normalization (IR Compilation)** — преобразование графа команд в линейный IR.
4. **Pipeline Execution** — интерпретация IR над `WorkingState`, генерация `FStateDelta`.
5. **World Apply** — применение Delta (единственная мутация).
6. **Biome Propagation** — макро‑эволюция биомов на основе Delta.
7. **Perception Update** — вычисление `S_Perceived` для UI.

Все фазы детерминированы; информация течёт только вперёд.

---

## Causal Execution Spec

Формальные законы для валидации причинности:

- **No backward influence:** `State(t)` никогда не зависит от `State(t+1)`.
- **Snapshot Immutability:** снапшот заморожен после создания.
- **Single Writer:** только `Delta` изменяет `World`.
- **No implicit reads:** любое чтение состояния должно быть явно в снапшоте.
- **DAG only:** граф команд должен быть ациклическим.
- **Determinism:** `одинаковый (Snapshot, Commands, RNG) → идентичный IR → идентичная Delta → идентичный World`.

---

## Execution Trace & Replay Engine

**Назначение:** запись и бит‑точное воспроизведение любого тика для отладки и верификации.

**Компоненты:**
- `TraceRecorder` — записывает хеши снапшотов, IR, дельты, RNG‑диапазоны.
- `ReplayExecutor` — может либо перевыполнить Pipeline (recompute), либо просто применить записанные дельты (playback).
- `ValidationLayer` — сверяет хеши при реплее, гарантируя отсутствие расхождений.
- `ExecutionTrace` — линейный лог структур `FTraceEntry` с `TickIndex`.

**Использование:** включается в режиме отладки; не влияет на геймплей. Позволяет откатиться на любой тик и пошагово выполнить Pipeline.

---

## Структура папок

```
Source/ProjectHerbalist/
├── Core/
│   ├── Simulation/
│   │   ├── Pipeline/
│   │   │   ├── PipelineCore/          (HerbalistPipeline, PipelineContext, PipelineResult)
│   │   │   ├── Commands/              (CommandTypes, CommandGraph, CommandBus)
│   │   │   ├── IR/                    (CommandIR, IRCompiler)
│   │   │   └── Stages/                (Morok, Zaryana, Fold, IntentResolver)
│   │   ├── Snapshot/                  (WorldSnapshot, InventorySnapshot, BiomeSnapshot, SnapshotService)
│   │   ├── Delta/                     (StateDelta)
│   │   ├── Tick/                      (SimulationTickManager)
│   │   └── Trace/                     (ExecutionTrace, TraceRecorder, ReplayEngine)
│   ├── World/                         (GridWorldManager, WorldState, WorldApply)
│   ├── Biome/                         (BiomeGraphSubsystem, BiomePropagation)
│   ├── Data/                          (IngredientTableRow, WaterTypeRow, StaticDataSnapshot)
│   ├── Inventory/                     (InventoryComponent, InventorySnapshot)
│   ├── Harvest/                       (HarvestService)
│   ├── Types/                         (HerbalistCoreTypes, CoreMath, RNGState)
│   └── Perception/                    (PerceptionComponent, PerceptionModel)
├── Gameplay/
│   ├── Player/                        (PlayerController)
│   ├── Interaction/                   (ResourceActor, StorageContainer)
│   └── GameMode/                      (GameModeBase)
├── UI/
│   ├── Widgets/                       (Inventory, Slot, Tooltip, AlchemySlot...)
│   ├── Controllers/                   (DragDropController)
│   └── ViewModel/                     (UICommandAdapters)
└── Subsystems/                        (IngredientRegistry, WaterTypeRegistry, AlchemySubsystem)
```

---

## Критические структуры данных

### `FWorldSnapshot`
```cpp
struct FWorldSnapshot {
    TMap<FGridCellId, FRealState> CellStates;
    int32 TickIndex;
    FRngState GlobalRngState;
};
```

### `FStateDelta`
```cpp
struct FStateDelta {
    TMap<FGridCellId, FRealState> CellUpdates;
    TArray<FInventoryOperation> InventoryOps;
};
```

### `FPipelineContext`
```cpp
struct FPipelineContext {
    FWorldSnapshot World;
    FBiomeSnapshot Biome;
    FInventorySnapshot Inventory;
    FRngState RNG;
    FRealState WorkingState;
    FIntent CurrentIntent;
    int32 TickIndex;
};
```

### `FPipelineResult`
```cpp
struct FPipelineResult {
    FStateDelta Delta;
    FRealState FinalWorkingState;
    FDebugTrace Trace; // опционально
};
```

### `FCommandIR` (линейная инструкция)
```cpp
struct FCommandIR {
    EOpCode Op;
    FOperand A, B, C;
    int32 DeterministicOrderIndex;
};
```

### `FMemoryState` (перенесено из архивного Contract v1.1 §7 — единственная часть контракта, подтверждённая кодом)
Моделирует память искажения клетки. Соответствует `HerbalistCoreTypes.h`.
```cpp
struct FMemoryState {
    float AccumulatedDistortion;      // I-1: меняется только через DistortionVelocity, не скачком
    float StabilityMemory;
    float HistoryPurity;
    float DistortionVelocity;
    float TimeOfLastDistortionChange;
};
```
Не входит в `FMemoryState`: параметры инвентаря, координаты капищ, погода/сезоны, параметры сущностей, флаги квестов.

---

## План миграции (PR план)

Миграция выполняется последовательно, каждый PR не ломает компиляцию и может быть отменён.

### PR-0 — Foundation
- Создание директорий `Simulation`, `Snapshot`, `Delta`, `Command`, `Trace`.
- Добавление пустых заголовков, лог‑категории `LogHerbalistSimulation`.

### PR-1 — Snapshot Layer
- Реализация структур `FWorldSnapshot`, `FInventorySnapshot`, `FBiomeSnapshot`.
- Добавление методов `CaptureSnapshot()` к `GridWorldManager`, `InventoryComponent`, `BiomeGraphSubsystem` (без изменения логики).

### PR-2 — Delta Layer
- `FStateDelta`, `FInventoryOperation`.
- Метод `ApplyDelta()` в `GridWorldManager` (пока не используется).

### PR-3 — Command System
- `FCommand`, `FCommandGraph`, `ECommandType`, `CommandBus`.
- UI и PlayerController переведены на генерацию команд (старая логика пока дублируется).

### PR-4 — Pipeline V2
- `HerbalistPipelineV2`, `PipelineContext`, стадии (Morok, Zaryana, …) как чистые функции.
- Вход: Snapshot + CommandGraph, выход: Delta.

### PR-5 — Bridge
- Создание `LegacyPipelineAdapter`.
- `CommandBus` подключается к `PipelineV2`. Старый Pipeline и новый работают параллельно.

### PR-6 — World Apply Reduction
- `GridWorldManager` теряет логику алхимии, остаётся только `ApplyDelta()`.
- Вырезаются старые вызовы `ApplyAlchemyResult`.

### PR-7 — BiomeGraph Post-step
- `BiomeGraphSubsystem::Propagate(Delta)` и `CaptureBiomeSnapshot()`.
- Биомы обновляются после World Apply, не влияют на Pipeline напрямую.

### PR-8 — Perception Layer
- `PerceptionComponent`, функция `ComputePerceivedState()`.
- UI переключается на чтение `S_Perceived`, доступ к `S_real` закрыт.

### PR-9 — Legacy Removal
- Удаление старого Pipeline и связанных прямых зависимостей (после стабилизации PR-8).

### PR-10 — Trace & Replay
- `TraceRecorder`, `ReplayExecutor`, валидация.
- Полный replay любого тика, хеш‑контроль.

**Рекомендованный порядок выполнения:**  
`PR-0 → PR-1 → PR-2 → PR-3 → PR-4 → PR-5 → PR-6 → PR-7 → PR-8 → PR-9 → PR-10`

---

## Критерии готовности

Рефакторинг считается завершённым, когда:

- [ ] Pipeline не ссылается на UE‑рантайм (нет `GetWorld()`, `GetSubsystem()`, `LoadObject`).
- [ ] Весь мир замораживается в `Snapshot` до конца тика.
- [ ] Единственная точка мутации — `World::ApplyDelta()`.
- [ ] UI не имеет доступа к `S_real` и не вызывает игровые сервисы напрямую.
- [ ] `FStateDelta` полностью описывает результат тика.
- [ ] Включённая трассировка гарантирует идентичность при повторе.
- [ ] Новые механики добавляются только как комбинации примитивов `Q/T/D/S/B`.

---

## Фактический статус реализации

> Зафиксировано аудитом кода 2026-08-12 (сессия по согласованию архитектурных документов).
> Ниже — не план, а снимок того, что реально есть в `Source/ProjectHerbalist` на момент аудита,
> сверенный построчно с планом миграции выше. Обновлять при следующих значимых PR.

### Статус по PR

| PR | Заявлено | Факт |
|----|----------|------|
| PR-0 Foundation | Директории + `LogHerbalistSimulation` | ✅ Директории есть (`Core/Simulation/{Public,Private}`), лог-категория — `LogHerbalist` (общая, не отдельная `LogHerbalistSimulation`) |
| PR-1 Snapshot Layer | `FWorldSnapshot/FInventorySnapshot/FBiomeSnapshot`, `CaptureSnapshot()` без изменения логики | ⚠️ Структуры существуют, но не совпадают с документом: нет `TickIndex`, нет `FRngState GlobalRngState` — вместо этого `int32 WorldSeed`, который **не продвигается между тиками** (см. ниже) |
| PR-2 Delta Layer | `FStateDelta`, `ApplyDelta()` — «пока не используется» | ✅ Реализовано и **уже используется** (план недооценил свою же скорость) — `ApplyStateDelta()` в `GridWorldManagerCore.cpp` |
| PR-3 Command System | `FCommand/FCommandGraph/ECommandType/CommandBus`, дублирование старой логики на переходный период | ⚠️ Есть `FCommandEntry/FCommandGraph`, но `ECommandPrimitive` — плоский enum игровых действий (`Harvest, Transfer, Apply, Talk, Wait`), а не композируемые примитивы Q/T/D/S/B из раздела «Command Algebra». Отдельного `CommandBus` нет — его роль играет `AGridWorldManager::QueueCommand()`. Старая логика не дублируется, а прямо застаблена (см. PR-9) |
| PR-4 Pipeline V2 | `HerbalistPipelineV2`, стадии Morok/Zaryana/Fold/IntentResolver как чистые функции | ⚠️ `PipelineV2.cpp` существует и чист (не трогает UE-рантайм) — но внутри всего 3 обработчика команд (`ProcessHarvestCommand/ProcessTransferCommand/ProcessApplyCommand`), никаких отдельных стадий Morok/Zaryana/Fold/IntentResolver нет |
| PR-5 Bridge | `LegacyPipelineAdapter`, старый и новый Pipeline параллельно | ➖ Пропущен по факту — легаси-методы (`HarvestFromCell`, `HarvestTest`) не мостятся, а сразу превращены в no-op заглушки с `UE_LOG(Warning, "deprecated")` |
| PR-6 World Apply Reduction | `GridWorldManager` теряет алхимию, остаётся только `ApplyDelta()` | ⚠️ Вычисления действительно вынесены в Pipeline, но публичные методы `ApplyAlchemyResult(...)` в `GridWorldManagerAlchemy.cpp` остались как тонкие обёртки над `QueueCommand` — с неиспользуемым параметром `FRngState& Rng` (мёртвый код, вводит в заблуждение) |
| PR-7 BiomeGraph Post-step | `Propagate(Delta)` после World Apply, не влияет на Pipeline напрямую | ❌ Наоборот: `AGridWorldManager::Tick()` вызывает `BiomeGraphSubsystem::StepSimulation()` **до** сбора команд и выполнения Pipeline в этом же кадре, и `StepSimulation → ApplyFieldsToGrid → Grid->ApplyBiomeInfluences()` пишет прямо в `Cell.TargetState`, **в обход `FStateDelta`**. Это нарушает Single Writer (Causal Execution Spec, «только Delta изменяет World») |
| PR-8 Perception Layer | UI переключается на `S_Perceived`, доступ к `S_real` закрыт | ❌ `PerceptionComponent`/`FPerceptionService` посчитаны и кэшируются каждые 0.5с, но **ни один UI-виджет их не читает** — весь `UI/` работает через `InventoryComponent->GetItems()` (сырое `S_real`). `ComputePerceivedInventory()` вообще не искажает данные. Слой посчитан, но не подключён |
| PR-9 Legacy Removal | Удаление старого Pipeline | ⚠️ Вычисления удалены, но сигнатуры (`HarvestFromCell`, `HarvestTest`, `MassHarvestTest`) оставлены как мёртвые заглушки вместо полного удаления |
| PR-10 Trace & Replay | `TraceRecorder/ReplayExecutor`, хеш-контроль | ⚠️ `FTraceRingBuffer` и `ReplayAndCompare()` реализованы и реально работают, но сравнение идёт по избранным полям (`Magnitude`, `Meta.Distortion`), а не по хешу полного состояния, как заявлено в разделе Execution Trace & Replay Engine |

### Критические баги, найденные при аудите

1. ✅ **[ИСПРАВЛЕНО 2026-08-12] RNG был de-facto заморожен между тиками.** `AGridWorldManager::CaptureState()` брал `WorldRNG.GetCurrentSeed()` — а `WorldRNG` продвигается (мутирует внутренний `Seed`) только при спорадических вызовах `FRand()/RandRange()` во время генерации мира и восстановления ресурсов (`SpawnResourcesInCell`), а не каждый симуляционный тик. Между такими событиями `FWorldSnapshot.WorldSeed` не менялся, и `SnapshotService::ExecuteTick()` каждый тик создавал `FRandomStream` заново с одним и тем же сидом — джиттер в `GenerateHarvestResult`/`ComputeApplyResult` получался бит-в-бит одинаковым для всех Harvest/Apply-команд подряд, пока где-то рядом не восстановится ресурс.
   **Фикс:** `FWorldSnapshot` получил поле `TickIndex`; `CaptureState()` теперь выводит `WorldSeed = HashCombine(RngBaseSeed, CurrentTickID)` — детерминированно, уникально на каждый тик, не зависит от несвязанных систем (world-gen RNG остался только для генерации мира). `RngBaseSeed` — новый `UPROPERTY` на `AGridWorldManager` (по умолчанию 12345, редактируем в редакторе/BP). Replay не сломан: сид хранится в самом `FTraceFrame.WorldSnapshot`, а не пересчитывается заново. См. `GridWorldManagerCore.cpp: CaptureState()`.
   Полноценный перенос `FRngState GlobalRngState` внутри `FWorldSnapshot` (state carried forward, а не производный от TickIndex) по-прежнему не сделан — текущий фикс закрывает наблюдаемый баг вариативности, но не полностью соответствует буквальной формулировке доку. Оставлено на будущее, если понадобится RNG-состояние, не завязанное на номер тика.
2. ✅ **[ИСПРАВЛЕНО 2026-08-12] Pipeline был подвешен на `AActor::Tick()`** — количество «тиков симуляции» в секунду зависело от FPS, что противоречило Tick Execution Model.
   **Фикс:** `AGridWorldManager` получил `SimulationFixedTimeStep` (0.05с по умолчанию, настраивается) и аккумулятор `SimulationTimeAccumulator`. Тело одного тика вынесено в `RunSimulationStep()` и вызывается из `Tick()` в цикле `while (Accumulator >= FixedTimeStep)` — при просадке FPS ниже шага за один рендер-кадр отрабатывает несколько симуляционных тиков подряд, при высоком FPS — не чаще шага. `CurrentTickID` теперь считает именно симуляционные тики, а не рендер-кадры (что также усиливает фикс RNG выше — сиды по-настоящему привязаны к тикам симуляции). `RegenerateCellParameters` (непрерывная релаксация к `TargetState`) намеренно оставлена на каждом кадре с реальным `DeltaTime` — это не часть Pipeline, а continuous field, ей не нужен дискретный шаг. Отдельного класса `SimulationTickManager` не заведено — аккумулятор живёт прямо в `AGridWorldManager`, что проще и достаточно, пока Pipeline управляет только одним ГридWorldManager.
3. ✅ **[ИСПРАВЛЕНО 2026-08-12] BiomeGraph нарушал Single Writer** — писал в `Cell.TargetState` напрямую, минуя `FStateDelta` (см. PR-7 выше).
   **Фикс:** `FStateDelta` получил поле `TargetStateNudges` (мягкая правка цели релаксации — в отличие от `WorldChanges`, не трогает `Cell.State`, только `TargetState`). `AGridWorldManager::ApplyBiomeInfluences()` больше не мутирует `Cells` в цикле — строит `FStateDelta` и проводит его через `ApplyStateDelta()`, ту же единственную точку записи, что использует Pipeline. Математика и результат не изменились, изменился только путь записи.
   Заодно исправлена смежная находка математического аудита: `RecalculateFieldsFromGrid()` каждый шаг **перезаписывал** `MorokField/ZaryanaField` средним по гриду, стирая вклад соседей, добавленный `PropagateWaves` на предыдущем шаге, — поле не могло по-настоящему распространяться дальше одного узла за раз. Теперь используется `FMath::Lerp(Field, GridAverage, GridBlendFactor)` — новый параметр (по умолчанию 0.3, настраивается в `UBiomeGraphAsset`), поле сохраняет память между шагами.
4. ⚠️ **[ЧАСТИЧНО ИСПРАВЛЕНО 2026-08-12] UI обращался к геймплейным акторам напрямую.** `AlchemyTransferWidget.cpp` делал собственный `TActorIterator<AGridWorldManager>` внутри виджета. Заодно тот же паттерн нашёлся продублированным трижды в `HerbalistPlayerController.cpp` (`TestNewHarvest/TestNewTransfer/TestNewApply`), хотя в том же классе уже был кэширующий `FindWorldManager()`.
   **Фикс:** все четыре места переведены на существующий `AHerbalistPlayerController::FindWorldManager()` (кэш + один `TActorIterator` на весь PlayerController вместо поиска в каждом месте заново).
   Не сделано: `ViewModel`/`UICommandAdapters` слоя как такового по-прежнему нет — виджет всё ещё напрямую типизирован на `AGridWorldManager` и сам собирает `FCommandEntry`/вызывает `QueueCommand`. Устранён конкретный баг (лишний, не переиспользуемый поиск актора), но не сама архитектурная граница L3↔L0 из документа.

5. ✅ **[ИСПРАВЛЕНО 2026-08-12] Мёртвый параметр `FRngState& Rng` в `ApplyAlchemyResult`.** Обе перегрузки принимали `Rng`, но никогда его не использовали — вызывающий код (`ApplyPotionToCell`) даже честно вычислял содержательный сид из координат клетки и `Memory.AccumulatedDistortion`, который затем молча отбрасывался. Параметр удалён из обеих перегрузок и всех вызовов.

### GDD-расхождения, закрытые 2026-08-12

По итогам сверки всего GDD (`01`–`14`) с кодом обнаружился отдельный класс проблем: не архитектурные баги, а **отсутствующие механики**, которые GDD описывает подробно (местами — с точными формулами и константами), а `ComputeApplyResult`/`GenerateHarvestResult` их полностью игнорировали. Закрыто:

1. **Порядок ингредиентов не влиял на результат** — Fold взвешивал только по `Magnitude*Count`, без учёта позиции, хотя `05_Systems.md` называет это одним из ключевых принципов минимум в четырёх документах. Теперь вес i-го ингредиента = `FoldWeightDecay^i` (из `UHerbalistSettings`).
2. **Правила воды отсутствовали** — реализованы все 4 явных правила из `05_Systems.md`: без воды → зола (`EAlchemyOutcome::Ash`), только вода → варёная вода (`BoiledWater`), разбавление `Magnitude` пропорционально доле воды, штраф при доле воды выше `MaxWaterRatio` (0.8). Различение воды/не-воды — новое поле `FInventoryItem::bIsWater`, проставляется при харвесте, реестры внутри Pipeline не трогаются.
3. **Biome Context Injection отсутствовал** — заявленная в `05_Systems.md`/`14_Biome_Graph.md` функция `UBiomeGraphSubsystem::ResolveContext()` не существовала, `MorokAffinity`/`ZaryanaAffinity` были мёртвыми параметрами (использовались только в `DebugPrintNodes`). Теперь `FBiomeSnapshot` несёт `FBiomeFieldContext` (MorokField/ZaryanaField/Affinity/AxisDrift) на биом, `ComputeApplyResult` смещает оси по `AxisDrift` и масштабирует силу Morok/Zaryana на `Affinity`, используя веса `BiomeMorokInfluence`/`BiomeZaryanaInfluence`/`BiomeAxisDriftWeight` из `UHerbalistSettings`.
4. **Footprint Recording был мёртвым кодом** несмотря на явную пометку «✅ Реализовано» в `14_Biome_Graph.md`. `RecordFootprint()` не вызывался ниоткуда. Теперь `FStateDelta` несёт `Footprints` (Pipeline формирует их как чистые данные — MorokImpact/ZaryanaImpact/AxisDelta из результата Apply), а `AGridWorldManager::RunSimulationStep()` вызывает `UBiomeGraphSubsystem::RecordFootprint()` для каждой записи — вне Pipeline, там, где обращение к UObject-подсистеме допустимо.
5. **Harvest использовал не ту математику.** Существовал полноценный `UHarvestService::Harvest()` с отклонением от `FAlatyr::S0`, но он не вызывался нигде — реальный путь сбора (`GenerateHarvestResult`) просто джиттерил `Cell.State`. Теперь `GenerateHarvestResult` считает то же самое отклонение от S0 (`BiomeState - S0`, взвешенное `HarvestBiomeWeight`), но как чистая функция снапшота: базовые параметры ингредиента резолвятся один раз при харвест-акторе (`AHerbalistResourceActor::GetBaseState()`) и едут в `FHarvestCommand::BaseState`, не требуя обращения к `UIngredientRegistrySubsystem` внутри Pipeline. `UHarvestService::Harvest()` как класс не удалён (используется/может использоваться отдельно), но перестал быть единственно верной, но не вызываемой версией математики.
6. **Bifurcation (Collapse/Purification)** — реализована по `BiomeSnap.CollapseThreshold`: при достижении порога — жребий, взвешенный текущей `Stability` (выше `Stability` → вероятнее очищение, а не коллапс), с целевыми значениями из `05_Systems.md` (Purification: Distortion→0.4, Purity/Stability +0.2; Collapse: Distortion→0.2, Stability −0.3, Corruption +0.2, `EAlchemyOutcome::Catastrophe`).

**Ещё найдено, но не тронуто** — не баги, а отдельные фичи вне сегодняшнего скоупа: эволюция предметов в инвентаре со временем (`UHerbalistSettings::InventoryDecayRate` уже объявлен, но нигде не используется — «порча» ресурсов между сбором и варкой из `05_Systems.md` не реализована); `EnvironmentToxicityWeight`/`EnvironmentBlendWeight` (влияние `FEnvironment` клетки на алхимию — в GDD не описано достаточно детально, чтобы реализовать без домысливания); применение зелий к сущностям/произвольным объектам мира, а не только к клеткам (`05_Systems.md`: «игрок может применить любое зелье к любому объекту или существу» — сейчас только `ApplyPotionToCell`).

### Судьба Contract v1.1

Документ `Core/CoreLock/Herbalist System Contract v1_1.md` архивирован (см. пометку в самом файле). Его слои `SemanticResolver/IntentResolver/PhysicsPipeline/WorldStateApplier/WorldManifestor` и типы `FAlchemyAtom`/`AtomUID` не встречаются нигде в `Source/` — контракт описывал архитектуру, которая не была реализована и была молча заменена Pipeline V2. Единственная часть, подтверждённая кодом — `FMemoryState` — перенесена выше в «Критические структуры данных».

---

## Глоссарий

| Термин | Определение |
|--------|-------------|
| **S_real** | Реальное состояние мира (числовые параметры) |
| **S_perceived** | Искажённое отображение `S_real`, которое видит игрок |
| **Morok** | Искажение, вносимое миром в трансформацию и восприятие |
| **Zaryana** | Стабилизирующее начало, противоположное Morok |
| **Snapshot** | Полная копия состояния мира на начало тика |
| **Delta (FStateDelta)** | Описание изменений, которые нужно применить к миру |
| **Pipeline** | Чистая функция, преобразующая Snapshot+Commands в Delta |
| **Command Algebra** | Система из 5 примитивов для выражения любого действия игрока |
| **IR (Intermediate Representation)** | Линейная последовательность низкоуровневых операций для исполнения |
| **ETRE** | Execution Trace & Replay Engine |

---

Данный документ является **единственным источником архитектурной истины** для дальнейшей разработки. Любое расхождение с ним должно рассматриваться как баг.