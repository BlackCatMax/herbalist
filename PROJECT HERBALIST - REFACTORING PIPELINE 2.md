Ниже представлен **итоговый архитектурный документ**, который аккумулирует все результаты аудита, согласованную целевую модель, формальную алгебру команд, тиковую модель, спецификацию причинности, систему трассировки/повторов и пошаговый план миграции в виде независимых PR. Документ самодостаточен и предназначен для продолжения работы в новых сессиях.

---

# 🏛️ PROJECT HERBALIST — DETERMINISTIC SIMULATION ENGINE

## Финальный архитектурный документ и план рефакторинга

**Статус:** утверждён  
**Версия:** 1.0  
**Дата:** 2026-04-28

> **Актуальный рабочий план — в отдельном документе:**
> [`AUDIT_AND_REFACTORING_PLAN.md`](AUDIT_AND_REFACTORING_PLAN.md) (2026-08-23).
> Этот файл — летопись выполненных изменений; там — что делать дальше,
> с приоритетами, номерами строк и численным обоснованием правок формул.

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

### Целевая (из исходного дизайна, не реализована полностью)

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

Ни `Gameplay/`, ни `Core/Biome/` (вместо этого — `Core/BiomeGraph/`), ни подпапки `UI/{Widgets,Controllers,ViewModel}`, ни подпапки `Simulation/{Pipeline,Snapshot,Delta,Tick,Trace}` в реальности не заведены — `Core/Simulation/` плоско делится на `Private/`/`Public/`, `UI/` и `Player/` не сгруппированы под `Gameplay/`. Полный перенос под эту структуру не проводился — слишком большой blast radius (каждый физический перенос файла в UE требует правки всех `#include`, ссылающихся на старый путь, десятки мест), и отдельно не запрашивался.

### Фактическая (после точечной уборки 2026-08-12)

```
Source/ProjectHerbalist/
├── Core/
│   ├── BiomeGraph/                    (BiomeGraphAsset, BiomeGraphCommands, BiomeGraphSubsystem, BiomeGraphTypes)
│   ├── Config/                        (HerbalistSettings — перенесён из Core/, лежал вне подпапок)
│   ├── Data/                          (IngredientTableRow, WaterTypeRow)
│   ├── Harvest/                       (HarvestService)
│   ├── Inventory/                     (HerbalistInventoryComponent, InventoryDragDropOperation)
│   ├── Resources/                     (AHerbalistResourceActor)
│   ├── Simulation/
│   │   ├── Private/                   (PipelineV2, SnapshotService, TraceReplay, PerceptionComponent.cpp, PerceptionService)
│   │   └── Public/                    (CommandTypes, DeltaTypes, SnapshotTypes, TraceTypes, PerceivedTypes, PerceptionComponent.h, SnapshotService.h)
│   ├── Storage/                       (AlchemyTableActor, StorageContainer)
│   ├── Subsystems/                    (AlchemySubsystem, IngredientRegistrySubsystem, WaterTypeRegistrySubsystem)
│   ├── Types/                         (BiomeRow, BiomeTypes, HerbalistCoreMath, HerbalistCoreTypes, HerbalistIngredient, HerbalistNameUtils)
│   └── World/                         (GridWorldManager + GridWorldManager{Core,Alchemy,Harvest,Tick,Debug}.cpp)
├── HerbalistLogChannels.h/.cpp
├── Player/                            (HerbalistPlayerController)
├── ProjectHerbalist.h/.cpp, ProjectHerbalistGameModeBase.h/.cpp
└── UI/                                (AlchemySlotWidget, AlchemyTransferWidget, InventoryDragDropController,
                                         InventorySlotWidget, InventoryTransferWidget, InventoryWidget, ItemTooltipWidget)
```

Сделано точечно, без риска правки десятков `#include`:
- Удалён `Core/Simulation/Private/SimulationModule.cpp` — пустой файл (0 байт), забытая заглушка PR-0, ничего не содержал и никем не подключался.
- `Core/HerbalistSettings.h/.cpp` → `Core/Config/HerbalistSettings.h/.cpp` — раньше лежал прямо в `Core/` вне какой-либо подпапки; поправлены все 3 `#include` (`HarvestService.cpp`, `HerbalistInventoryComponent.cpp`, `PipelineV2.cpp`).
- `Core/CoreLock/Herbalist System Contract v1_1.md` → `herbalist_docs/Herbalist_Vault/03_Technical/Archive/` — markdown-документ убран из дерева C++ исходников в документацию, где ему и место; пустая `Core/CoreLock/` удалена.

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

- [x] Pipeline не ссылается на UE‑рантайм (нет `GetWorld()`, `GetSubsystem()`, `LoadObject`) — проверено `grep` по `PipelineV2.cpp` на дату 2026-08-12, совпадений нет; весь UE-рантайм-доступ (капчур/применение) вынесен в `SnapshotService`/`GridWorldManager`, как и задумано.
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
>
> **Таблица ниже переcверена с кодом в конце сессии 2026-08-12.** Первая её версия была снимком
> *начала* сессии и к концу дня противоречила разделам с фиксами ниже (описывала как «не сделано»
> то, что было исправлено несколькими часами позже). Строки PR-0/1/4/6/7/8 обновлены; каждая
> проверена `grep` по актуальному коду, а не по памяти.

