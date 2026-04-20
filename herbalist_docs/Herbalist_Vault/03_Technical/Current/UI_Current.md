---
tags: [technical, current, ui]
---
# Пользовательский интерфейс — реализация
## Виджеты
### `UInventoryWidget`
- Отображает слоты инвентаря в `VerticalBox`.
- Поддерживает Drop из других инвентарей.
- Привязывается к `UHerbalistInventoryComponent`.
### `UInventorySlotWidget`
- Отображает один слот.
- Двойной клик — попытка переместить в другой инвентарь (контекстно: в открытый трансфер или алхимический стол).
- Drag — создаёт `UInventoryDragDropOperation`.
- Drop — принимает предметы из других инвентарей.
- Тултип — `UItemTooltipWidget` при наведении.
### `UInventoryTransferWidget`
- Контейнер для двух `UInventoryWidget` (левый и правый).
- Используется при взаимодействии с `AStorageContainer`.
### `UAlchemyTransferWidget`
- Содержит:
  - `PlayerInventory` (UInventoryWidget)
  - Слоты: `WaterSlot` (1), `IngredientSlot1-3` (до 9), `ResultSlot` (1)
- Кнопка `MixButton` запускает алхимию.
- При двойном клике по слоту ингредиента предмет возвращается в инвентарь игрока.
- Результат варки помещается в `ResultSlot`.
### `UAlchemySlotWidget`
- Специализированный слот для алхимического стола.
- Различает типы: `Water`, `Ingredient`, `Result`.
- `CanAcceptItem` проверяет соответствие типа.
### `UItemTooltipWidget`
- Показывает **все числовые параметры** предмета.
- **Важно:** отображает `S_real`, а не `S_perceived` (нарушает GDD, требуется доработка).
## Управление вводом
- `AHerbalistPlayerController` использует Enhanced Input.
- Действия:
  - `Harvest` (ЛКМ) — сбор ресурса.
  - `Info` (ПКМ) — информация о клетке.
  - `Inventory` (I) — открыть/закрыть инвентарь.
  - `Interact` (E) — взаимодействие с объектом (сундук, алхимический стол).
  - `ApplyAlchemy` (F) — тестовое применение алхимии на клетку.
  - `UsePotion` (U) — применить первое зелье из инвентаря на клетку под прицелом.
## Взаимодействие с миром
- `StorageContainer` — открывает `InventoryTransferWidget`.
- `AlchemyTableActor` — открывает `AlchemyTransferWidget`.
- При открытии любого виджета включается курсор и UI-режим ввода.