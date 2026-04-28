// HerbalistPlayerController.h
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "InputActionValue.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Core/Inventory/HerbalistInventoryComponent.h"
#include "UI/InventoryWidget.h"
#include "UI/AlchemyTransferWidget.h"
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

    UPROPERTY(EditDefaultsOnly, Category = "UI")
    TSubclassOf<UInventoryWidget> InventoryWidgetClass;

    void Move(const FInputActionValue& Value);
    void Look(const FInputActionValue& Value);
    void Harvest();
    void Info();
    void Inventory();
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

    // Кэш для мира
    UPROPERTY()
    AGridWorldManager* CachedWorldManager = nullptr;

    bool TryHarvestResource(AHerbalistResourceActor* Resource);
};