### Статус по PR

| PR | Заявлено | Факт |
|----|----------|------|
| PR-0 Foundation | Директории + `LogHerbalistSimulation` | ✅ Директории есть (`Core/Simulation/{Public,Private}`); `LogHerbalistSimulation` заведена — `HerbalistLogChannels.h` объявляет 9 категорий по подсистемам (см. «Централизованное логирование» ниже) |
| PR-1 Snapshot Layer | `FWorldSnapshot/FInventorySnapshot/FBiomeSnapshot`, `CaptureSnapshot()` без изменения логики | ⚠️ Структуры существуют и расширены по ходу сессии: `TickIndex` и `WorldTime` добавлены, сид тика больше не заморожен, `FBiomeSnapshot` несёт `FBiomeFieldContext`. Отличие от документа осталось одно: `FRngState GlobalRngState` (состояние, переносимое между тиками) так и не заведён — вместо него `int32 WorldSeed`, производный от `(RngBaseSeed, TickIndex)` |
| PR-2 Delta Layer | `FStateDelta`, `ApplyDelta()` — «пока не используется» | ✅ Реализовано и **уже используется** (план недооценил свою же скорость) — `ApplyStateDelta()` в `GridWorldManagerCore.cpp` |
| PR-3 Command System | `FCommand/FCommandGraph/ECommandType/CommandBus`, дублирование старой логики на переходный период | ⚠️ Есть `FCommandEntry/FCommandGraph`, но `ECommandPrimitive` — плоский enum игровых действий (`Harvest, Transfer, Apply, Talk, Wait`), а не композируемые примитивы Q/T/D/S/B из раздела «Command Algebra». Отдельного `CommandBus` нет — его роль играет `AGridWorldManager::QueueCommand()`. Старая логика не дублируется, а прямо застаблена (см. PR-9) |
| PR-4 Pipeline V2 | `HerbalistPipelineV2`, стадии Morok/Zaryana/Fold/IntentResolver как чистые функции | ⚠️ `PipelineV2.cpp` существует и чист (не трогает UE-рантайм). Стадии появились по ходу сессии как чистые статические функции: `ApplyMorokAxisMix`, `ApplyZaryanaAxisMix`, `ComputeIntentCoherence`, Fold с затуханием по порядку — но живут внутри одного `.cpp` рядом с обработчиками команд, а не в отдельных файлах `Stages/`, как рисует структура папок |
| PR-5 Bridge | `LegacyPipelineAdapter`, старый и новый Pipeline параллельно | ➖ Пропущен по факту — легаси-методы (`HarvestFromCell`, `HarvestTest`) не мостятся, а сразу превращены в no-op заглушки с `UE_LOG(Warning, "deprecated")` |
| PR-6 World Apply Reduction | `GridWorldManager` теряет алхимию, остаётся только `ApplyDelta()` | ⚠️ Вычисления вынесены в Pipeline; мёртвый параметр `FRngState& Rng` из `ApplyAlchemyResult(...)` удалён. Сами методы остались как тонкие обёртки, собирающие `FCommandEntry(Apply)` и отправляющие в `QueueCommand` — это осознанно (точка входа для BP/Exec), но означает, что `GridWorldManager` не «потерял алхимию» полностью, как формулирует план |
| PR-7 BiomeGraph Post-step | `Propagate(Delta)` после World Apply, не влияет на Pipeline напрямую | ⚠️ Single Writer восстановлен: `ApplyBiomeInfluences()` больше не мутирует `Cells` напрямую, а собирает `Delta.TargetStateNudges` и проводит через `ApplyStateDelta()`. Осталось расхождение с планом по порядку: `BiomeGraphSubsystem::StepSimulation()` по-прежнему вызывается в начале `Tick()`, до фиксированного шага Pipeline, а не как post-step после World Apply |
| PR-8 Perception Layer | UI переключается на `S_Perceived`, доступ к `S_real` закрыт | ⚠️ Подключён частично: `ComputePerceivedInventory()` реально искажает (была заглушка-копия), `UPerceptionComponent` кэширует `FPerceivedInventory`, `InventorySlotWidget`/`ItemTooltipWidget` читают искажённую версию через `GridWorldManager::GetPerceivedInventory()`. Не закрыто: `AlchemySlotWidget`/`AlchemyTransferWidget` всё ещё берут `S_real` напрямую, `FPerceivedWorld` (искажение самой сетки) визуально нигде не потребляется — поэтому «доступ к `S_real` закрыт» ещё не выполнено |
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
3. **Biome Context Injection отсутствовал** — заявленная в `05_Systems.md`/`14_Biome_Graph.md` функция `UBiomeGraphSubsystem::ResolveContext()` не существовала, `MorokAffinity`/`ZaryanaAffinity` были мёртвыми параметрами (использовались только в `DebugPrintNodes`). Теперь `FBiomeSnapshot` несёт `FBiomeFieldContext` (MorokField/ZaryanaField/Affinity/AxisDrift) на биом, `ComputeApplyResult` смещает оси по `AxisDrift` и масштабирует силу Morok/Zaryana на `Affinity`, используя веса из `UHerbalistSettings` (на момент правки — `BiomeMorokInfluence`/`BiomeZaryanaInfluence`/`BiomeAxisDriftWeight`; позже в тот же день `BiomeMorokInfluence` был вытеснен `MorokMixStrengthFactor` при портировании легаси-формулы Morok и удалён — см. «Аудит связности + чистка мусора» ниже).
4. **Footprint Recording был мёртвым кодом** несмотря на явную пометку «✅ Реализовано» в `14_Biome_Graph.md`. `RecordFootprint()` не вызывался ниоткуда. Теперь `FStateDelta` несёт `Footprints` (Pipeline формирует их как чистые данные — MorokImpact/ZaryanaImpact/AxisDelta из результата Apply), а `AGridWorldManager::RunSimulationStep()` вызывает `UBiomeGraphSubsystem::RecordFootprint()` для каждой записи — вне Pipeline, там, где обращение к UObject-подсистеме допустимо.
5. **Harvest использовал не ту математику.** Существовал полноценный `UHarvestService::Harvest()` с отклонением от `FAlatyr::S0`, но он не вызывался нигде — реальный путь сбора (`GenerateHarvestResult`) просто джиттерил `Cell.State`. Теперь `GenerateHarvestResult` считает то же самое отклонение от S0 (`BiomeState - S0`, взвешенное `HarvestBiomeWeight`), но как чистая функция снапшота: базовые параметры ингредиента резолвятся один раз при харвест-акторе (`AHerbalistResourceActor::GetBaseState()`) и едут в `FHarvestCommand::BaseState`, не требуя обращения к `UIngredientRegistrySubsystem` внутри Pipeline. `UHarvestService::Harvest()` как класс не удалён (используется/может использоваться отдельно), но перестал быть единственно верной, но не вызываемой версией математики.
6. **Bifurcation (Collapse/Purification)** — реализована по `BiomeSnap.CollapseThreshold`: при достижении порога — жребий, взвешенный текущей `Stability` (выше `Stability` → вероятнее очищение, а не коллапс), с целевыми значениями из `05_Systems.md` (Purification: Distortion→0.4, Purity/Stability +0.2; Collapse: Distortion→0.2, Stability −0.3, Corruption +0.2, `EAlchemyOutcome::Catastrophe`).

