# Проект Herbalist: Итоговая архитектура (полная спецификация)

## Введение

Документ объединяет все архитектурные решения, принятые в ходе рефакторинга. Он включает как первоначальные цели (устранение God Object’ов, событийная архитектура, детерминизм), так и последующие уточнения:

- **Единое центральное хранилище состояния** – `UWorldStateSubsystem` (а не `AGridWorldManager` как владелец).  
- **Разделение труда**: PCG управляет визуальным миром (ресурсы, вода), грид – только параметрами и логикой.  
- **Поля Морока и Заряны** – диффузия по клеткам (вместо графа биомов), визуализация сферами (чёрный/белый).  
- **Сбор воды** – трейс по тегу `WaterSurface`, параметры от биома клетки.  
- **Алхимия в доме** – без влияния биома места варки.  
- **Единое окно записи** – `ApplyDelta()` только через `WorldApplyService`.  
- **Масштаб мира** – 5×5 км, сетка до 2500×2500 клеток (до ~6 млн ячеек).  
- **World Partition** только для визуального стриминга (без мультиплеера).  
- **Оптимизация памяти** – легковесные структуры для пустых клеток, чанковая диффузия, параллельные вычисления.

Документ служит **единственным источником истины** для реализации и тестирования.

---

## 1. Основные принципы

1. **Единственный владелец состояния мира** – `UWorldStateSubsystem` (World Subsystem). Он никогда не выгружается и не зависит от World Partition.  
2. **Единственное окно записи** – `ApplyDelta()`. Никто, кроме `WorldApplyService` и `InventoryApplyService`, не может изменять состояние.  
3. **Разделение визуального и логического**:  
   - **PCG + World Partition** отвечают за визуальные акторы (ресурсы, декорации, водные поверхности).  
   - **State Subsystem** хранит параметры клеток (`S_real`, поля Морока/Заряны, тип ресурса, стадию роста).  
4. **Детерминизм и воспроизводимость** – чистые функции пайплайна, полные снапшоты с RNG-состоянием, replay с сохранением координат трейса.  
5. **Восприятие (`S_perceived`)** полностью отделено от реальности (`S_real`) через `PerceptionBuilder`.  
6. **UI на MVVM** – виджеты привязаны к ViewModel, которые подписаны на факты и отображают только искажённые данные.  
7. **Производительность** – оптимизация памяти, параллельная диффузия, асинхронные снапшоты по дельтам, отключение отладочной визуализации в Shipping.

---

## 2. Слои и компоненты (итоговая карта)

| Слой | Компоненты | Ответственность | Стриминг / Жизненный цикл |
|------|------------|------------------|----------------------------|
| **Simulation (ядро)** | `UWorldStateSubsystem`, `CommandBus`, `SimulationOrchestrator`, `PipelineExecutor`, `WorldApplyService`, `InventoryApplyService`, `FieldPropagationService`, `PerceptionBuilder`, `TraceRecorder` | Вся игровая логика, состояния, дельты, диффузия, восприятие, replay. | Постоянно в памяти (World Subsystem). Не зависит от WP. |
| **Domain** | `UHerbalistRuleSet` (DataAsset) | Правила алхимии (вода, конфликты осей, бифуркация). | Статический ассет. |
| **Validation** | `FStateValidationGate` | Проверка дельт на инварианты (NaN, отрицательные остатки, выход за [0,1]). | Без состояния. |
| **Application (UI)** | `InventoryViewModel`, `AlchemyViewModel`, другие ViewModel | Подписка на факты, преобразование `S_perceived` в данные для UI, отправка интентов. | Живут с виджетами, отписываются при дестрое. |
| **Infrastructure** | `UHerbalistAssetCatalog`, `UEventOutbox`, `UCheckpointingSubsystem`, `UPCGBridge` | Статические данные, события, сохранения, связь с PCG. | `UPCGBridge` подписан на события WP. |
| **Presentation (визуальный мир)** | Визуальные акторы ресурсов, декорации, водные поверхности (PCG + WP) | Отображение состояния мира, синхронизация через `BeginPlay()`. | Управляется World Partition (стриминг, HLOD). Выгружаются без потери данных. |

