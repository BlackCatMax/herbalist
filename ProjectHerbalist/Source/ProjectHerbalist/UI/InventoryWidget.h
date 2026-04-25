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
    void SetOtherInventory(UHerbalistInventoryComponent* InOther);

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

    UPROPERTY()
    UHerbalistInventoryComponent* OtherInventory = nullptr;

    void RefreshInventoryDisplay();
    void ClearSlots();
};
