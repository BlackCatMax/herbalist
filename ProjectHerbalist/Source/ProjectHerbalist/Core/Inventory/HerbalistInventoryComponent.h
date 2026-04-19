#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Core/Types/HerbalistCoreTypes.h"
#include "HerbalistInventoryComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnInventoryChanged);

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class PROJECTHERBALIST_API UHerbalistInventoryComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UHerbalistInventoryComponent();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory")
    int32 MaxSlots = 20;

    static constexpr int32 MAX_STACK_SIZE = 9;

    UFUNCTION(BlueprintCallable, Category = "Inventory")
    bool AddItem(const FInventoryItem& Item, int32 Amount = 1);

    UFUNCTION(BlueprintCallable, Category = "Inventory")
    bool RemoveItem(int32 Index, int32 Amount = 1);

    UFUNCTION(BlueprintCallable, Category = "Inventory")
    bool TransferOneItem(int32 SourceIndex, int32 TargetIndex);

    UFUNCTION(BlueprintCallable, Category = "Inventory")
    bool TransferItemTo(int32 SourceIndex, UHerbalistInventoryComponent* TargetInventory);

    // Разделить стопку
    UFUNCTION(BlueprintCallable, Category = "Inventory")
    bool SplitStack(int32 Index, int32 Amount, FInventoryItem& OutItem);

    UFUNCTION(BlueprintCallable, Category = "Inventory")
    TArray<FInventoryItem> GetItems() const { return Items; }

    int32 GetNumSlots() const { return Items.Num(); }

    const FInventoryItem* GetSlot(int32 Index) const;

    UFUNCTION(BlueprintCallable, Category = "Inventory")
    void Clear();

    UPROPERTY(BlueprintAssignable, Category = "Inventory")
    FOnInventoryChanged OnInventoryChanged;

protected:
    UPROPERTY()
    TArray<FInventoryItem> Items;

    int32 FindStackableSlot(const FInventoryItem& Item) const;
    bool AreItemsStackable(const FInventoryItem& A, const FInventoryItem& B) const;
    void MergeStack(FInventoryItem& Target, const FInventoryItem& Source, int32 AddedCount);
};