---

## 3. Центральное хранилище: `UWorldStateSubsystem`

Используется вместо `AGridWorldManager` для хранения всех данных мира. Это `UWorldSubsystem`, поэтому он автоматически создаётся и уничтожается с уровнем, но **не выгружается** World Partition.

### 3.1. Хранимые данные

```cpp
UCLASS()
class UWorldStateSubsystem : public UWorldSubsystem
{
    // --- размеры сетки ---
    int32 GridSizeX, GridSizeY;
    float CellSize; // в см

    // --- состояние клеток (оптимизированное) ---
    TArray<FGridCellLight> Cells;      // компактный массив
    // или для простоты: TArray<FGridCellFull> CellsFull (80 байт/клетку)

    // --- поля Морока/Заряны (плоские массивы) ---
    TArray<float> MorokField;          // размер = GridSizeX * GridSizeY
    TArray<float> ZaryanaField;

    // --- биомная маска (текстура или плоский массив весов) ---
    UTexture2D* BiomeMaskTexture;

    // --- RNG (детерминированный) ---
    FRandomStream DeterministicRng;

    // --- публичные методы ---
    const FGridCellLight& GetCell(int32 X, int32 Y) const;
    float GetMorokField(int32 X, int32 Y) const;
    float GetZaryanaField(int32 X, int32 Y) const;
    void ApplyDelta(const FWorldDelta& Delta, FValidationReport& OutReport);
    void ApplyInventoryDelta(const FInventoryDelta& Delta);
    void SaveSnapshot(FSimulationSnapshot& OutSnapshot) const;
    void LoadSnapshot(const FSimulationSnapshot& Snapshot);
};
```

### 3.2. Оптимизация памяти для 5×5 км

| Размер клетки | Сетка | Клеток | Память (полная структура 80 Б) | Память (легковесная 8 Б + хеш) |
|---------------|-------|--------|-------------------------------|--------------------------------|
| 4 м (рекомендуется) | 1250×1250 | 1.56 млн | ~125 МБ | ~12.5 МБ + 50 МБ (расширенные) |
| 2 м (для особых зон) | 2500×2500 | 6.25 млн | ~500 МБ | ~50 МБ + 200 МБ (расширенные) |

Рекомендуется использовать **лёгкую структуру** `FGridCellLight` (8 байт) для пустых клеток и хеш-таблицу для занятых. Для первых итераций можно использовать полную структуру – 125 МБ на 1.56 млн клеток приемлемо.

### 3.3. Структуры клеток

```cpp
// Лёгкая (всегда в массиве)
struct FGridCellLight
{
    uint16 ResourceID;      // 0 = пусто
    uint8 GrowthStage : 3;  // 0..7
    uint8 bAccessible : 1;
    uint8 Reserved : 4;
};

// Полная (для занятых клеток, хранится отдельно)
struct alignas(32) FGridCellFull
{
    FVector4 Direction;   // 16 байт
    float Magnitude;      // 4
    float Distortion;     // 4
    uint16 ResourceID;
    uint8 GrowthStage : 3;
    uint8 bAccessible : 1;
    uint8 Padding[2];
};
```

---

## 4. Поля Морока и Заряны (диффузия)

### 4.1. `UFieldPropagationService` (World Subsystem)

- Хранит те же массивы `MorokField` / `ZaryanaField` (или использует массивы из `UWorldStateSubsystem`).  
- Метод `Step(float DeltaSeconds)` вызывается из `SimulationOrchestrator` не чаще `PropagationInterval` (по умолчанию 1 сек).  
- Алгоритм для всей сетки (O(N)) с использованием `ParallelFor` и двойного буфера:

