#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/Inventory/HerbalistInventoryComponent.h"
#include "StorageContainer.generated.h"

UCLASS()
class PROJECTHERBALIST_API AStorageContainer : public AActor
{
    GENERATED_BODY()

public:
    AStorageContainer();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Storage")
    UHerbalistInventoryComponent* InventoryComponent;

    // Пункт 4.4: настраиваемая вместимость
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Storage")
    int32 MaxSlots = 20;

    void OnInteract(APlayerController* PlayerController);

protected:
    virtual void BeginPlay() override;

    UPROPERTY(EditDefaultsOnly, Category = "UI")
    TSubclassOf<class UInventoryTransferWidget> TransferWidgetClass;

    UPROPERTY()
    class UInventoryTransferWidget* TransferWidgetInstance = nullptr;
};