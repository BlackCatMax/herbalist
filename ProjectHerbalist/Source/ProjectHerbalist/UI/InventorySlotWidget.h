#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Core/Inventory/HerbalistInventoryComponent.h"
#include "InventorySlotWidget.generated.h"

class UImage;
class UTextBlock;
class UItemTooltipWidget;

UCLASS()
class PROJECTHERBALIST_API UInventorySlotWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    void InitializeSlot(int32 InIndex, const FInventoryItem& InItem, UHerbalistInventoryComponent* InInventory);
    void Refresh();
    void SetOtherInventory(UHerbalistInventoryComponent* InOther);

protected:
    virtual FReply NativeOnMouseButtonDoubleClick(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
    virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
    virtual void NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation) override;
    virtual bool NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
    virtual void NativeOnDragCancelled(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
    virtual void NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
    virtual void NativeOnMouseLeave(const FPointerEvent& InMouseEvent) override;
    virtual void NativeDestruct() override;

    UPROPERTY(meta = (BindWidget))
    UImage* ItemIcon;

    UPROPERTY(meta = (BindWidget))
    UTextBlock* ItemNameText;

    UPROPERTY(meta = (BindWidget))
    UTextBlock* CountText;

    UPROPERTY(meta = (BindWidget))
    UTextBlock* PerceptionText;

    UPROPERTY(EditDefaultsOnly, Category = "Tooltip")
    TSubclassOf<UItemTooltipWidget> TooltipWidgetClass;

private:
    int32 FindRealIndex() const;
    int32 SlotIndex = -1;

    UPROPERTY()
    UHerbalistInventoryComponent* InventoryComponent = nullptr;

    UPROPERTY()
    UHerbalistInventoryComponent* OtherInventory = nullptr;

    void UpdateDisplay();
    bool TryMoveToOtherInventory();

    FInventoryItem CachedItem;

    UPROPERTY()
    UItemTooltipWidget* ActiveTooltip;
};