**Поправка 2026-08-12 (аудит связности):** предыдущая версия этого абзаца ошибочно утверждала, что порча предметов в инвентаре не реализована. Это неверно — `UHerbalistInventoryComponent::TickComponent()`/`ApplyDecayToItem()` (тик каждые 0.2с) реально читает `Settings->InventoryDecayRate`, учитывает `IngredientTableRow::DecayRate` конкретного ингредиента и `Item.State.Meta.Stability`, увеличивает `Distortion`/`Corruption`, снижает `Purity`/`Stability`. Работает независимо от `CreationTime` (по тик-интервалу, не по `Now − CreationTime`), так что более ранний баг с `CreationTime≡0` на порчу не влиял.

**Ещё найдено, но не тронуто** — не баги, а отдельные фичи вне сегодняшнего скоупа: `EnvironmentToxicityWeight`/`EnvironmentBlendWeight`/`BifurcationThreshold` в `UHerbalistSettings` объявлены, но нигде не читаются — `BifurcationThreshold`, в частности, дублирует уже подключённый `BiomeGraphAsset::CollapseThreshold` другим значением, что может сбить с толку при следующей правке (какой из двух порогов настоящий); применение зелий к сущностям/произвольным объектам мира, а не только к клеткам (`05_Systems.md`: «игрок может применить любое зелье к любому объекту или существу» — сейчас только `ApplyPotionToCell`).

### Портирование легаси-математики варки (2026-08-12)

Пункты выше (Fold/вода/Morok/Zaryana/Bifurcation) были **переизобретены заново** по тексту GDD, без сверки с историей репозитория. Дальнейшая сверка через `git log --all -S"ApplyMorok"` показала, что настоящая реализация этой математики существовала в коде вплоть до коммита `1539015` («Alchemy Table Fixes») — целый кластер файлов `Core/Pipeline/{HerbalistPipeline,PipelineFold,PipelineWater,PipelineMorok,PipelineZaryana,PipelineMeta,IntentResolver,AlchemySemantics,AlchemyPhysicsPipeline,AlchemyWorldStateApplier}.cpp` — и была удалена при переходе на PipelineV2 без переноса. Три места, где сегодняшняя реализация оказалась заметно грубее оригинала, портированы точнее:

