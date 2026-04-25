#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "InventoryTransferWidget.generated.h"

class UHerbalistInventoryComponent;
class UInventoryWidget;
class UVerticalBox;

UCLASS()
class PROJECTHERBALIST_API UInventoryTransferWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    void BindInventories(UHerbalistInventoryComponent* InLeftInventory, UHerbalistInventoryComponent* InRightInventory);

    UFUNCTION(BlueprintCallable, Category = "Inventory")
    UHerbalistInventoryComponent* GetLeftInventory() const;

    UFUNCTION(BlueprintCallable, Category = "Inventory")
    UHerbalistInventoryComponent* GetRightInventory() const;

protected:
    UPROPERTY(meta = (BindWidget))
    UVerticalBox* LeftInventoryContainer;

    UPROPERTY(meta = (BindWidget))
    UVerticalBox* RightInventoryContainer;

    UPROPERTY(EditDefaultsOnly, Category = "Inventory")
    TSubclassOf<UInventoryWidget> InventoryWidgetClass;

private:
    UPROPERTY()
    UInventoryWidget* LeftInventoryWidget = nullptr;

    UPROPERTY()
    UInventoryWidget* RightInventoryWidget = nullptr;
};