```cpp
void Step(float DeltaTime)
{
    // Для каждой клетки (x,y):
    //   inflow_morok = Σ (MorokField[nb] * DiffRate * Weight)
    //   morok_natural = GetMorokNaturalFromBiome(x,y)
    //   MorokField_new = MorokField_old + inflow_morok + morok_natural - MorokField_old * DecayRate * DeltaTime
    //   клиппинг [0, MaxFieldValue]
}
```

- **Визуализация** (отладка): при `bEnableFieldVisualization` обновляет `UInstancedStaticMeshComponent` (сферы). Цвет от чёрного (0) до белого (MaxFieldValue).

### 4.2. Консольные команды

| Команда | Эффект |
|---------|--------|
| `viz.morok` | Включить сферы Морока |
| `viz.zaryana` | Включить сферы Заряны |
| `viz.both` | Двухцветные сферы (левое полушарие – Морок, правое – Заряна) |
| `viz.step N` | Показывать каждую N-ю клетку |
| `viz.off` | Выключить визуализацию |

В `Shipping` конфигурациях код визуализации не компилируется.

---

## 5. PCG и визуальный мир (World Partition)

### 5.1. Принцип «симуляция решает, PCG рисует»

- `UWorldStateSubsystem` хранит для каждой клетки `ResourceID` (логический тип) и `GrowthStage`.  
- PCG-граф при генерации уровня (или динамически) считывает эти данные и инстансит правильную статическую модель через `Hierarchical Instanced Static Meshes` (HISM).  
- При изменении `ResourceID` или `GrowthStage` публикуется факт `FactResourceChanged`. `UPCGBridge` обновляет визуальный актор (заменяет модель или пересоздаёт инстанс).

### 5.2. Акторы ресурсов (без состояния)

- У каждого визуального актора есть только `CellCoord`.  
- В `BeginPlay()` он запрашивает у `UWorldStateSubsystem` актуальный `ResourceID`. Если 0 – самоуничтожается или скрывается.  
- При клике отправляется `IntentHarvestResource` с `CellCoord`. После применения дельты публикуется факт, и актор может скрыться.

### 5.3. Водные поверхности

- PCG генерирует меши/сплайны с тегом `WaterSurface`.  
- Сбор воды – отдельная клавиша, трейс из камеры, попадание по тегу → `IntentCollectWater` с координатами.  
- `PipelineExecutor::ExecuteCollectWater` определяет клетку, получает биом и поля, вычисляет параметры воды.

### 5.4. World Partition только для визуала

- Все визуальные акторы имеют `IsSpatiallyLoaded = true`.  
- `UWorldStateSubsystem` **не выгружается** и не зависит от WP.  
- При выгрузке ячейки WP визуальные акторы уничтожаются, данные клеток остаются.  
- При повторной загрузке акторы спавнятся заново и синхронизируются через `BeginPlay()`.

---

## 6. Алхимия (крафт зелий)

- Всегда на базе игрока (дом). Не зависит от биома места варки.  
- Использует только свойства ингредиентов (уже несут отпечаток биома сбора) и правила воды.  
- Стадия `InjectBiomeContext` полностью удалена из пайплайна.  
- `FPipelineExecutor::ExecuteCraft()` получает список ингредиентов, порядок, количество воды → возвращает `FStateDelta` (изменение инвентаря: удалить ингредиенты, добавить зелье).

---

## 7. Снапшоты, трассировка и детерминизм

### 7.1. Полный снапшот `FSimulationSnapshot`

Содержит:
- `FWorldSnapshot`: копии `ResourceID`, `GrowthStage`, `FRealState` для занятых клеток, а также массивы `MorokField` и `ZaryanaField`.  
- `FInventorySnapshot`: список предметов игрока.  
- `FRngState`: состояние детерминированного генератора.  
- Хеш состояния (CRC64 или CityHash).

### 7.2. Replay