- **`ComputeIntentCoherence()`** (было: `Intent.Coherence` захардкожен `0.5f` в 4 местах вызова) — портирована из `IntentResolver.cpp`: вес ингредиента по позиции (`FoldWeightDecay^i`), согласие доминирующих осей (`AxisAgreement`), качество ингредиентов (`Purity+Stability`), бонус воды. Теперь Pipeline сам считает `Coherence` из `Cmd.Ingredients` — вызывающий код (`HerbalistPlayerController`, `AlchemyTransferWidget`, `GridWorldManagerAlchemy`) больше не проставляет её.
- **Morok** (было: случайная перестановка двух осей) — портирован из `PipelineMorok.cpp::ApplyMorokDistortion`: настоящее матричное смешивание 4 осей (`newBody=(1-mix)·B+k·M+mix·S...`) с масштабированием и клампом длины.
- **Zaryana** (было: усиление одной доминирующей оси) — портирован из `PipelineZaryana.cpp::ApplyZaryanaStructuring`: усиление осей выше среднего / подавление ниже среднего + мягкая `tanh`-нелинейность.
- Обе функции теперь работают через `DirectionToUnitVector`/`UnitVectorToDirection` (единичный 4D-вектор) вместо L1-симплекса — легаси делал то же самое через отдельный тип `FL2Direction`+`FRngState`; здесь обошлись без него, чтобы не заводить второй тип ГПСЧ рядом с `FRandomStream`.
- `ZaryanaStrength = Coherence · (1 − EffectiveMorok)` — легаси-формула из `HerbalistPipeline.cpp::ApplyMorok`, теперь осмысленная, поскольку `Coherence` реальный, а не константа.

Не портировано осознанно: **`BuildEnvironmentMeta()`** (`PipelineMeta.cpp`) существовала, но не вызывалась нигде даже в легаси-коде (`git grep` по `1539015` не находит вызовов) — это была мёртвая функция и до PipelineV2, портировать её означало бы придумывать точку интеграции с нуля, как и раньше. **Собственная точная формула Bifurcation** (легаси: `Distortion·0.3` в диапазоне `[0.1,0.4]` для Collapse, `Distortion·0.6` в `[0.3,0.5]` для Purification, триггер при `Distortion>0.92`) не портирована — сегодняшняя реализация (плоские целевые значения ≈0.2/≈0.4, триггер по `CollapseThreshold`) ближе к тексту `05_Systems.md`, чем легаси-код. **Delta-модель Apply** — легаси считал не абсолютное новое состояние клетки, а *дельту* к текущему (`NewDir = CurrentDir + DeltaDir`), что соответствует формализму GDD `S_real(t+1)=S_real(t)+ΔS`; сегодняшний `ComputeApplyResult` по-прежнему заменяет `Cell.State` целиком — это осталось как отдельный, не сегодняшний архитектурный долг.

### Морок по лору: убрано насыщение, вырезан Core/Harvest (2026-08-23)

Четыре правки по итогам аудита (`AUDIT_AND_REFACTORING_PLAN.md`). Сборка чистая,
автотесты 7/7.

**1-2. Frame-rate зависимость.** `GnilnikiNudgeRate` применялся целиком каждый кадр,
хотя задокументирован «в секунду» — на 60 FPS порча мира шла в 60 раз быстрее
заявленного и зависела от частоты кадров. `Memory.HistoryPurity` сходился тем
быстрее, чем выше FPS, — порог появления Берегини достигался на разных машинах за
разное время. Первое получило `* DeltaTime`, второе — `1 - Exp(-Rate * DeltaTime)`
(линейная форма при больших DeltaTime перелетает цель). Заодно `HistoryPurity`
считается только для Речной поймы: больше его никто не читает.

**3. Вырезан `Core/Harvest`** — 156 строк, не вызывались ниоткуда. Остаток
дореформенного пайплайна со старой S0-формулой сбора, вытесненной
`GenerateHarvestResult`. Ушёл вместе с `HarvestConditionWeight` (единственный
потребитель) и последней ссылкой на сломанный `S0` — теперь его можно чинить
начисто. `HarvestService` в `CollectWater` служил лишь null-guard'ом, блокировавшим
сбор воды при отсутствии сервиса.

**4. Убрано насыщение Морока — главное.**

Замеры показали: прежняя форма `D += Noise·(1−D)` работала тем слабее, чем грязнее
смесь. На злом составе (`D_fold = 0.8178`) весь диапазон влияния мира составлял
**0.072** — исход определялся ингредиентами вчетверо сильнее, чем местом, вопреки
Core Lock §2. Bifurcation не срабатывал **ни разу за 200 варок даже при
MorokAffinity = 1.0**, а с ним был мёртв весь зависимый контент: `RecordFootprint`,
«опасный полюс» легендарных сущностей, Стукачи-предвестники.

Морок и Заряна переписаны как одна операция в две стороны — возведение параметра
из [0,1] в степень:

```cpp
MorokExponent = EffectiveMorok * MorokPressure * (1 - Stability);
Distortion = Pow(Distortion, 1 / (1 + MorokExponent));      // Морок тянет к 1
Distortion = Pow(Distortion, 1 + ZaryanaStrength * 0.25);   // Заряна тянет к 0
```

Форма выбрана по лору, а не по удобству:
- ограничена [0,1] **по построению** — клампы убраны, как ранее в `GenerateHarvestResult`;
- 0 и 1 — неподвижные точки: абсолютно чистое неуязвимо для Морока (фольклорно —
  апотропейные травы), абсолютно искажённое не вытянуть Заряной;
- Морок **усиливает имеющееся, а не впрыскивает своё** — `03_Narrative`: «Морок не
  действует как отдельная сущность. Он проявляется через поведение системы, изменяя
  её структуру»;
- гасится Stability — `03_Narrative`: «нестабильность усиливается в нарушенной среде,
  она ослабевает в согласованных условиях».

