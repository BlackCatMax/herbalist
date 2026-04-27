# План приведения проекта Herbalist в порядок

## Общая стратегия

Цель: устранить критические баги, обеспечить соответствие GDD, повысить стабильность и производительность. Работа разбита на **5 этапов**. Каждый этап завершается тестированием (PIE, юнит-тесты).

Приоритеты:
- **Критический** (Crash / блокировка геймплея)
- **Высокий** (Искажение геймплея, несоответствие спецификации)
- **Средний** (Производительность, удобство разработки)
- **Низкий** (Чистота кода, отложенные фичи)

---

## Этап 0: Подготовительный (перед исправлениями)

- **Создать резервную ветку** (git) или архив исходников.
- **Запустить все существующие автоматические тесты** (`IngredientRegistryTest.cpp`) – убедиться, что они проходят.
- **Собрать проект в Development Editor**, убедиться, что нет ошибок компиляции.
- **Создать чек-лист для ручного тестирования** (сбор, алхимия, открытие UI, сундук, применение зелья, граф биомов).

---

## Этап 1: Исправление критических багов (блокируют геймплей)

### 1.1. Реализовать `AGridWorldManager::OnResourceCollected`
**Проблема:** Сбор через актор ресурса не обновляет клетку (стресс, регенерация, Distortion).
**Решение:**
```cpp
void AGridWorldManager::OnResourceCollected(AHerbalistResourceActor* Actor)
{
    if (!Actor) return;
    FGridCell* Cell = GetCell(Actor->GetGridX(), Actor->GetGridY());
    if (!Cell) return;
    
    // Симулируем сбор через основную логику (но без добавления предмета повторно)
    FRealState ResourceState = HarvestFromCell(Actor->GetGridX(), Actor->GetGridY(), FConditionModifier());
    if (ResourceState.Magnitude < 0.01f && ResourceState.Meta.Distortion < 0.01f)
    {
        UE_LOG(LogHerbalist, Warning, TEXT("OnResourceCollected: invalid resource state, skipping inventory add"));
        return;
    }
    
    // Добавляем предмет в инвентарь игрока (актор сам не добавляет)
    AHerbalistPlayerController* PC = Cast<AHerbalistPlayerController>(GetWorld()->GetFirstPlayerController());
    if (PC && PC->InventoryComponent)
    {
        FInventoryItem Item;
        Item.IngredientID = Actor->GetIngredientID();
        Item.State = ResourceState;
        Item.Count = 1;
        PC->InventoryComponent->AddItem(Item, 1);
    }
}
```
В `AHerbalistResourceActor::Harvest` **удалить** добавление предмета в инвентарь (оставить только визуал, эффекты и вызов `OnResourceCollected`).
**Приоритет:** Критический.

### 1.2. Исправить восстановление стресса (тик)
**Проблема:** При снижении `HarvestStress` не пересчитываются Distortion и Magnitude клетки.
**Решение:** В `GridWorldManager::Tick` в цикле по `StressCells` после изменения `HarvestStress` вызвать `RecalculateDistortionFromHarvestStress(*Cell)`.
```cpp
Cell->HarvestStress = FMath::Max(0.0f, Cell->HarvestStress - HarvestStressDecayRate * DeltaTime);
RecalculateDistortionFromHarvestStress(*Cell); // добавить
```
**Приоритет:** Критический.

### 1.3. Учесть граф биомов в условии тика
**Проблема:** Тик выключается, если нет активных клеток, но граф должен продолжаться.
**Решение:** В `GridWorldManager::Tick` в конце изменить:
```cpp
bool bShouldTick = (DirtyCells.Num() > 0) || (RegrowingCells.Num() > 0) || (StressCells.Num() > 0);
UBiomeGraphSubsystem* Graph = GetWorld()->GetSubsystem<UBiomeGraphSubsystem>();
if (Graph && Graph->IsInitialized()) bShouldTick = true;
```
**Приоритет:** Критический.

