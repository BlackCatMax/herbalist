#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/Inventory/HerbalistInventoryComponent.h"
#include "Core/Interaction/Interactable.h"
#include "StorageContainer.generated.h"

UCLASS()
class PROJECTHERBALIST_API AStorageContainer : public AActor, public IInteractable
{
    GENERATED_BODY()

public:
    AStorageContainer();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Storage")
    UHerbalistInventoryComponent* InventoryComponent;

    // Пункт 4.4: настраиваемая вместимость
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Storage")
    int32 MaxSlots = 20;

    virtual void OnInteract_Implementation(AHerbalistPlayerController* PC) override;

protected:
    virtual void BeginPlay() override;

    UPROPERTY(EditDefaultsOnly, Category = "UI")
    TSubclassOf<class UInventoryTransferWidget> TransferWidgetClass;

    UPROPERTY()
    class UInventoryTransferWidget* TransferWidgetInstance = nullptr;
};