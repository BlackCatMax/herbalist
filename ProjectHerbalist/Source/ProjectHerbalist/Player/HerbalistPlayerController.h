// Copyright Project Herbalist. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "InputActionValue.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Core/Inventory/HerbalistInventoryComponent.h"
#include "HerbalistPlayerController.generated.h"

UCLASS()
class PROJECTHERBALIST_API AHerbalistPlayerController : public APlayerController
{
    GENERATED_BODY()

public:
    AHerbalistPlayerController();

    virtual void SetupInputComponent() override;
    virtual void BeginPlay() override;

    // Компонент инвентаря (будет создан автоматически)
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory")
    UHerbalistInventoryComponent* InventoryComponent;

    // Команды для тестирования (консоль)
    UFUNCTION(Exec)
    void HarvestTest(int32 X, int32 Y);

    UFUNCTION(Exec)
    void ApplyTest(int32 X, int32 Y);

    UFUNCTION(Exec)
    void ShowInventory();

    UFUNCTION(Exec)
    void MassHarvestTest(int32 X, int32 Y, int32 Count);

protected:
    // Input mapping
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
    class UInputMappingContext* DefaultMappingContext;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
    class UInputAction* MoveAction;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
    class UInputAction* LookAction;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
    class UInputAction* HarvestAction;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
    class UInputAction* InfoAction;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
    class UInputAction* InventoryAction;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
    class UInputAction* ApplyAlchemyAction;

    void Move(const FInputActionValue& Value);
    void Look(const FInputActionValue& Value);
    void Harvest();
    void Info();
    void Inventory();
    void ApplyAlchemy();

    bool GetHitResultFromCamera(FHitResult& OutHit);
    void OnLeftClick();
    void OnRightClick();
    void OnApplyAlchemyKey();
};