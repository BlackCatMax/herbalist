---
tags: [technical, current, ui]
---
# Пользовательский интерфейс — реализация (обновлено: Фазы 1-4)
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
- Coherence вычисляется динамически через `ComputeIntentCoherence`.
- Глобальный Distortion передаётся из `HPC->CurrentGlobalDistortion`.
### `UAlchemySlotWidget`
- Классификация через `FIngredientRegistry::IsWater`.
### `UItemTooltipWidget`
- Показывает **S_perceived**: искажённые значения параметров + реальные в скобках.
- Искажение через `Perception::PerceiveValue` (мультипликативное, bounded).
- `PerceiveClass` (подмена самого факта "что это за вещь") в коде не существует — искажаются только числа, не идентичность предмета; см. `ROADMAP.md`, "Механики с открытым дизайном".
- Глобальный Distortion из `AHerbalistPlayerController::CurrentGlobalDistortion`.
- Детерминированный seed (нет дрожания).
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