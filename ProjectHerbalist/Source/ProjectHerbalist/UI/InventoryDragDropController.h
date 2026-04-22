// InventoryDragDropController.h
#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Core/Types/HerbalistCoreTypes.h"
#include "InventoryDragDropController.generated.h"

class UHerbalistInventoryComponent;
class UInventoryDragDropOperation;
class UAlchemySlotWidget;

UCLASS()
class PROJECTHERBALIST_API UInventoryDragDropController : public UObject
{
    GENERATED_BODY()

public:
    // Попытаться переместить один предмет из исходного инвентаря в целевой
    UFUNCTION(BlueprintCallable, Category = "Inventory|DragDrop")
    static bool TryTransferItem(UHerbalistInventoryComponent* SourceInventory, int32 SourceIndex,
                                UHerbalistInventoryComponent* TargetInventory);

    // Попытаться добавить разделённую стопку в целевой инвентарь
    UFUNCTION(BlueprintCallable, Category = "Inventory|DragDrop")
    static bool TryAddSplitItem(const FInventoryItem& SplitItem,
                                UHerbalistInventoryComponent* SourceInventory,
                                UHerbalistInventoryComponent* TargetInventory,
                                UInventoryDragDropOperation* DragOp);

    // Попытаться добавить предмет в слот алхимии
    UFUNCTION(BlueprintCallable, Category = "Inventory|DragDrop")
    static bool TryAddToAlchemySlot(const FInventoryItem& Item, UAlchemySlotWidget* Slot,
                                    UHerbalistInventoryComponent* PlayerInventory);
};