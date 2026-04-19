#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Core/Inventory/HerbalistInventoryComponent.h"
#include "InventorySlotWidget.generated.h"

class UImage;
class UTextBlock;

UCLASS()
class PROJECTHERBALIST_API UInventorySlotWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    void InitializeSlot(int32 InIndex, const FInventoryItem& InItem, UHerbalistInventoryComponent* InInventory);
    void Refresh();

protected:
    virtual FReply NativeOnMouseButtonDoubleClick(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

    UPROPERTY(meta = (BindWidget))
    UImage* ItemIcon;

    UPROPERTY(meta = (BindWidget))
    UTextBlock* ItemNameText;

    UPROPERTY(meta = (BindWidget))
    UTextBlock* CountText;

private:
	int32 FindRealIndex() const;
    int32 SlotIndex = -1;
    UPROPERTY()
    UHerbalistInventoryComponent* InventoryComponent = nullptr;

    void UpdateDisplay();
    bool TryMoveToOtherInventory();

    FInventoryItem CachedItem;
};