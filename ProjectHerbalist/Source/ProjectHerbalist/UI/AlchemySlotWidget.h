// AlchemySlotWidget.h
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Core/Types/HerbalistCoreTypes.h"
#include "Math/RandomStream.h"
#include "AlchemySlotWidget.generated.h"

UENUM(BlueprintType)
enum class EAlchemySlotType : uint8
{
    Water,
    Ingredient,
    Result
};

UCLASS()
class PROJECTHERBALIST_API UAlchemySlotWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    void InitializeSlot(EAlchemySlotType InType, int32 InMaxCount = 1);

    bool CanAcceptItem(const FInventoryItem& Item) const;
    bool AddItem(const FInventoryItem& Item, int32 Amount = 1);
    bool RemoveItem(int32 Amount = 1);
    void Clear();

    const FInventoryItem* GetItem() const { return bHasItem ? &StoredItem : nullptr; }
    int32 GetCount() const { return Count; }
    EAlchemySlotType GetSlotType() const { return SlotType; }

    // Искажённая версия StoredItem.State, посчитанная один раз при AddItem —
    // тот же экземпляр, что видит сам слот в UpdateDisplay. Наружу, чтобы
    // AlchemyTransferWidget мог сослаться на неё в статусной строке, а не
    // считать восприятие заново своим RNG (иначе цифры в слоте и в тосте
    // разъехались бы — два независимых броска шума на одно и то же событие).
    const FRealState& GetPerceivedState() const { return PerceivedState; }

    virtual FReply NativeOnMouseButtonDoubleClick(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
    virtual bool NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
    virtual void NativeOnDragCancelled(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;

protected:
    virtual void NativeConstruct() override;
    void UpdateDisplay();

    UPROPERTY(meta = (BindWidgetOptional))
    class UImage* IconImage;

    UPROPERTY(meta = (BindWidgetOptional))
    class UTextBlock* ItemNameText;

    UPROPERTY(meta = (BindWidgetOptional))
    class UTextBlock* CountText;

    EAlchemySlotType SlotType;
    FInventoryItem StoredItem;
    int32 Count = 0;
    int32 MaxCount = 1;
    bool bHasItem = false;

    // Искажённая версия StoredItem.State для отображения (07_UX: котёл не должен
    // раскрывать S_real). Отдельный фиксированный сид, не WorldRNG — предметы
    // здесь уже вне мира/инвентарного контейнера, у котла нет доступа к живому
    // потоку AGridWorldManager (protected), а плодить публичный геттер ради этого
    // не хотелось. Тот же принцип, что HerbalistInventoryComponent::DecayRng:
    // детерминированность самого по себе шума, не завязанная на симуляцию.
    FRandomStream PerceptionRng = FRandomStream(20260824);
    FRealState PerceivedState;
};