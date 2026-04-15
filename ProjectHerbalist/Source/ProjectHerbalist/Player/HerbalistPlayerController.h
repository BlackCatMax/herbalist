#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "InputActionValue.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "HerbalistPlayerController.generated.h"

UCLASS()
class PROJECTHERBALIST_API AHerbalistPlayerController : public APlayerController
{
    GENERATED_BODY()

public:
    virtual void SetupInputComponent() override;
    virtual void BeginPlay() override;

    // Дальность луча для взаимодействия с ячейками
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
    float TraceDistance = 500.0f;

    // Exec-команды (консоль)
    UFUNCTION(Exec)
    void HarvestTest(int32 X, int32 Y, int32 ResourceType);

    UFUNCTION(Exec)
    void ApplyTest(int32 X, int32 Y);

    UFUNCTION(Exec)
    void ShowInventory();

    UFUNCTION(Exec)
    void MassHarvestTest(int32 X, int32 Y, int32 ResourceType, int32 Count);

protected:
    // Enhanced Input Assets
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
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
    class UInputAction* SelectResource1Action;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
    class UInputAction* SelectResource2Action;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
    class UInputAction* SelectResource3Action;

    // Callbacks
    void Move(const FInputActionValue& Value);
    void Look(const FInputActionValue& Value);
    void Harvest();
    void Info();
    void Inventory();
    void ApplyAlchemy();
    void SelectResource1();
    void SelectResource2();
    void SelectResource3();

    // Вспомогательные методы
    bool GetHitResultFromCamera(FHitResult& OutHit);
    void OnLeftClick();          // вызывается из Harvest()
    void OnRightClick();         // вызывается из Info()
    void OnApplyAlchemyKey();    // вызывается из ApplyAlchemy()

    int32 CurrentResourceType = 0; // 0 = крапива, 1 = папоротник, 2 = мухомор
};