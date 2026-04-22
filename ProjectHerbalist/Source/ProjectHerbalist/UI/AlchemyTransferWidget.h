// AlchemyTransferWidget.h
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Core/Types/HerbalistCoreTypes.h"
#include "UI/AlchemySlotWidget.h"
#include "AlchemyTransferWidget.generated.h"

class UInventoryWidget;
class UButton;
class UTextBlock;
class UHerbalistInventoryComponent;

UCLASS()
class PROJECTHERBALIST_API UAlchemyTransferWidget : public UUserWidget
{
    GENERATED_BODY()

public:
	UAlchemySlotWidget* FindSuitableSlot(const FInventoryItem& Item) const;
    UAlchemyTransferWidget(const FObjectInitializer& ObjectInitializer);

    void BindInventory(UHerbalistInventoryComponent* InPlayerInventory);
    bool TryAddItemToSlot(const FInventoryItem& Item);

    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;
    virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

protected:
    UPROPERTY(meta = (BindWidget))
    UInventoryWidget* PlayerInventory;

    UPROPERTY(meta = (BindWidget))
    UAlchemySlotWidget* WaterSlot;

    UPROPERTY(meta = (BindWidget))
    UAlchemySlotWidget* IngredientSlot1;

    UPROPERTY(meta = (BindWidget))
    UAlchemySlotWidget* IngredientSlot2;

    UPROPERTY(meta = (BindWidget))
    UAlchemySlotWidget* IngredientSlot3;

    UPROPERTY(meta = (BindWidget))
    UAlchemySlotWidget* ResultSlot;

    UPROPERTY(meta = (BindWidget))
    UButton* MixButton;

    UPROPERTY(meta = (BindWidget))
    UTextBlock* StatusText;

    UFUNCTION()
    void OnMixClicked();

    UPROPERTY()
    UHerbalistInventoryComponent* PlayerInventoryComponent;

    bool CollectIngredients(TArray<FInventoryItem>& OutIngredients);
    void ClearIngredientSlots();
    void SetStatusMessage(const FString& Message);

    bool bIsMixing = false;
};