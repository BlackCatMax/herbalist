---
tags: [technical, current, core]
version: 2.0
based_on: ProjectHerbalist source, 2026-09-06
---
# Core — реализованная механика

Полностью переписано 2026-09-06 (`based_on: prototype_2026-04` устарел —
не упоминал детерминированный пайплайн/`FStateDelta`/`TraceReplay` вовсе,
которые сейчас и есть ядро "Core"; несколько структур ниже за это время
поменяли поля/значения). История решений, зафиксированных ниже как факт —
`CHANGELOG.md`.

## Структуры данных

### `FRealState`

    struct FRealState {
        float Magnitude;
        FDirection Direction;
        FMeta Meta;
    };

### `FDirection`

    struct FDirection {
        float Body, Mind, Spirit, Nature;
        void NormalizeSum(); // Body+Mind+Spirit+Nature → 1 (L1)
    };

Существует и второй, L2-нормализованный вариант (`FL2Direction`,
`ToL2()`/`NormalizeL2()`, `HerbalistCoreTypes.h`) — не путь по умолчанию,
частный случай для мест, которым нужна именно L2-метрика. Весь пайплайн
живёт на L1 (`NormalizeSum()`).

### `FMeta`

    struct FMeta {
        float Distortion, Stability, Purity, Potency, Resonance, Corruption;
    };

### `FEnvironment`

    struct FEnvironment {
        float Toxicity, Fertility, Moisture;
    };

### `FMemoryState`

    struct FMemoryState {
        float AccumulatedDistortion, StabilityMemory, HistoryPurity;
        float DistortionVelocity, TimeOfLastDistortionChange;
        bool bDegrading;
    };

`bDegrading` — гистерезис бистабильной релаксации клетки: `Corruption`
прошёл порог входа, цель релаксации сама сдвинута к испорченному полюсу,
пассивное восстановление не работает, пока клетку не вычистят активно
(`RegenerateCellParameters`, `GridWorldManagerCore.cpp`). Не в прототипе
2026-04 — добавлено вместе со всей бистабильной механикой позже.

### `FIntent`

    struct FIntent {
        float Coherence;
    };

Не константа — `ComputeIntentCoherence` (`PipelineV2.cpp`) считает её из
фактических ингредиентов (вес по позиции, согласие доминирующих осей,
качество, бонус воды) на каждый крафт. То, что вызывающий код кладёт в
`Cmd.Apply.Intent.Coherence`, `ProcessApplyCommand` безусловно
перезаписывает своим значением — реального потребителя этого поля со
стороны вызывающего кода нет (`Herbalist.PipelineV2.ApplyIgnoresCallerCoherence`
проверяет это явно). `UAlchemyTransferWidget` вызывает `ComputeIntentCoherence`
напрямую для превью до фактической варки.

## Нормализация направления

Используется `NormalizeSum()`:

1. Обнуление отрицательных значений.
2. Деление каждого компонента на сумму четырёх.
3. Если сумма ≈ 0 → все компоненты = 0.25.

## Эталонное состояние `S₀` ([[S0|Алатырь]])

    const FRealState FAlatyr::S0 = []{
        FRealState S;
        S.Magnitude = 1.0f;
        S.Direction = {0.25f, 0.25f, 0.25f, 0.25f};   // L1-нормализовано из {1,1,1,1}
        S.Meta.Distortion = 0.0f;
        S.Meta.Stability  = 1.0f;
        S.Meta.Purity     = 1.0f;
        S.Meta.Potency    = 1.0f;
        S.Meta.Resonance  = 1.0f;
        S.Meta.Corruption = 0.0f;
        return S;
    }();

Пересчитано (было `Magnitude=0.5`, `Direction={0.5,0.5,0.5,0.5}`, только
`Stability`/`Purity`=1, остальной `Meta`=0 — старые L2-эры числа). Текущий
S0 — предельно согласованное, предельно полное состояние (граничные
значения идеала по каждой полярной оси), не усреднённая точка.

**Роль изменилась**: S0 больше **не** используется как база расчёта
отклонений при сборе (`Harvest`) — вынесен из формулы намеренно, потому
что плохо работал началом координат (лежит вне диапазона реальных биомов
по всем шести Meta-осям). Реальная роль сегодня — недостижимый ориентир
прогрессии: `HerbalistCore::Math::Distance(State, FAlatyr::S0)` (не
нормализовано в [0,1]) используется в `GetSelectedCellInfo`
(`GridWorldManagerDebug.cpp`) и метрике мира Заряны/Буяна. Сбор берёт базу
из `IngredientTableRow::BaseState`/биома клетки (`GenerateHarvestResult`,
`PipelineV2.cpp`), не из S0.

