#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Core/Inventory/HerbalistInventoryComponent.h"
#include "InventorySlotWidget.generated.h"

class UImage;
class UTextBlock;
class UHerbalistInventoryComponent;
class UInventoryDragDropOperation;

UCLASS()
class PROJECTHERBALIST_API UInventorySlotWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    void InitializeSlot(int32 InIndex, const FInventoryItem& InItem, UHerbalistInventoryComponent* InInventory);
    void Refresh();

protected:
    virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
    virtual void NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation) override;
    virtual bool NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;

    UPROPERTY(meta = (BindWidget))
    UImage* ItemIcon;

    UPROPERTY(meta = (BindWidget))
    UTextBlock* ItemNameText;

private:
    int32 SlotIndex = -1;
    UPROPERTY()
    UHerbalistInventoryComponent* InventoryComponent = nullptr;

    void UpdateDisplay();
    bool TryMoveItem(UInventoryDragDropOperation* DragOp);

    FInventoryItem CachedItem;
};