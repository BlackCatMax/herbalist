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
13. [Глоссарий](#глоссарий)

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