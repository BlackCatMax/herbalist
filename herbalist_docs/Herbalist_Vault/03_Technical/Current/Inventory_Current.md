---
tags: [technical, current, inventory]
---
# Инвентарь — реализация (обновлено: Фазы 1-4)
## Компонент `UHerbalistInventoryComponent`
- Хранит `TArray<FInventoryItem>`.
- Максимальный размер стека — `MAX_STACK_SIZE = 9`.
- `MaxSlots` — общее количество слотов (по умолчанию 20).
## `FInventoryItem`

struct FInventoryItem {
    EResourceType Type;
    FRealState State;
    int32 Count;
};

## Стекирование

Два предмета считаются стекуемыми, если:

- Одинаковый `Type`.
    
- Различия по ключевым параметрам не превышают порогов:
    
    - `Magnitude`: 0.15
        
    - `Distortion`: 0.2
        
    - `Purity`, `Stability`: 0.2
        
    - Евклидово расстояние `Direction` < 0.3
        

При слиянии стопок параметры усредняются с весами, пропорциональными количеству. Дополнительно:

- `Distortion` увеличивается на 15% от разницы.
    
- `Purity` уменьшается на 10% от разницы.
    

## Операции

- `AddItem` — пытается добавить в существующую стопку или создаёт новую.
    
- `RemoveItem` — уменьшает счётчик, удаляет слот при Count=0.
    
- `TransferItemTo` — перенос 1 единицы в другой инвентарь.
    
- `SplitStack` — разделение стопки (используется при Shift+Drag).
    

## Drag & Drop

- `UInventoryDragDropOperation` хранит `SourceInventory`, `SourceIndex`, `bIsSplit`, `SplitItem`.
    
- При обычном перетаскивании перемещается вся стопка.
    
- При Shift — создаётся разделённая стопка (половина).
    
- Отмена drag'а возвращает предмет в исходный инвентарь.
    

## UI

- `UInventoryWidget` — контейнер слотов, подписывается на `OnInventoryChanged`.
    
- `UInventorySlotWidget` — отображает иконку, название, количество, обрабатывает drag&drop и тултип.
    
- `UItemTooltipWidget` — показывает **[[S_perceived]]** (искажённые значения + реальные в скобках). Использует `Perception::PerceiveValue`. `PerceiveClass` (подмена идентичности предмета) в коде не существует — см. `ROADMAP.md`. Глобальный Distortion из HerbalistPlayerController::CurrentGlobalDistortion.