// AlchemySlotWidget.h
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Core/Types/HerbalistCoreTypes.h"
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
    
    virtual FReply NativeOnMouseButtonDoubleClick(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
    virtual bool NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
    
protected:
    virtual void NativeConstruct() override;
    void UpdateDisplay();
    
    UPROPERTY(meta = (BindWidget))
    class UImage* IconImage;
    
    UPROPERTY(meta = (BindWidget))
    class UTextBlock* CountText;
    
    UPROPERTY(meta = (BindWidget))
    class UBorder* SlotBorder;
    
    EAlchemySlotType SlotType;
    FInventoryItem StoredItem;
    int32 Count = 0;
    int32 MaxCount = 1;
    bool bHasItem = false;
};