### 1.4. Исправить split-баг в `AlchemySlotWidget::NativeOnDrop`
**Проблема:** При переносе разделённой стопки в слот алхимии добавляется 1 единица вместо `SplitItem.Count`.
**Решение:** В `AlchemySlotWidget::NativeOnDrop` изменить:
```cpp
if (DragOp->bIsSplit)
{
    ItemToMove = DragOp->SplitItem;
    // не меняем Count
}
...
if (AddItem(ItemToMove, ItemToMove.Count))  // было 1
```
**Приоритет:** Критический.

### 1.5. Исправить `Perception::PerceiveClass` – лог после return
**Проблема:** UE_LOG никогда не выполняется.
**Решение:** Перенести `UE_LOG` перед `return`.
**Приоритет:** Критический (чистота отладки).

### 1.6. Сделать `FL2Direction::NormalizeL2` детерминированной
**Проблема:** Использует `FMath::FRand()` вместо переданного `Rng`.
**Решение:** В `HerbalistCoreTypes.h` внутри `NormalizeL2` заменить вызовы `FRand()` на `Random01(Rng)` (функция `Random01` определена в `HerbalistPipeline.h`). Для этого нужно, чтобы `Rng` был доступен – он уже есть в сигнатуре. Но `Random01` – свободная функция, требуется включить `HerbalistPipeline.h` в `HerbalistCoreTypes.h`? Это создаст циклическую зависимость. Лучше вынести `Random01` в отдельный хедер или реализовать свою версию внутри `NormalizeL2`:
```cpp
float Random01(FRngState& RngState)
{
    RngState.Seed = (RngState.Seed * 196314165) + 907633515;
    return (RngState.Seed & 0x00FFFFFF) / float(0x01000000);
}
```
**Приоритет:** Критический (детерминизм).

### 1.7. Удалить мёртвый код `FBiomeDefaults::GetRandomResourceForBiome` (через ассеты)
**Проблема:** Метод сканирует AssetRegistry, но не используется; может быть вызван ошибочно.
**Решение:** Закомментировать тело, оставив `return NAME_None;` и пометить `[[deprecated]]`, либо удалить полностью. Заменить вызовы в `GridWorldManager::InitializeCells` на `FIngredientRegistry::GetRandomResourceForBiome` (уже используется).
**Приоритет:** Критический (риск падения производительности).

---

## Этап 2: Интеграционные исправления (соответствие GDD, экология)

### 2.1. Добавить `MarkDirty` в `ApplyBiomeInfluences`
**Проблема:** Изменения Stability/Purity от графа не интерполируются.
**Решение:** В цикле по клеткам после изменения `TargetState` добавить `MarkDirty(Cell.X, Cell.Y)`.
**Приоритет:** Высокий.

### 2.2. Передавать `WaterTypeID` в `HarvestWater` и использовать `WaterTypeRegistry`
**Проблема:** Тип воды не влияет на результат сбора.
**Решение:**
- Изменить сигнатуру `HarvestWater` на `HarvestWater(const FGridCell& Cell, const FConditionModifier& Conditions)`.
- Внутри получить `WaterTypeID` из `Cell.WaterTypeID`, загрузить `FWaterTypeRow`, применить его параметры к `WaterState` (умножить или сложить).
**Приоритет:** Высокий.

### 2.3. Исправить `AlchemyTransferWidget::OnMixClicked` – использовать Distortion клетки стола
**Проблема:** Coherence зависит от `CurrentGlobalDistortion` (под прицелом), а не от места варки.
**Решение:** В методе уже есть `Cell` (если координаты стола валидны). Взять `GlobalD = Cell->Memory.AccumulatedDistortion`, иначе fallback на `HPC->CurrentGlobalDistortion`.
**Приоритет:** Высокий.

### 2.4. Закэшировать `WorldManager` в `PlayerController`
**Проблема:** `FindWorldManager` сканирует акторов при каждом вызове (редко, но неоптимально).
**Решение:** Добавить член `AGridWorldManager* CachedWorldManager`, инициализировать в `BeginPlay` и возвращать его в `FindWorldManager`, если валиден.
**Приоритет:** Средний.

