// HerbalistPlayerController.h
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "InputActionValue.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Core/Inventory/HerbalistInventoryComponent.h"
#include "Core/Journal/HerbalistJournalComponent.h"
#include "UI/InventoryWidget.h"
#include "UI/AlchemyTransferWidget.h"
#include "UI/JournalWidget.h"
#include "HerbalistPlayerController.generated.h"

class AStorageContainer;
class UInventoryTransferWidget;
class AAlchemyTableActor;
class AGridWorldManager;
class AHerbalistResourceActor;

UCLASS()
class PROJECTHERBALIST_API AHerbalistPlayerController : public APlayerController
{
    GENERATED_BODY()

public:
    AHerbalistPlayerController();
	
	virtual void SetupInputComponent() override;
    virtual void BeginPlay() override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory")
    UHerbalistInventoryComponent* InventoryComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Journal")
    UHerbalistJournalComponent* JournalComponent;

    UPROPERTY(BlueprintReadOnly, Category = "UI")
    bool bIsAnyWidgetOpen = false;

    UPROPERTY()
    UInventoryTransferWidget* CurrentTransferWidget = nullptr;

    UPROPERTY()
    UAlchemyTransferWidget* CurrentAlchemyWidget = nullptr;

    UPROPERTY()
    AAlchemyTableActor* CurrentAlchemyTable = nullptr;
	
    UPROPERTY(BlueprintReadOnly, Category = "Alchemy")
    float CurrentGlobalDistortion = 0.3f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Herbalist|Harvesting")
    float MaxHarvestDistance = 200.0f;

    UFUNCTION(BlueprintCallable, Category = "UI")
    void CloseAnyWidget();

    UFUNCTION(Exec)
    void HarvestTest(int32 X, int32 Y);
    UFUNCTION(Exec)
    void ApplyTest(int32 X, int32 Y);
    UFUNCTION(Exec)
    void ShowInventory();
    UFUNCTION(Exec)
    void MassHarvestTest(int32 X, int32 Y, int32 Count);
	
	UFUNCTION(Exec)
	void TestNewHarvest(int32 X, int32 Y, FName IngredientID);

    UFUNCTION(Exec, BlueprintCallable, Category = "Alchemy")
    void UsePotion();
    
    UFUNCTION(BlueprintCallable, Category = "Herbalist|Harvesting")
    bool CanHarvestActor(AActor* TargetActor) const;
	
    AGridWorldManager* FindWorldManager() const;
    void GetCellFromHit(const FHitResult& Hit, int32& OutX, int32& OutY) const;
    void UpdateDistortionFromCell(int32 X, int32 Y);
	
    UFUNCTION(Exec)
    void TestNewTransfer(FName IngredientID, int32 Amount);

    UFUNCTION(Exec)
    void TestNewApply(int32 X, int32 Y, FString IngredientList);

    // Сохранения v1 (Core/Save/HerbalistSaveSubsystem.h) — тонкие обёртки над
    // подсистемой, тем же паттерном, что HarvestTest/ApplyTest над пайплайном.
    UFUNCTION(Exec)
    void SaveGame();

    UFUNCTION(Exec)
    void LoadGame();

    // Экран Травника (UI/JournalWidget.h, 07_UX §7.2.4) — открывается тем же
    // способом, что и инвентарь (Inventory()). Exec-обёртка отдельно от
    // JournalAction — тестируемо консолью до того, как в редакторе назначены
    // сам Input Action asset и WBP-блюпринт (оба нужно создать в редакторе,
    // C++ этого не может — тот же паттерн, что InventoryWidgetClass/InventoryAction).
    UFUNCTION(Exec)
    void ToggleJournalUI();

protected:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
    UInputMappingContext* DefaultMappingContext;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
    UInputAction* MoveAction;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
    UInputAction* LookAction;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
    UInputAction* HarvestAction;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
    UInputAction* InfoAction;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
    UInputAction* InventoryAction;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
    UInputAction* ApplyAlchemyAction;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
    UInputAction* InteractAction;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
    UInputAction* JournalAction;

    UPROPERTY(EditDefaultsOnly, Category = "UI")
    TSubclassOf<UInventoryWidget> InventoryWidgetClass;

    UPROPERTY(EditDefaultsOnly, Category = "UI")
    TSubclassOf<UJournalWidget> JournalWidgetClass;

    void Move(const FInputActionValue& Value);
    void Look(const FInputActionValue& Value);
    void Harvest();
    void Info();
    void Inventory();
    void Journal();
    void ApplyAlchemy();
    void Interact();

    bool GetHitResultFromCamera(FHitResult& OutHit);
    void OnLeftClick();
    void OnRightClick();
    void OnApplyAlchemyKey();

    void OnUsePotion();

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
    UInputAction* UsePotionAction;

private:
    UPROPERTY()
    UInventoryWidget* InventoryWidgetInstance = nullptr;

    UPROPERTY()
    UJournalWidget* JournalWidgetInstance = nullptr;

    // Кэш для мира
    UPROPERTY()
    AGridWorldManager* CachedWorldManager = nullptr;

    bool TryHarvestResource(AHerbalistResourceActor* Resource);
};