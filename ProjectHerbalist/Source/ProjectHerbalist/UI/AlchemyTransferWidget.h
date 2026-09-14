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
class AGridWorldManager;

UCLASS()
class PROJECTHERBALIST_API UAlchemyTransferWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    UAlchemyTransferWidget(const FObjectInitializer& ObjectInitializer);

    void BindInventory(UHerbalistInventoryComponent* InPlayerInventory);
    // TryAddItemToSlot удалена 2026-09-02 (чистка мёртвого кода) — «положить
    // предмет в первый подходящий слот» не звалось ниоткуда; реальный путь
    // предмета в стол идёт через FindSuitableSlot ниже + drag-n-drop слота.
    UAlchemySlotWidget* FindSuitableSlot(const FInventoryItem& Item) const;

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

    // Витрина котла (2026-09-14): слот результата показывает предмет из
    // AGridWorldManager::OnBrewCompleted, сам предмет уже в сумке. Раньше
    // окно искало зелье в сумке по времени создания и промахивалось, когда
    // оно сливалось с похожей стопкой.
    void HandleBrewCompleted(const FInventoryItem& Produced);

    // Варки из этого окна, ещё не вернувшие результат: чужой крафт в том же
    // тике витрину не трогает.
    int32 PendingBrewCount = 0;

    TWeakObjectPtr<AGridWorldManager> BoundWorldManager;
    FDelegateHandle BrewCompletedHandle;
};