### 2.5. Исправить восстановление Distortion/Magnitude при снижении стресса (уже в п.1.2)

---

## Этап 3: Исправление средних проблем (стабильность, производительность)

### 3.1. Добавить проверки на `nullptr` для `BindWidgetOptional` в UI
**Проблема:** Потенциальный краш, если виджет не содержит компонент.
**Решение:** Во всех `UpdateDisplay` и обработчиках перед использованием `IconImage`, `ItemNameText`, `CountText` и т.д. добавить `if (IconImage)`.
**Приоритет:** Средний.

### 3.2. Перевести статические реестры в `UGameInstanceSubsystem`
**Проблема:** Проблемы с Hot Reload и висячие указатели.
**Решение:**
- Создать `UIngredientRegistrySubsystem` и `UWaterTypeRegistrySubsystem`, наследующие `UGameInstanceSubsystem`.
- Перенести `TMap` и методы в них.
- Заменить все статические вызовы `FIngredientRegistry::GetRow(...)` на `GetGameInstance()->GetSubsystem<UIngredientRegistrySubsystem>()->GetRow(...)`.
- Удалить статические классы.
**Приоритет:** Средний (но требует рефакторинга, можно отложить, если Hot Reload не критичен).

### 3.3. Закэшировать списки ресурсов по биомам в `IngredientRegistry`
**Проблема:** `GetResourcesForBiome` и `GetRandomResourceForBiome` перебирают все строки при каждом вызове.
**Решение:** В `Initialize` построить `TMap<EBiomeType, TArray<FName>>` и `TMap<EBiomeType, TArray<int32>>` для весов.
**Приоритет:** Средний (производительность при большой сетке).

### 3.4. Удалить мёртвые поля и код
- `FAlchemyAtom::ContributionVector` (не используется)
- `UInventorySlotWidget::OtherInventory`, `PerceptionText`
- `UAlchemySlotWidget::SlotBorder`
- `FDeltaState` (в `PipelineTypes.h`)
- `BuildEnvironmentMeta` (в `PipelineMeta.cpp`)
- `LoadIngredientAsset` и `LoadIngredientAssetStatic` в `HarvestService`
- Двойной include в `AlchemySemantics.cpp`
**Приоритет:** Низкий.

### 3.5. Добавить проверку валидности `ResourceState` после `HarvestService::Harvest`
**Проблема:** При ошибке сбора клетка всё равно помечается регенерирующей.
**Решение:** В `HarvestFromCell` после вызова `HarvestService->Harvest` проверить, что `Resource.Magnitude >= 0.01f` или `Resource.Meta.Distortion > 0`. Если нет – вернуть пустое состояние и не менять клетку (не ставить таймер регенерации).
**Приоритет:** Средний.

### 3.6. Унифицировать `ProcessWaterOnly` и `ApplyBoiledWaterTransform`
**Проблема:** Дублирование кода.
**Решение:** Оставить одну реализацию, например, в `AlchemySemantics.cpp`, и вызывать её из `PipelineWater.cpp`.
**Приоритет:** Низкий.

---

## Этап 4: Улучшения архитектуры и соответствия GDD

### 4.1. Вынести константы `k_biome`, `k_condition` в `UHerbalistSettings`
**Проблема:** Жёстко заданы в коде.
**Решение:** Добавить в `UHerbalistSettings` поля `HarvestBiomeWeight = 0.6f` и `HarvestConditionWeight = 0.4f`, использовать их в `HarvestService`.
**Приоритет:** Средний.

