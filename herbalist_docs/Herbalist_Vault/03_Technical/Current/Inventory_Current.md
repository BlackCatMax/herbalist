---
tags: [technical, current, inventory]
version: 2.0
based_on: ProjectHerbalist source, 2026-09-06
---
# Инвентарь — реализация

Переписано 2026-09-06 — прежняя версия (Фазы 1-4) не упоминала ни одно из
полей/механик, добавленных с тех пор (посадочный материал, биом сбора,
сушка/отстой/выпаривание, переносные и стационарные хранилища). История —
`CHANGELOG.md`.

## Компонент `UHerbalistInventoryComponent`
- Хранит `TArray<FInventoryItem>`.
- Максимальный размер стека — `MAX_STACK_SIZE = 9`.
- `MaxSlots` — общее количество слотов (по умолчанию 20, за пределы не
  выходит независимо от типа контейнера).
- `ContainerType` (`EStorageContainerType`) — влияет только на
  decay-множитель предметов внутри, не на `MaxSlots`: `None` (на себе, без
  контейнера) / `Basket` (корзина, хуже базовой линии, открытая) / `Sack`
  (мешок, хуже корзины — держит влагу) / `Tues` (туёс, лучше базовой линии,
  но переносной) / `Cabinet` (шкаф, закрыт от пыли/вредителей) / `Cellar`
  (погреб, тёмный/прохладный) / `Jar` (банка, герметичная — лучшее
  хранение). `Basket`/`Sack`/`Tues` — переносные (носятся, `EquipContainer`);
  `Cabinet`/`Cellar`/`Jar` — стационарные расширения дома (`BuildHomeStorage`,
  требуют Respect Домового + материал).
- `StationType` (`EProcessingStationType`) — переключает поведение
  `TickComponent` для растянутых во времени процессов: `None` / `DryingRack`
  (сушилка) / `SettlingStand` (отстойник) / `EvaporationStill` (выпарной
  куб). Никак не связан с `ContainerType` — сушилка/отстойник/выпарной куб
  не хранилища decay-типа, а станции обработки.

## `FInventoryItem`

    struct FInventoryItem {
        FName IngredientID;
        FRealState State;
        int32 Count;
        float CreationTime;
        bool bSubjectToDecay;
        bool bIsWater;
        EAlchemyOutcome BrewOutcome;    // Valid/BoiledWater/Ash/Catastrophe/Purified
        bool bIsPlantingStock;          // посадочный материал, не для варки
        EBiomeType SourceBiome;         // биом сбора, для межбиомного бонуса варки
        bool bIsDried;
        float DryingTimeRemainingSeconds;    // -1 = не сушится/не сушился
        bool bHasSettled;
        float SettlingTimeRemainingSeconds;  // -1 = не в отстое
        bool bHasEvaporated;
        float EvaporationTimeRemainingSeconds; // -1 = не выпаривался
    };

Три пары `bHasX`/`XTimeRemainingSeconds` (сушка/отстой/выпаривание) —
независимые терминальные процессы, каждый с собственной формулой эффекта
(сушка меняет `Meta` через `IngredientTableRow::DriedStateDelta` ценой
ничего; отстой усиливает доминирующую ось `Direction` ценой `Magnitude`;
выпаривание концентрирует `Magnitude`/`Potency` ценой `Distortion`/
`Corruption`) — сознательно не обобщены в один "процесс", тот же принцип,
что у трёх независимых наборов полей оберегов (`GridWorldManagerWards.cpp`).
Таймер каждого тикает ТОЛЬКО пока предмет физически лежит в
соответствующей станции (`StationType`) — вынутый раньше срока замирает на
месте, не сбрасывается.

## Стекирование

Два предмета стекуемы (`AreItemsStackable`), если выполнено ВСЁ:

- Одинаковый `IngredientID`.
- Одинаковый `bIsPlantingStock` (посадочный материал не мешается с обычным
  сбором того же вида).
- Одинаковый `bIsDried` (сушёный и свежий — разные алхимические сущности).
- Ни один не в процессе сушки (`DryingTimeRemainingSeconds < 0` у обоих).
- Одинаковый `bHasSettled`, ни один не в процессе отстоя.
- Одинаковый `bHasEvaporated`, ни один не в процессе выпаривания.
- `HerbalistCore::Math::AreStatesSimilar(A.State, B.State)` — допуски по
  умолчанию: `Magnitude` 0.15, `Distortion`/`Purity`/`Stability` 0.2,
  `Direction` (евклидово расстояние) 0.3.

При слиянии (`MergeStack` → `BlendRealStatesForStack`,
`HerbalistCoreMath.cpp`) все оси `State` усредняются с весами,
пропорциональными количеству — простое взвешенное среднее, без
искусственного смещения Distortion/Purity в какую-либо сторону.

## Операции

- `AddItem` — пытается добавить в существующую стопку или создаёт новую.
- `RemoveItem` — уменьшает счётчик, удаляет слот при `Count=0`.
- `TransferItemTo` — перенос 1 единицы в другой инвентарь.
- `SplitStack` — разделение стопки (используется при Shift+Drag).
- `GetAvailableCapacityFor` — вместимость под конкретный предмет ДО
  списания (существующие совместимые стеки + свободные слоты × `MAX_STACK_SIZE`),
  чтобы `TradeWithCommunity`/подношения могли проверить место, не рискуя
  потерять товар при переполнении.

## Drag & Drop

- `UInventoryDragDropOperation` (`Core/Inventory/`) хранит `SourceInventory`,
  `SourceIndex`, `bIsSplit`, `SplitItem`.
- `UInventoryDragDropController` (`UI/`) — статический
  `TryTransferItem(SourceInventory, SourceIndex, ...)`, вынесенная общая
  логика переноса, читаемая из нескольких мест (слот инвентаря, слот
  алхимии).
- При обычном перетаскивании перемещается вся стопка.
- При Shift — создаётся разделённая стопка (половина).
- Отмена drag'а возвращает предмет в исходный инвентарь.

## Хранилища — реальный мир, не только компонент

- `AStorageContainer` — актор с `InventoryComponent`, открывает
  `UInventoryTransferWidget` при взаимодействии.
- Переносные типы получаются экипировкой предмета из инвентаря
  (`AHerbalistPlayerController::EquipContainer`, поиск по имени, не
  расходуется).
- Стационарные (`Cellar`/`Cabinet`/`Jar`) строятся у клетки-якоря дома
  (`BuildHomeStorage`) — Respect Домового ≥ порога И материал (Дубовая
  кора) одним стеком, тот же приём "мягкой прокачки", что и у сада
  (`GardenNicheUnlockTypes.h`). Не больше одного хранилища каждого типа.
- `ADryingRackActor`/аналогичные для отстоя/выпаривания — тоже подклассы
  `AStorageContainer`, отличаются только `StationType` компонента внутри.

## Восприятие в тултипе

- `UItemTooltipWidget` показывает **S_perceived** (искажённые значения +
  реальные в скобках) через `Perception::PerceiveValue` — детерминированный
  сид от identity+state предмета (`ComputeInventoryPerceptionSeed`,
  `PerceptionService.cpp`, 2026-09-05), не от тика: повторные наведения на
  один и тот же предмет дают один и тот же шум, не усредняются к честному
  `S_real` наблюдением.
- `PerceiveClass` (подмена самого факта "что это за вещь") в коде НЕ
  существует — искажаются только числа, не идентичность предмета. См.
  `ROADMAP.md`.
- Глобальный `Distortion` — из `AHerbalistPlayerController::CurrentGlobalDistortion`.