`MorokMixStrengthFactor` → `MorokPressure` (0.5 → 1.0): прежнее имя врало, осевым
смешиванием этот коэффициент никогда не управлял.

Правка вышла хирургической — низ поля не тронут (чистое зелье 0.189 → 0.180), ожил
только верх. Один и тот же злой рецепт теперь даёт 148 Catastrophe + 52 Purified в
Болоте и Valid ×200 во всех остальных семи биомах.

**Оговорка:** тюнинг переехал в `MorokAffinity`, лежащий в бинарном `DA_BiomeGraph`.
Порог срабатывания — между 0.5 и 0.6, а дефолт кода равен 0.5. Структурно механика
ожила, но фактические значения ассета не читаются — поэтому вынос графа в JSON
(по образцу `extract_biomes.py`) повышен в приоритете.

### Автотесты реально запускаются + оживлён HarvestStress (2026-08-12)

Первый в истории проекта настоящий прогон автотестов (headless `UnrealEditor-Cmd -ExecCmds="Automation RunTests Herbalist"`). Дорога до него вскрыла три бага подряд, каждый невидимый для компиляции:

1. **`ExtraModuleNames` в Target.cs заставляет модуль собираться, но не загружаться.** Более ранняя правка «подключил тесты к сборке» была неполной: тесты компилировались, но никогда не регистрировались. Нужна запись в `Modules` внутри `.uproject`.
2. **Модулю нужен `IMPLEMENT_MODULE`.** Без него движок грузит DLL и не может инициализировать («could not be successfully initialized after it was loaded») — тесты снова не появляются. Добавлен `ProjectHerbalistTestsModule.cpp`.
3. **Переписанный `IngredientRegistryTest` падал в рантайме.** `UIngredientRegistrySubsystem` — это `UGameInstanceSubsystem` с `ClassWithin = UGameInstance`, поэтому `NewObject<>()` без Outer невалиден: ensure, а на третьем тесте краш всего прогона. Тесты переведены на хелпер, создающий временный `UGameInstance` как Outer.

**Итог: 7 из 7 зелёные.**

Первый же валидный прогон поймал настоящий баг геймплея: тест `PipelineV2.Harvest` ждал `HarvestStress == 0.1`, а код давал `0.001`. Прав оказался тест — `AGridWorldManager::HarvestStressIncrement = 0.1f` был объявлен, но **не использовался нигде**, в `ProcessHarvestCommand` стояла захардкоженная константа `0.001`, в точности равная тогдашнему спаду за секунду. То есть след одного сбора **стирался за одну секунду**, и механика «сбор истощает место» не работала в принципе.

### Стресс клетки: длительный и зависящий от биома (2026-08-12)

Переведён на игровое время вместо магических констант:
- `UHerbalistSettings::GameDayMinutes = 32` — длины игровых суток в проекте **не было вообще** (только неиспользуемый `ETimeOfDayMask`).
- `UHerbalistSettings::StressRecoveryGameDays = 7` — клетка со стрессом 1.0 зарастает за игровую неделю (было: за полсуток).
- `UHerbalistSettings::HarvestStressIncrement = 0.1` — переехал с `AGridWorldManager`, потому что нужен Pipeline'у, а тот до актора не достаёт.
- `FBiomeRow::StressRecoveryMultiplier` — на биом.

Ритм получается осмысленный: **один сбор метит клетку примерно на 0.7 игровых суток, десять сборов подряд забивают её на полную неделю.**

Множитель **выводится из уже написанного лора**, а не назначается вручную: скорость зарастания = `Fertility × (1 − Distortion·0.7)` (место затягивает рану жизненной силой, Навь заживать мешает), затем поправка на характер воды из авторского поля `type` компендиума — «стоячая» держит след (×1.35), «живая/ключевая/разливная/речная» промывает (×0.85), «солонцеватая/жёсткая» мёртвая вода не промывает (×1.10). Нормировано так, что средний биом зарастает ровно за базовые 7 суток.

| биом | множ. | зарастание | почему |
|---|---|---|---|
| Смешанный лес | 0.59 | 4.1 сут | живая вода промывает |
| Речная пойма | 0.64 | 4.5 сут | живая вода промывает |
| Широколиственный лес | 0.66 | 4.6 сут | высокое плодородие |
| Тайга | 0.67 | 4.7 сут | ключевая вода |
| Лесостепь | 0.80 | 5.6 сут | речная вода |
| Степь | 1.30 | 9.1 сут | мёртвая солонцеватая вода |
| Тундра | 1.64 | 11.5 сут | скудость: мерзлота, ягель растёт десятилетиями |
| **Болото** | **1.72** | **12.0 сут** | **стоячая вода + Навь (Distortion 0.70)** |

Болото вышло самым долгим **естественно**, из авторского `type: чёрная, стоячая`, а не подгонкой. Тундра рядом, но по другой причине — не Навь, а скудость: два «долгих» биома с разной природой.

`extract_biomes.py` поддерживает необязательное переопределение `stress_recovery_multiplier` во фронтматтере — там, где формула соврёт, автор ставит своё число.

### Формула сбора: интерполяция вместо отклонения от S0 (2026-08-12)