### 4.2. Реализовать глобальный слой `S_perceived` (основное расхождение с GDD)
**Проблема:** UI показывает реальные значения, а должны искажённые.
**План минимум:** 
- В `InventorySlotWidget` и `AlchemySlotWidget` при отображении имени и параметров использовать `Perception` так же, как в тултипе.
- В `GridWorldManager::GetSelectedCellInfoBP` для отладки можно оставить реальные значения, но в UI игрока – искажённые.
- Создать вспомогательную функцию `FText GetPerceivedName(const FInventoryItem& Item, float GlobalDistortion)`.
**Приоритет:** Высокий (для соответствия GDD), но трудоёмкий.

### 4.3. Настроить автоматический вызов `InitializeCells`
**Проблема:** Может быть забыт в Blueprint.
**Решение:** В `AGridWorldManager::BeginPlay` добавить:
```cpp
if (Cells.Num() == 0) InitializeCells();
```
**Приоритет:** Низкий.

### 4.4. Добавить конфигурацию `MaxSlots` для `StorageContainer`
**Проблема:** Вместимость сундука не настраивается.
**Решение:** Добавить `UPROPERTY(EditAnywhere) int32 MaxSlots = 20;` и в `BeginPlay` или конструкторе установить `InventoryComponent->MaxSlots = MaxSlots`.
**Приоритет:** Низкий.

### 4.5. Восстановить `StabilityMemory` (обновлять в `UpdateMemory`)
**Проблема:** `StabilityMemory` всегда 0, а используется в катастрофе.
**Решение:** В `UpdateMemory` добавить:
```cpp
Memory.StabilityMemory = FMath::FInterpTo(Memory.StabilityMemory, NewState.Meta.Stability, DeltaTime, 0.1f);
```
Или убрать `StabilityMemory` из формулы катастрофы.
**Приоритет:** Средний.

---

## Этап 5: Подготовка к следующим фазам (отложенные фичи)

Эти пункты не обязательны для текущего прототипа, но важны для дальнейшего развития.

- **Реализовать эволюцию предметов в инвентаре (Фаза 5):** добавить `FTimerHandle` в `HerbalistInventoryComponent`, периодически вызывать функцию, изменяющую `State` (увеличение Distortion, снижение Purity). Учитывать тип предмета (вода портится быстрее).
- **Добавить влияние спецэффектов воды (`EWaterSpecialEffect`) в алхимию** – модифицировать `BlendWaterAndNonWater`.
- **Реализовать капища** – акторы, которые в радиусе снижают `MorokField` и повышают `Stability`.
- **Реализовать базовый вариант градиентов сбора** – добавить в `FConditionModifier` дельты от соседних клеток.
- **Добавить сезонные и погодные модификаторы** – передавать их в `HarvestService`.

---

## Резюме по приоритетам

| Этап | Задача | Приоритет |
|------|--------|------------|
| 1.1 | OnResourceCollected | Критический |
| 1.2 | Восстановление стресса | Критический |
| 1.3 | Тик графа | Критический |
| 1.4 | Split-баг в алхимии | Критический |
| 1.5 | Лог в Perception | Критический |
| 1.6 | Детерминизм L2 | Критический |
| 1.7 | Удалить мёртвый код ассетов | Критический |
| 2.1 | MarkDirty в ApplyBiomeInfluences | Высокий |
| 2.2 | WaterType в HarvestWater | Высокий |
| 2.3 | Distortion стола в алхимии | Высокий |
| 4.2 | Глобальный S_perceived | Высокий |
| 3.2 | Реестры в Subsystems | Средний |
| 3.3 | Кэширование списков ресурсов | Средний |
| 3.5 | Проверка результата Harvest | Средний |
| 4.1 | Константы в настройки | Средний |
| 4.5 | StabilityMemory | Средний |
| Остальные | Чистка, мелкие фиксы | Низкий |

**Ожидаемое время выполнения** (при полной занятости 1 разработчика):
- Этап 1: 2–3 дня.
- Этапы 2–3: 5–7 дней.
- Этап 4: 3–5 дней.
- Этап 5 (отложенные фичи) – по мере необходимости.

После завершения этапов 1–4 проект будет готов к закрытому тестированию и дальнейшей разработке.