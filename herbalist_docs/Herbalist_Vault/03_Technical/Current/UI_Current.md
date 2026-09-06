---
tags: [technical, current, ui]
version: 2.0
based_on: ProjectHerbalist source, 2026-09-06
---
# Пользовательский интерфейс — реализация

Переписано 2026-09-06 — прежняя версия не называла Травник/MemoryRevealWidget
вовсе и не отражала, что подавляющее большинство систем, добавленных после
Фазы 4, управляется Exec-консолью, не назначенными Input Action (сознательный
v1-выбор проекта, не пробел). История — `CHANGELOG.md`.

## Виджеты

### `UInventoryWidget`
- Отображает слоты инвентаря в `VerticalBox`.
- Поддерживает Drop из других инвентарей.
- Привязывается к `UHerbalistInventoryComponent`.

### `UInventorySlotWidget`
- Отображает один слот.
- Двойной клик — попытка переместить в другой инвентарь (контекстно: в
  открытый трансфер или алхимический стол); слот-витрина результата варки
  теперь только очищается сам, не добавляет предмет в инвентарь повторно
  (аудит 2026-09-05).
- Drag — создаёт `UInventoryDragDropOperation`; перенос через общий
  `UInventoryDragDropController::TryTransferItem`.
- Drop — принимает предметы из других инвентарей.
- `FindRealIndex` резолвит слот по стабильному `CreationTime`, не по
  дрейфующему от порчи/сушки/отстоя/выпаривания `State` (аудит 2026-09-05
  — раньше мог найти чужой слот).
- Тултип — `UItemTooltipWidget` при наведении, показывает человекочитаемое
  имя травы (`GetItemDisplayName`), не сырой `IngredientID`.

### `UInventoryTransferWidget`
- Контейнер для двух `UInventoryWidget` (левый и правый).
- Используется при взаимодействии с `AStorageContainer` (переносные и
  стационарные хранилища, сушилка/отстойник/выпарной куб — все подклассы).

### `UAlchemyTransferWidget`
- Три слота ингредиентов + слот воды + слот-витрина результата
  (`IngredientSlot1/2/3`, число слотов жёстко зашито — открытый пункт,
  см. `ROADMAP.md`).
- Coherence — превью через `ComputeIntentCoherence` (`PipelineV2.cpp`),
  реальный крафт пересчитывает то же значение сам, не доверяет превью.
- Закрытие котла без "Смешать" возвращает содержимое слотов в инвентарь,
  не уничтожает (аудит 2026-09-05).
- Переполнение слота честно отказывает (`AddItem` → `false`), не усекает
  `Amount` молча при Shift-сплите (аудит 2026-09-05).
- Глобальный `Distortion` — из `HPC->CurrentGlobalDistortion`.

### `UAlchemySlotWidget`
- Классификация через `IngredientRegistrySubsystem`/`FIngredientTableRow::bIsWater`.
- Вторая и последующие единицы травы в одном слоте честно блендятся
  (`HerbalistCore::Math::BlendRealStatesForStack`, общая формула с
  `MergeStack` инвентаря), не отбрасываются в пользу первой (аудит
  2026-09-05).

### `UItemTooltipWidget`
- Показывает **S_perceived**: искажённые значения + реальные в скобках.
- Искажение через `Perception::PerceiveValue`, детерминированный сид от
  identity+state предмета (не от тика, см. `Inventory_Current.md`).
- `PerceiveClass` в коде не существует — см. `ROADMAP.md`.

### `UJournalLogWidget` (Травник, читаемый лог)
- Чистый C++, без единого `.uasset` — дерево виджета строится в коде,
  готов без WBP. Открывается `ToggleJournalUI()`.
- Фильтр по конкретному ингредиенту.
- `UJournalWidget`/`UJournalEntryRowWidget` — более богатая WBP-раскладка,
  спроектирована, но не назначена в редакторе (не блокирует — `JournalLogWidget`
  уже полностью рабочий путь).

### `UMemoryRevealWidget` (фрагменты памяти Заряны)
- Строит дерево виджета явно, не полагаясь на порядок вызова относительно
  `AddToViewport()` (аудит 2026-09-05 — раньше ломался навсегда на первом
  показе).
- Первые в проекте автотесты на UMG-виджет, гоняются без реального
  вьюпорта.

## Управление вводом (реально назначенные Enhanced Input действия)

`AHerbalistPlayerController::SetupInputComponent` — каждая привязка логирует
собственное имя и то, к какому ассету привязана (или явный `Warning`, если
`UInputAction` не назначен в Blueprint'е — самая тихая поломка ввода,
2026-09-03):

- `Move`/`Look` — передвижение/камера.
- `HarvestAction` (сбор ресурса).
- `InfoAction` (информация о клетке).
- `InventoryAction` (открыть/закрыть инвентарь).
- `JournalAction` (открыть/закрыть Травник).
- `ApplyAlchemyAction` (применить первое зелье инвентаря на клетку под
  прицелом).
- `InteractAction` (взаимодействие с объектом — сундук, алхимический стол).
- `UsePotionAction` — тот же обработчик, что `ApplyAlchemyAction` вызывает
  (`OnUsePotion`).

## Exec-консоль — вся механика, добавленная после Фазы 4

Каждая новая система с 2026-08-31 получала **v1 через Exec-команду
`AHerbalistPlayerController`**, не выделенный Input Action/WBP-экран —
сознательный, повторяющийся выбор проекта (сам механизм проверяется и
работает уже сейчас, UI/level-design — отдельный, явно откладываемый
проход, см. `ROADMAP.md` "Контент/редактор"). Не полный список, ориентир
по категориям:

- Сад/сбор: `SetGardenPlot`, `PlantSeed`, `SetHarvestIntent`,
  `SetGatheringTool`, `ApplyFertilizer`.
- Обереги/артефакты: `ActivateWard`, `EquipSilverWard`, `LootKurgan`,
  `UseInvisibilityCap`, `UseYouthApple`, `UseComb`, `UseMirror`, `UseYarnBall`,
  `AcquireFeather`+`Use*Feather`.
- Хранилища: `EquipContainer`, `BuildHomeStorage`.
- Община: `TalkTo`, `ChooseDialogueBranch`, `OfferToCommunity`,
  `TradeWithCommunity`, `FoundBase`.
- Отладка/диагностика: `SelectCell`+`GetSelectedCellInfo` (правый клик,
  единственный консольный способ проверить состояние клетки без остановки
  игры — включая курганы, `Kurgan=<ID>` суффикс, если на клетке есть
  неразграбленный).

## Взаимодействие с миром

- `AStorageContainer` (и подклассы — сушилка/отстойник/выпарной куб) —
  открывает `InventoryTransferWidget`.
- `AAlchemyTableActor` — открывает `AlchemyTransferWidget`, тот же актор
  регистрирует Домового на своей клетке при `BeginPlay`.
- При открытии любого виджета включается курсор и UI-режим ввода.