## Детерминированная симуляция (не было в prototype_2026-04)

Ядро игры — чистая функция `Simulation::ExecutePipeline` (`PipelineV2.cpp`,
объявление в `Core/Simulation/Public/PipelineV2.h`):

    FStateDelta ExecutePipeline(
        const FWorldSnapshot& WorldSnapshot,
        const FInventorySnapshot& InventorySnapshot,
        const FBiomeSnapshot& BiomeSnapshot,
        const FCommandBatch& Commands,
        FRandomStream& Rng);

Не трогает `GameInstance`/`AGridWorldManager` напрямую — читает только
переданные снапшоты, пишет только в возвращаемую дельту. Это же и делает
её тестируемой в Editor-мире автотестов без живого мира.

**Команды** (`CommandTypes.h`) — один тик несёт `FCommandBatch` (плоский
`TArray<FCommandEntry>`, переименован из `FCommandGraph` 2026-08-30: имя
обещало граф зависимостей, реализация всегда была плоским списком,
исполняемым по порядку добавления). Шесть примитивов
(`ECommandPrimitive`): `Query`/`Transfer`/`Apply`/`Harvest`/`Wait`/`Talk` —
`Query` и `Talk` не производят дельту вовсе (диалог читает `Molva`/`Respect`
напрямую, не через пайплайн).

**Дельта** (`DeltaTypes.h`) — единственный канал, которым `ExecutePipeline`
сообщает об изменениях:

    struct FStateDelta {
        TMap<FIntPoint, FGridCell> WorldChanges;       // прямая перезапись клетки
        TArray<FInventoryOperation> InventoryOps;      // Add/Remove/Transfer
        TArray<FName> BiomeActivations;
        TMap<FIntPoint, FRealState> TargetStateNudges; // мягкая цель для RegenerateCellParameters
        TArray<FBiomeFootprintEntry> Footprints;       // след в биом-графе (05_Systems.md)
    };

`WorldChanges` и `TargetStateNudges` идут через один `ApplyStateDelta()`
(`AGridWorldManager`) — единственную точку записи в игровой мир
(**Single-Writer**). Известное задокументированное исключение —
`SeedRosaCorruptedCircle` (`GridWorldManagerZaryana.cpp`) пишет в
`Cell.State` напрямую при первом размещении Заряны: ни один существующий
канал `FStateDelta` не даёт "поднять `State`, не трогая `TargetState`", а
новый канал ради одного редкого разового события сочтён непропорциональным.

**Снапшоты** (`SnapshotTypes.h`) — `FWorldSnapshot` (сетка клеток + сид +
время + капища), `FInventorySnapshot` (контейнеры по ID), `FBiomeSnapshot`
(поля/аффинити/дрейф биом-графа, `CollapseThreshold`) — замороженные,
несериализуемые срезы состояния на момент тика, готовятся вызывающей
стороной (`AGridWorldManager::CaptureState`) до вызова `ExecutePipeline`.

**Верификация детерминизма** — `Simulation::ReplayAndCompare(const FTraceFrame&,
FRandomStream&)` (`TraceReplay.h/.cpp`) повторяет записанный кадр
трассировки на том же сиде и сравнивает получившуюся `FStateDelta` со
всеми пятью полями (`WorldChanges`/`InventoryOps`/`BiomeActivations`/
`TargetStateNudges`/`Footprints`) — не только по количеству записей, но и
по значению (`Magnitude`/`Distortion` и т.д.), см. `TraceReplayTest.cpp`.

## Рандомизация

Реальный источник случайности везде в геймплее — **`FRandomStream`**
(движковый тип UE), не самодельный ЛКГ: `AGridWorldManager::WorldRNG`
инициализируется `RngBaseSeed` в `BeginPlay()`, тот же `Rng` передаётся в
`ExecutePipeline`/`ReplayAndCompare` явным параметром — тем же сидом
детерминированно повторяется весь тик (это и проверяет `TraceReplay`
выше). Презентационный/невлияющий на симуляцию шум (визуальный джиттер
сущностей, восприятие в тултипах) намеренно использует **отдельные**
локальные `FRandomStream`, не `WorldRNG` — общий сид не должен двигаться
от того, что рисуется на экране.

Отдельно существует `FRngState` (`HerbalistCoreTypes.h`) — старый
линейный конгруэнтный генератор (`Seed = Seed*196314165 + 907633515`),
переживший с прототипа. Единственный оставшийся потребитель —
`FL2Direction::NormalizeL2()` (случайное направление при вырожденном
нулевом векторе), не общая RNG-инфраструктура проекта.
