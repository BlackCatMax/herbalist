#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Core/Inventory/HerbalistInventoryComponent.h"
#include "InventoryWidget.generated.h"

class UInventorySlotWidget;
class UVerticalBox; // или UVerticalBox, замените при необходимости

UCLASS()
class PROJECTHERBALIST_API UInventoryWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    void BindInventory(UHerbalistInventoryComponent* InInventory);
    UHerbalistInventoryComponent* GetInventoryComponent() const { return InventoryComponent; }
    // SetOtherInventory удалена 2026-09-02 (чистка мёртвого кода) — объявление
    // без единой реализации в проекте (линковка упала бы на первом же вызове),
    // никто не звал. Перенос между инвентарями идёт через drag-n-drop
    // (NativeOnDrop ниже) и AlchemyTransferWidget.

protected:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;
    virtual bool NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;

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