- Каждый тик симуляции записывается `FTraceFrame`:
  - Снапшот **до** выполнения команд.
  - Список команд (включая `IntentCollectWater` с `HitLocation`).
  - Полученную `FStateDelta`.
  - Хеш состояния после применения.
- Кольцевой буфер на последние 128 кадров.
- При воспроизведении используется сохранённая точка попадания для воды (повторный трейс не выполняется).

### 7.3. Запреты для детерминизма

- Запрещено использовать недетерминированные функции (`FMath::Rand()`, GUID) вне `PipelineExecutor`.  
- Запрещена прямая мутация `UWorldStateSubsystem` из любого места, кроме `WorldApplyService` / `InventoryApplyService`.  
- Запрещено чтение реальных данных (`S_real`) в UI – только `S_perceived`.

---

## 8. Жизненный цикл симуляции (один кадр)

Вызывается из `AGridWorldManager::Tick()` (или из `UWorld` Tick):

```cpp
void USimulationOrchestrator::ExecuteFrame(float DeltaTime)
{
    // 1. Атомарный захват снапшота
    FSimulationSnapshot Snapshot;
    StateSubsystem->SaveSnapshot(Snapshot);
    InventoryService->SaveSnapshot(Snapshot);
    FRngState RngState = DeterministicRng.GetState();

    // 2. Получить команды из CommandBus
    TArray<FCommandEntry> Commands = CommandBus->DequeueAll();

    // 3. Выполнить пайплайн (чистая функция)
    FStateDelta Delta = PipelineExecutor->Execute(Snapshot, Commands);

    // 4. Валидация дельты
    if (!ValidationGate.Validate(Delta, OutReport)) return;

    // 5. Применить дельту (единственное окно записи)
    WorldApplyService->ApplyDelta(Delta);
    InventoryApplyService->ApplyDelta(Delta);

    // 6. Обновить поля (раз в секунду)
    static float AccTime = 0;
    AccTime += DeltaTime;
    if (AccTime >= PropagationInterval)
    {
        FieldPropagationService->Step(AccTime);
        AccTime = 0;
    }

    // 7. Опубликовать факты
    EventOutbox->Publish(FactWorldChanged{Delta});
    EventOutbox->Publish(FactInventoryChanged{Delta.InventoryOps});

    // 8. Обновить восприятие и оповестить ViewModel
    PerceptionBuilder->Update(Snapshot, StateSubsystem->GetMorokFields());

    // 9. Записать трассировку
    TraceRecorder->RecordFrame(Snapshot, Commands, Delta, RngState);
}
```

---

## 9. Тестирование

| Тест | Что проверяет |
|------|----------------|
| `AssetCatalog_IngredientTable_IsValid` | Валидность таблиц, нормализация осей, диапазон `MorokAffinity`. |
| `Pipeline_Stage_Fold` | Правильность смешивания ингредиентов с весами. |
| `Pipeline_Stage_Morok` | Нелинейное искажение. |
| `Pipeline_Stage_Bifurcation` | Коллапс/очищение при порогах. |
| `Pipeline_Determinism` | Два последовательных прогона с одинаковыми входными данными дают идентичную дельту. |
| `FieldPropagation_Diffusion` | Начальное пятно Морока за N шагов распространяется по соседям. |
| `CollectWater_Determinism` | Повторный прогон с сохранённой точкой попадания даёт ту же воду. |
| `Inventory_PassiveDecay` | Предмет за N игровых часов портится до ожидаемого состояния. |
| `World_ApplyDelta_Atomic` | Дельта с ошибкой не применяется частично. |
| `EventOutbox_Subscription` | Подписчик получает факт, отписка работает. |

Все тесты используют фиксированный RNG или сохранённые снапшоты.

---

## 10. Консольные команды для дизайнеров

