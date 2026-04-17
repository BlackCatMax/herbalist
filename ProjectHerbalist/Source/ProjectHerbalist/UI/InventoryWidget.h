#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Core/Inventory/InventoryDragDropOperation.h"
#include "InventoryWidget.generated.h"

class UHerbalistInventoryComponent;
class UInventorySlotWidget;
class UVerticalBox;

UCLASS()
class PROJECTHERBALIST_API UInventoryWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    void BindInventory(UHerbalistInventoryComponent* InInventory);

protected:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;

    UFUNCTION()
    void OnInventoryChanged();

    UPROPERTY(meta = (BindWidget))
    UVerticalBox* SlotContainer;

    UPROPERTY(EditDefaultsOnly, Category = "Inventory")
    TSubclassOf<UInventorySlotWidget> SlotWidgetClass;

private:
    UPROPERTY()
    UHerbalistInventoryComponent* InventoryComponent = nullptr;

    void RefreshInventoryDisplay();
    void ClearSlots();
};