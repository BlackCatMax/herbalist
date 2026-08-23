// HerbalistInventoryComponent.h
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Core/Types/HerbalistCoreTypes.h"
#include "Math/RandomStream.h"
#include "HerbalistInventoryComponent.generated.h"

struct FInventorySnapshot;
struct FStateDelta;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnInventoryChanged);

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class PROJECTHERBALIST_API UHerbalistInventoryComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UHerbalistInventoryComponent();
	void ApplyStateDelta(const FStateDelta& Delta);

    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

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
	
	FInventorySnapshot CaptureState() const;

    UPROPERTY(BlueprintAssignable, Category = "Inventory")
    FOnInventoryChanged OnInventoryChanged;

protected:
    UPROPERTY()
    TArray<FInventoryItem> Items;

    int32 FindStackableSlot(const FInventoryItem& Item) const;
    bool AreItemsStackable(const FInventoryItem& A, const FInventoryItem& B) const;
    void MergeStack(FInventoryItem& Target, const FInventoryItem& Source, int32 AddedCount);

    // Применить порчу к одному предмету (если bSubjectToDecay == true)
    void ApplyDecayToItem(FInventoryItem& Item, float DeltaTime, float DecayRate);

    // Периодичность обновления (в секундах)
    static constexpr float DecayUpdateInterval = 1.0f;
    float TimeSinceLastDecayUpdate = 0.0f;

    // Джиттер осей при порче раньше брался из глобального FMath::FRandRange —
    // недетерминированно, не воспроизводимо по сиду (тот же класс бага, что и
    // у спавна ресурсов, AUDIT_AND_REFACTORING_PLAN §1.3/META_AUDIT §1.1).
    // Сид фиксированный, не WorldRNG: инвентарь — состояние актора-владельца,
    // не сетки мира, но детерминизм внутри одной сессии всё равно нужен для
    // трассировки/реплея.
    FRandomStream DecayRng = FRandomStream(20260823);
};