| Команда | Описание |
|---------|-----------|
| `herb.debug.grid on/off` | Показать/скрыть сетку клеток |
| `herb.debug.field morok` | Визуализация Морока (чёрные–белые сферы) |
| `herb.debug.field zaryana` | Визуализация Заряны |
| `herb.sim.snapshot` | Сохранить текущий снапшот в файл |
| `herb.sim.replay` | Воспроизвести последний записанный кадр |
| `herb.inventory.add <id>` | Добавить тестовый предмет |
| `herb.cell.info` | Показать параметры клетки под курсором |
| `herb.biome.override <type>` | Временно переопределить биом всей карты |

---

## 11. Полная таблица изменений файлов (объединённая)

| Файл | Действие | Ключевые изменения |
|------|----------|--------------------|
| `UWorldStateSubsystem.h/.cpp` | **Новый** | Центральное хранилище состояния мира (вместо GridWorldManager) |
| `UFieldPropagationService.h/.cpp` | **Новый** | Диффузия Морока/Заряны, визуализация сферами |
| `UPCGBridge.h/.cpp` | **Новый** | Регистрация PCG-ресурсов, сбор, обновление визуала по фактам |
| `USimulationOrchestrator.h/.cpp` | **Новый** | Управление тиком симуляции |
| `UCommandBus.h/.cpp` | **Новый** | Очередь интентов, валидация, нормализация |
| `FPipelineExecutor.h/.cpp` | Переработан | Удалена стадия `InjectBiomeContext` для крафта, добавлен `ExecuteCollectWater` |
| `FPerceptionBuilder.h/.cpp` | Переработан | Искажение `S_real` на основе Morok/Zaryana |
| `UEventOutbox.h/.cpp` | **Новый** | Публикация фактов, управление подписками |
| `UHerbalistAssetCatalog.h/.cpp` | **Новый** | Статические таблицы (ингредиенты, вода, биомы) |
| `AGridWorldManager.h/.cpp` | Радикально упрощён | Оставлена только сетка и `ApplyDelta()`, удалены спавн, алхимия, сбор, очереди |
| `AHerbalistResourceActor.h/.cpp` | Облегчён | Отправляет `IntentHarvestResource`, не хранит состояние |
| `UHerbalistInventoryComponent.h/.cpp` | Переработан | Пассивный контейнер, мутации только через сервис |
| `AHerbalistPlayerController.h/.cpp` | Облегчён | Добавлен `CollectWater()` с трейсом, отправка интентов |
| `CommandTypes.h` | Расширен | Добавлен `FIntentCollectWater`, удалены поля биомного контекста |
| `DeltaTypes.h` | Расширен | Добавлены `RngChecksum`, `FValidationReport` |
| `SnapshotTypes.h` | Расширен | Включены копии полей Morok/Zaryana |
| `TraceTypes.h` / `TraceReplay.cpp` | Расширен | Сохранение `HitLocation` для воды, экспорт/импорт |
| `HerbalistSettings.h/.cpp` | Расширен | Настройки диффузии, визуализации, воды |
| `ProjectHerbalist.Build.cs` | Расширен | Пути `Public/Core`, `Public/UI`, модуль `PCG` |
| `ProjectHerbalistGameModeBase.h/.cpp` | Облегчён | Только вызов Bootstrap |
| `HerbalistInventoryComponent.cpp` | Удалена логика Tick | Порча пассивная |
| `BiomeGraphSubsystem.h/.cpp` | **Удалён** | Заменён FieldPropagationService и биомными текстурами |
| `BiomeGraphTypes.h` | **Удалён** | Граф биомов упразднён |
| `HarvestService.h/.cpp` | **Удалён** | Логика сбора в `PipelineExecutor` |
| `AlchemySubsystem.h/.cpp` | **Удалён** | Правила в `RuleSet`, пайплайн в `PipelineExecutor` |
| `IngredientRegistrySubsystem`, `WaterTypeRegistrySubsystem` | **Удалены** | Заменены `AssetCatalog` |
| `PerceptionService.h/.cpp` | **Удалён** | Заменён `PerceptionBuilder` |
| `SnapshotService.cpp` | Расщеплён | Функции разнесены по сервисам |
| `GridWorldManagerAlchemy.cpp`, `GridWorldManagerHarvest.cpp` | **Удалены** | Мёртвый код |
| `SimulationModule.cpp` | **Удалён** | Пустой файл |
| Все ViewModel и Widgets | Переработаны на MVVM | Интенты, perceived-данные, подписка на факты |