Трассировка пайплайна на реальных данных (desk-check формул с числами из `ingredients.json`/`DT_BiomeDefaults.json`) показала, что **44% замеров при сборе упирались в кламп**: `Potency` и `Resonance` были константой 1.0 во всех восьми биомах, `Stability` — почти всегда 0. То есть обещание GDD «одни и те же ресурсы дают разный результат в разных местах» по большинству осей не работало.

Причина оказалась двойной:
1. `FAlatyr::S0` инициализирует только 5 полей из 8 — `Potency`/`Resonance`/`Corruption` остаются нулями по умолчанию `FMeta`. Плюс `S0.Direction` нормализован по **L2** (сумма 2.0), а весь остальной пайплайн — по L1 (`NormalizeSum()`, сумма 1.0). Из-за рассогласования шкал телесная ось багульника схлопывалась с 0.087 до 0.003.
2. Главное: **сама форма формулы**. `Base + k·(Biome − S0)` не ограничена (диапазон `[−k, 1+k]`), и у трав с параметрами у края любой толчок биома в ту же сторону выбивал их за границу. Сдвиг `S0` внутрь диапазона снижал частоту клампа, но не устранял причину — на намеренно крайних ингредиентах кламп возвращался (4–7 замеров из 56).

**Решение:** `GenerateHarvestResult` переведён на `Lerp(BaseState, CellState, k)`. Результат всегда лежит между природой травы и состоянием места, оба из [0,1] — **выйти за границы невозможно по построению**, кламп не нужен (проверено: 0 из 168 замеров, включая крайние ингредиенты). Разброс при этом остался полным (0.236 против 0.097 у варианта с «мягким пределом» `x + Δ(1−x)`, который границы тоже гарантирует, но вдвое сужает вариативность, сопротивляясь у краёв).

Семантически это ближе к GDD (`05_Systems.md`): «ресурсы не существуют до момента сбора как фиксированные сущности — они формируются в момент взаимодействия как результат преобразования локального состояния». Это описание интерполяции, а не вычитания эталона.

`S0` из формулы сбора **ушёл совсем** — и это разрешает конфликт двух ролей, найденный ранее: он перестаёт быть началом координат (роль, в которой был непригоден — лежит вне диапазона реальных биомов по всем шести мета-осям) и освобождается для настоящей роли недостижимого ориентира в `Distance(S_real, S0)`, когда дойдут руки до прогрессии. Сейчас `FAlatyr::S0` не используется живым кодом вовсе (остался только в осиротевшем `UHarvestService`).

Сопутствующее:
- `HarvestBiomeWeight` понижен 0.6 → 0.4: при 0.6 место перебивало вид, и разные травы в одном биоме сходились друг к другу.
- Новое поле `IngredientTableRow::Resilience` (0..1) — сопротивляемость травы характеру места, гасит эффективный `k`. Фольклорно: сильные травы держат свою природу вопреки месту. Прокинуто `ResourceActor` → `FHarvestCommand::Resilience` → Pipeline, реестры внутри Pipeline по-прежнему не трогаются. По умолчанию 0.0 — то есть поведение всех существующих ингредиентов не меняется, пока значения не проставят в таблице.

### Биомы генерируются из компендиума (2026-08-12)

`extract_biomes.py` — по образцу существующего `extract_ingredients.py`: единственный источник истины для биомов теперь написанный лор (`04_Compendium/Биомы`), а `DT_BiomeDefaults.json` выводится из него. До этого таблица велась отдельно и разошлась с компендиумом по 35 значениям (сильнее всего Болото и Тайга).

