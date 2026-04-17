// Copyright Project Herbalist. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Core/Types/HerbalistCoreTypes.h"
#include "HerbalistInventoryComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnInventoryChanged);

USTRUCT(BlueprintType)
struct FInventoryItem
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EResourceType Type = EResourceType::None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FRealState State;

    // В будущем добавим LastUpdateTime для порчи
};

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class PROJECTHERBALIST_API UHerbalistInventoryComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UHerbalistInventoryComponent();

    // Максимальное количество слотов
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory")
    int32 MaxSlots = 20;

    // Добавить предмет
    UFUNCTION(BlueprintCallable, Category = "Inventory")
    bool AddItem(const FRealState& State, EResourceType Type);

    // Удалить по индексу
    UFUNCTION(BlueprintCallable, Category = "Inventory")
    void RemoveItem(int32 Index);

    // Добавить в public секцию после RemoveItem
    UFUNCTION(BlueprintCallable, Category = "Inventory")
    void SwapItems(int32 IndexA, int32 IndexB);

    // Получить копию всех предметов
    UFUNCTION(BlueprintCallable, Category = "Inventory")
    TArray<FInventoryItem> GetItems() const { return Items; }

    // Очистить инвентарь
    UFUNCTION(BlueprintCallable, Category = "Inventory")
    void Clear();

    // Событие изменения инвентаря (для виджета)
    UPROPERTY(BlueprintAssignable, Category = "Inventory")
    FOnInventoryChanged OnInventoryChanged;

protected:
    UPROPERTY()
    TArray<FInventoryItem> Items;
};