---

## 12. Системный контракт v4.0 (окончательный)

### 12.1. Запреты (повторение)

- Прямая мутация `S_real` или инвентаря из UI/Actor'ов запрещена.  
- Чтение реальных данных слоем UI запрещено (только `S_perceived`).  
- RNG доступен только внутри пайплайна через передаваемое состояние.  
- Сбор воды при replay не использует повторный трейс – координаты сохраняются.  
- Никто кроме `FieldPropagationService` не изменяет `MorokField`/`ZaryanaField` (кроме зелий через дельту).  
- Запрещены циклические зависимости модулей.

### 12.2. Расширяемость

- Новые типы ресурсов – добавляются в `AssetCatalog` и таблицы.
- Новые биомы – добавляются текстура маски и строки в таблицу биомов.
- Новые поля (например, `Fertility`) – расширяется `FGridCellFull` и `FieldPropagationService`.
- Новые команды – добавляются в `CommandTypes.h` и обрабатываются в `PipelineExecutor`.

### 12.3. Ожидаемая производительность (5×5 км, 1.56 млн клеток)

| Операция | Частота | Время (CPU) |
|----------|---------|--------------|
| Диффузия полей (шаг) | 1 раз/сек | 1–2 мс (ParallelFor) |
| Снэпшот (полный) | раз в 10 кадров | 5–10 мс (можно асинхронно) |
| Снэпшот (дельта) | каждый кадр | 0.1–0.5 мс |
| Пайплайн на 1 команду | в среднем 10/кадр | 0.05 мс |
| Обновление визуализации сфер | по запросу | 0.5–1 мс (ISM) |

---

## Заключение

Архитектура готова к реализации. Все ключевые решения задокументированы:  

- **Центральное состояние** – `UWorldStateSubsystem`.  
- **Диффузия полей** – быстрая, масштабируемая, детерминированная.  
- **PCG + World Partition** – полный контроль над визуалом без потери данных.  
- **Детерминизм** – снапшоты, replay, сохранение координат.  
- **Производительность** – оптимизирована для 5×5 км и 1.5–6 млн клеток.  
- **Тестирование** – покрыты все критические подсистемы.

Документ является обязательным для всех разработчиков. Изменения вносятся только через формальное обновление версии.

---

*Версия 4.0 (окончательная, объединённая)*  
*Дата: 2026-05-01*

---
---

## Дополнение к финальному архитектурному документу: Операционные контракты

*Данный раздел фиксирует **обязательные правила выполнения**, которые не относятся к структуре классов, но критичны для детерминизма и стабильности системы. Архитектура считается закрытой для крупных изменений; далее — только уточнения контрактов.*

---

### 1. Tick order contract (детерминированная последовательность)

Каждый игровой кадр (тик) `SimulationOrchestrator::ExecuteFrame()` **строго** выполняет следующие шаги в указанном порядке. **Никакой код вне этого потока не может изменять состояние мира, инвентаря, полей или RNG.**