Что скрипт **не** берёт из компендиума и сохраняет из прежнего JSON: `EntityActivityBase` (во фронтматтере отсутствует) и `DefaultWaterState` (в компендиуме есть только прозой в разделе «## Вода», причём неполно — без Magnitude/Stability/Potency/Resonance; разбирать прозу регуляркой ненадёжно).

**Требует решения автора:** скрипт нашёл **13 внутренних противоречий** в самом компендиуме — фронтматтер расходится с markdown-таблицами в теле того же документа (например, Болото: `corruption` 0.70 во фронтматтере против 0.65 в тексте). Скрипт берёт фронтматтер как машиночитаемый источник и **докладывает каждое расхождение, ничего не решая молча**. Пока эти 13 мест не выверены вручную, часть чисел в таблице — выбор скрипта, а не автора.

**Важно:** игра читает `Content/Data/DT_BiomeDefaults.uasset`, а не этот JSON. Чтобы правки дошли до игры, таблицу нужно переимпортировать в редакторе UE — этого никто пока не делал.

### Аудит связности + чистка мусора (2026-08-12)

Отдельный проход агента по всей кодовой базе на мёртвый код, рассинхрон именований и неиспользуемые include. Исправлено:
- Удалён мёртвый `FBiomeDefaults::GetRandomResourceForBiome` (0 вызовов, комментарий ссылался на несуществующий `FIngredientRegistry`).
- Убраны неиспользуемые `#include`: `LandscapeInfo.h` (`GridWorldManagerCore.cpp`), `Engine/AssetManager.h`/`Engine/StreamableManager.h`/`Core/Types/HerbalistIngredient.h` (`InventorySlotWidget.cpp`).
- Удалён осиротевший `UHerbalistSettings::BiomeMorokInfluence` — собственный побочный эффект сегодняшней сессии: был введён при первой версии `ComputeApplyResult`, заменён на `MorokMixStrengthFactor` при портировании легаси-формулы Morok, но само поле не убрали.
- Поправлен вводящий в заблуждение комментарий `GridWorldManager.h` — `ApplyAlchemyResult` помечался как «старая алхимия, будет заменена», хотя это тонкая обёртка над новым командным пайплайном.
- **Реальная находка, не просто чистка:** `EAlchemyOutcome` (Ash/BoiledWater/Catastrophe/Valid) считался в `ComputeApplyResult`, но при крафте `PotionItem.IngredientID` всегда жёстко ставился `"Potion"` независимо от `Outcome` — при этом UI (`AlchemySlotWidget`, `ItemTooltipWidget`) уже умел отображать `IngredientID == "Ash"`/`"BoiledWater"` отдельным именем («Зола»/«Кипячёная вода»), но эта ветка была недостижима: Pipeline никогда не создавал предмет с таким `IngredientID`. Теперь `IngredientID` при крафте берётся из `Outcome`. Заодно поправлен `AlchemyTransferWidget::CheckForNewPotion()` — она искала в инвентаре только `"Potion"` и не заметила бы вырожденный крафт (золу/кипячёную воду).

Найдено, но не тронуто (архитектурная развилка, не чистка): `UHerbalistSettings::BifurcationThreshold`/`EnvironmentToxicityWeight`/`EnvironmentBlendWeight` — нечитаемые поля, но представляют нереализованную фичу (Environment→Meta), а не мусор; удалять не стал. `AlchemySlotWidget`/`AlchemyTransferWidget` всё ещё показывают `S_real` вместо `S_perceived` — тот же паттерн, что чинили в `InventorySlotWidget`, но отдельно не запрашивали. `HarvestTest`/`MassHarvestTest` — живые Exec-команды за мёртвой заглушкой (PR-9), сознательно оставлены как есть.

### Perception подключён к инвентарной UI (2026-08-12)

Раньше `FPerceptionService::ComputePerceivedInventory()` ничего не искажала («Пока без искажений, просто копируем»), и `ComputePerceivedWorld()` добавляла фиксированный шум независимо от реального `Distortion` клетки. При этом единственная попытка показать искажение в UI жила прямо в `ItemTooltipWidget::SetItem()` — локальная функция `PerceiveValue()`, которая **показывала и искажённое, и реальное значение одновременно** (`"Сила: 0.62 (0.58)"`) — то есть полностью выдавала игроку ground truth рядом с иллюзией, подрывая саму идею GDD «игрок никогда не видит S_real».

Исправлено:
- `FPerceptionService::PerceiveRealState()` — общая формула для мира и инвентаря: амплитуда шума масштабируется собственным `Distortion` объекта (при Distortion≈0 почти не искажает, при высоком — заметно), а не фиксированный диапазон.
- `ComputePerceivedInventory()` реально искажает предметы (было — заглушка).
- `UPerceptionComponent` кэширует `FPerceivedInventory` тем же тиком (0.5с), что и `FPerceivedWorld`; `AGridWorldManager::GetPerceivedInventory()` — геттер по аналогии с `GetPerceivedWorld()`.
- `ItemTooltipWidget::SetItem()` больше не искажает сам и не показывает реальное значение — принимает уже искажённый предмет и просто отображает.
- `InventorySlotWidget` берёт искажённую версию предмета через `GridWorldManager::GetPerceivedInventory()` (по индексу того же слота в контейнере 0) для имени зелья (`GeneratePotionName`) и для тултипа; реальный `CachedItem` остаётся источником для drag/drop/split/transfer — эти операции обязаны работать с настоящими данными.

Не тронуто: `AlchemySlotWidget` (слоты котла) всё ещё показывает `GeneratePotionName(StoredItem.State)` по реальному состоянию — та же проблема, тот же паттерн исправления, отдельно не запрашивали. `FPerceivedWorld` (искажение самой сетки/клеток) по-прежнему ничем не потребляется визуально — сегодня подключили только инвентарь. `AHerbalistPlayerController::CurrentGlobalDistortion` теперь нигде не читается (только пишется) — раньше был источником для тултипа, теперь дублирует то, что даёт Perception per-item; не удалял, возможно используется в Blueprint HUD.

### Производительность SnapshotService (2026-08-12)

При 20Гц фиксированном шаге (см. выше) `FSnapshotService::ExecuteTick()` безусловно делал полный снапшот мира/инвентаря/биомов и применение (пустой) дельты **на каждый тик**, даже когда `PendingCommands` пуст — то есть почти всегда, пока игрок не собирает/не варит именно в этот момент. Плюс `CaptureWorld()` и `ApplyDeltaToWorld()` каждая заново искала `AGridWorldManager` через некэшированный `TActorIterator` — притом что весь вызов идёт из метода этого же актора. `PerceptionComponent` делал свой независимый такой же поиск на отдельном каждые-0.5с тике.

Исправлено:
- `ExecuteTick()` возвращает `FStateDelta()` сразу, если `Commands.Commands.Num() == 0` — пустой граф гарантированно даёт пустую дельту, снапшотить нечего.
- `SnapshotService` кэширует `AGridWorldManager*` (`TWeakObjectPtr`, самоинвалидируется при смене уровня — тот же паттерн, что уже в `HerbalistPlayerController`/`BiomeGraphSubsystem`) — `CaptureWorld()`/`ApplyDeltaToWorld()` больше не гоняют `TActorIterator` дважды за тик. `PerceptionComponent` получает этот выигрыш бесплатно, так как использует тот же `FSnapshotService::CaptureWorld()`.

Не тронуто: `PerceptionComponent` по-прежнему копирует весь грид каждые 0.5с для данных, которые никто не читает (Perception не подключён к UI — отдельный, ранее зафиксированный пункт); `RegenerateCellParameters` всё ещё идёт каждый рендер-кадр, не на фиксированном шаге. Ни то ни другое не масштабируется на кардинально больший грид без дополнительной переработки (dirty-флаги/частичные снапшоты) — сейчас это не critical при дефолтных 20×20 клетках.

### Ревью портирования + тесты (2026-08-12)

Независимое ревью портирования выше нашло два реальных дефекта, не пойманных сборкой сессии:

1. **`ProjectHerbalistTests` не был подключён ни к `.uproject`, ни к `ProjectHerbalistEditorTarget.ExtraModuleNames`** — модуль тестов ни разу не собирался за всю сессию, поэтому ошибка компиляции в `PipelineV2Test.cpp` (звал `ExecutePipeline` с 4 аргументами при актуальных 5 — `FBiomeSnapshot` не хватало) оставалась незамеченной. Подключён; заодно всплыл `IngredientRegistryTest.cpp`, ссылавшийся на `Core/Data/IngredientRegistry.h` — файл из того же удалённого легаси-кластера, что `ApplyMorok` (§ выше). Переписан на актуальный `UIngredientRegistrySubsystem` (тот же состав проверок: `Classify`/`IsKnown`), тест `FAlchemyAtom` убран — эквивалента этого типа сегодня не существует. Заодно поправлено неверное ожидание `FPipelineV2HarvestWaterTest` (`InventoryOps.Num() == 0`, хотя вода добавляет одну операцию — так было и до, и после сегодняшних правок).
2. **`FInventoryItem::CreationTime` был захардкожен `0.0`** в трёх местах создания предмета (харвест, вода, зелье) — из-за чего `AlchemyTransferWidget::CheckForNewPotion()` (сравнивает `Item.CreationTime >= LastCraftTime - 0.1f`, где `LastCraftTime` — реальное игровое время) практически никогда не находил свежесваренное зелье. `FWorldSnapshot` получил поле `WorldTime` (из `GetWorld()->GetTimeSeconds()`, капчурится в `AGridWorldManager::CaptureState()`), Pipeline проставляет его в `CreationTime` вместо константы.

### Централизованное логирование (2026-08-12)

Раньше в проекте была одна категория на всё — `LogHerbalist` (91 вызов в 19 файлах) плюс самодельный локальный `LogBiomeGraph`. Даже обещанная в PR-0 этого документа `LogHerbalistSimulation` так и не была заведена. Теперь: `Source/ProjectHerbalist/HerbalistLogChannels.h/.cpp` объявляет по категории на подсистему — `LogHerbalistSimulation` (Pipeline/Snapshot/Delta/Trace), `LogHerbalistBiome` (BiomeGraph, заменил `LogBiomeGraph`), `LogHerbalistHarvest`, `LogHerbalistAlchemy`, `LogHerbalistInventory`, `LogHerbalistWorld` (GridWorldManager), `LogHerbalistUI`, `LogHerbalistPlayer`, `LogHerbalistData` (реестры/DataTable). `LogHerbalist` оставлен как общая категория для модуля/GameMode. Все 91 существующих вызова `UE_LOG` переведены на новые категории — теперь можно фильтровать Output Log по подсистеме (`log LogHerbalistBiome Verbose` и т.д.) вместо разбора одного общего потока.

### Судьба Contract v1.1

Документ [`Herbalist System Contract v1_1.md`](herbalist_docs/Herbalist_Vault/03_Technical/Archive/Herbalist%20System%20Contract%20v1_1.md) архивирован и вынесен из `Source/` в документацию (см. пометку в самом файле; раньше жил в `Core/CoreLock/` — эта директория удалена как пустая).

**Уточнение 2026-08-12:** изначальная формулировка здесь («архитектура не была реализована») была неточной. `git log --all -S"ApplyMorok"` показал, что слои `SemanticResolver`/`IntentResolver`/`PhysicsPipeline`/`WorldStateApplier` **были реализованы** (`Core/Pipeline/*.cpp`, вплоть до коммита `1539015`) — контракт описывал реальный, рабочий код, а не аспирацию. Просто при переходе на Pipeline V2 этот код удалили целиком, не портировав содержательную математику (Fold с затуханием, правила воды, Morok/Zaryana, `ComputeIntentCoherence`) в новый пайплайн — отсюда и не встречаются эти типы в текущем `Source/`. Часть этой математики портирована обратно точнее сегодня же (см. «Портирование легаси-математики варки» выше). `FMemoryState` (§7 контракта) — перенесена в «Критические структуры данных» ранее.

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