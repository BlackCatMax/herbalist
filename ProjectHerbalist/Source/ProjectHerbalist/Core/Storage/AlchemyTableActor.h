#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/Types/HerbalistCoreTypes.h"
#include "AlchemyTableActor.generated.h"

UCLASS()
class PROJECTHERBALIST_API AAlchemyTableActor : public AActor
{
    GENERATED_BODY()

public:
    AAlchemyTableActor();

    UFUNCTION(BlueprintCallable, Category = "Herbalist|Alchemy")
    FIntPoint GetGridCoords() const { return GridCoords; }

    void OnInteract(class AHerbalistPlayerController* PlayerController);

    void SetSlotItem(int32 SlotIndex, const FInventoryItem& Item);
    FInventoryItem GetSlotItem(int32 SlotIndex) const;
    void ClearSlot(int32 SlotIndex);
    TArray<FInventoryItem> GetIngredientsForCraft() const;

    void UpdateGridCoords();

protected:
    UPROPERTY()
    FIntPoint GridCoords = FIntPoint(-1, -1);

    UPROPERTY()
    FInventoryItem StoredWater;

    UPROPERTY()
    TArray<FInventoryItem> StoredIngredients;
};