| Шаг | Описание | Примечание |
|-----|----------|-------------|
| 1. **Capture Snapshot** | Атомарное копирование всего состояния (`WorldState`, `Inventory`, `RngState`) в `FSimulationSnapshot`. | Запрещены любые изменения во время копирования. |
| 2. **Dequeue Intents** | Извлечение всех накопленных интентов из `CommandBus` (очередь очищается полностью). | Интенты, поступившие во время выполнения этого кадра, попадут в следующий кадр. |
| 3. **Pipeline Execution** | Вызов `PipelineExecutor->Execute(Snapshot, Commands)` – чистая функция без сайд-эффектов. | Результат – `FStateDelta`. |
| 4. **Validation Gate** | Проверка дельты на инварианты (NaN, отрицательные остатки, допустимые диапазоны). | При ошибке дельта отбрасывается, публикуется факт ошибки, дальнейшие шаги пропускаются. |
| 5. **Apply World Delta** | `WorldApplyService->ApplyDelta()` → единственное место, вызывающее `UWorldStateSubsystem::ApplyDelta()`. | Мир изменяется **только здесь**. |
| 6. **Apply Inventory Delta** | `InventoryApplyService->ApplyDelta()` → единственное место, изменяющее инвентарь. | Инвентарь изменяется **только здесь**. |
| 7. **Field Propagation (если пора)** | Если с прошлого шага прошло >= `PropagationInterval`, вызывается `FieldPropagationService->Step()`. | Диффузия и обновление полей Морока/Заряны. |
| 8. **Publish Facts** | `EventOutbox->Publish()` для `FactWorldChanged`, `FactInventoryChanged`, `FactFieldsUpdated`. | Все факты публикуются **синхронно**, но обработчики не могут мутировать состояние. |
| 9. **Perception Rebuild** | `PerceptionBuilder->Update()` пересчитывает `S_perceived` из свежего снапшота и новых полей. | Результат кешируется для ViewModel. |
| 10. **Trace Recording** | `TraceRecorder->RecordFrame()` сохраняет снапшот, команды, дельту и RNG-состояние. | Только если `bEnableTracing == true`. |
| 11. **UI Update** (неявно) | ViewModel, подписанные на факты, обновляют свои свойства → привязанные виджеты перерисовываются. | Не входит в `ExecuteFrame`, но происходит в том же потоке сразу после публикации фактов. |

**Запрещается:**
- Вызывать `ApplyDelta` из любого другого места.
- Менять поля Морока/Заряны напрямую (только через `FieldPropagationService::Step` или через зелья, которые идут через дельту).
- Читать `UWorldStateSubsystem` во время шагов 5–6 из других потоков.

---

### 2. Mutation rules (права на изменение)

| Компонент | Что может мутировать | ЧТО НЕ МОЖЕТ мутировать |
|-----------|----------------------|--------------------------|
| `WorldApplyService` | Вызывать `UWorldStateSubsystem::ApplyDelta()` (только он) | Инвентарь, поля (кроме как через дельту), RNG |
| `InventoryApplyService` | Вызывать `UInventoryState::ApplyDelta()` (только он) | Мир, поля, RNG |
| `FieldPropagationService` | Массивы `MorokField`, `ZaryanaField` (через внутренний double-buffer) | Мир, инвентарь, RNG |
| `PipelineExecutor` | **Ничего** (чистая функция, только возвращает дельту) | Всё остальное |
| `UPCGBridge` | **Ничего** (только читает состояние для спавна/обновления визуала) | Мир, инвентарь, поля, RNG |
| `UEventOutbox::Publish` | **Ничего** (только вызывает подписчиков) | Состояние (подписчики не могут мутировать) |
| ViewModel / UI | **Ничего** (только читают `S_perceived` и отправляют интенты) | Мир, инвентарь, поля, RNG |
| Console commands (отладка) | Только в `WITH_EDITOR` и только через отправку интентов или специальные debug-флаги | Прямая мутация запрещена |

**Исключение:** Отладочные команды визуализации (сферы) работают с `UInstancedStaticMeshComponent`, но не с данными симуляции.

**Золотое правило:** *Единственный способ изменить состояние – отправить Intent. Единственное место, где Intent превращается в изменение – `SimulationOrchestrator::ExecuteFrame`.*

---

### 3. Snapshot guarantees (детерминированное состояние)

`FSimulationSnapshot` считается **полным и достаточным** для полного восстановления состояния игры на момент захвата. Он включает:

| Компонент | Что сохраняется | Что НЕ сохраняется |
|-----------|----------------|---------------------|
| **Мир** | `ResourceID`, `GrowthStage`, `Magnitude`, `Distortion`, `Direction` для каждой клетки (в сжатой форме) | Временные визуальные эффекты, анимации |
| **Поля** | Полные массивы `MorokField` и `ZaryanaField` (размер `GridSizeX * GridSizeY`) | История изменений полей |
| **Инвентарь** | Список предметов: `ItemID`, количество, `FRealState` каждого | UI-состояние (выбранный слот и т.п.) |
| **RNG** | Текущее состояние `FRandomStream` (seed и позиция) | Статистика вызовов |
| **Биомная маска** | Не сохраняется (статические данные, есть в `AssetCatalog`) | – |
| **Сущности** | Если есть динамические сущности (NPC) – их позиции и состояния | – |

**Гарантии:**
- Два одинаковых снапшота + одинаковый список команд = идентичная дельта и итоговое состояние.
- `FSimulationSnapshot` можно сериализовать в файл и восстановить позже (сохранение/загрузка).
- При сохранении снапшота **все ссылки на UObject'ы** заменяются на первичные ключи (ID), чтобы избежать висячих указателей.

---

### 4. Event semantics (правила работы EventOutbox)

`UEventOutbox` – синхронный, однонаправленный канал уведомлений. **Не является шиной сообщений общего назначения.**

**Правила:**

| Правило | Описание |
|---------|----------|
| **Синхронная публикация** | `Publish(Fact)` вызывает всех подписчиков немедленно, в том же потоке, до возврата управления. |
| **Запрет на мутации** | Подписчики **не могут** изменять мир, инвентарь, поля или RNG. Исключение – отладочная визуализация (сферы). |
| **Новые Intents** | Подписчики **могут** отправлять новые интенты в `CommandBus`. Они попадут в очередь и будут выполнены в **следующем** кадре (не в текущем). Это предотвращает рекурсию. |
| **Запрет рекурсивного Publish** | Внутри обработчика факта запрещён вызов `Publish` того же или другого факта (приводит к ассерту в дебаге). |
| **Отписка** | Все подписчики обязаны отписаться в своём `BeginDestroy` / `NativeDestruct`. `UEventOutbox` использует слабые ссылки, но явная отписка обязательна. |
| **Потокобезопасность** | `UEventOutbox` работает **только** в GameThread. Вызов `Publish` из другого потока приведёт к крашу. |

**Пример разрешённого использования:**

```cpp
void UInventoryViewModel::OnFactInventoryChanged(const FFactInventoryChanged& Fact)
{
    // Чтение: обновить свои свойства
    UpdateItems(Fact.NewInventorySnapshot);
    
    // Разрешено: отправить новый Intent (будет в следующем кадре)
    CommandBus->SubmitIntent(FIntentRefreshUI{});
    
    // Запрещено: менять мир или инвентарь
    // WorldState->ApplyDelta(...); // ОШИБКА!
}
```

**Пример запрещённого использования (рекурсия):**

```cpp
void UBadSubscriber::OnFact(const FFact& Fact)
{
    EventOutbox->Publish(FFactAnother{}); // ОШИБКА! Рекурсивный Publish
}
```

---

## Статус архитектуры

После добавления этих операционных контрактов архитектура считается **закрытой для крупных перестроек**. Дальнейшие изменения:

- **Допустимы:** уточнение API, оптимизация производительности, добавление новых типов команд/ингредиентов/правил, улучшение визуализации.
- **Требуют пересмотра контрактов:** изменение Tick order, введение нового писателя, нарушение детерминизма, изменение семантики событий.

**Переход к фазе реализации:** Все ключевые решения приняты. Приступаем к написанию кода, тестов и документации для дизайнеров.

---

*Версия документа: 4.1 (операционные контракты добавлены)*  
*Дата: 2026-05-01*