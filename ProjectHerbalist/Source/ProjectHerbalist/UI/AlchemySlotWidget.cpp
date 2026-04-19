// AlchemySlotWidget.cpp
#include "UI/AlchemySlotWidget.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/Border.h"
#include "Player/HerbalistPlayerController.h"
#include "Core/Inventory/HerbalistInventoryComponent.h"
#include "Core/Harvest/HerbalistHarvest.h"

void UAlchemySlotWidget::InitializeSlot(EAlchemySlotType InType, int32 InMaxCount)
{
    SlotType = InType;
    MaxCount = InMaxCount;
    Clear();
}

bool UAlchemySlotWidget::CanAcceptItem(const FInventoryItem& Item) const
{
    if (bHasItem && Count >= MaxCount) return false;
    if (SlotType == EAlchemySlotType::Result) return false;
    if (SlotType == EAlchemySlotType::Water && Item.Type != EResourceType::Water) return false;
    if (SlotType == EAlchemySlotType::Ingredient && Item.Type == EResourceType::Water) return false;
    return true;
}

bool UAlchemySlotWidget::AddItem(const FInventoryItem& Item, int32 Amount)
{
    if (!CanAcceptItem(Item) || Amount <= 0) return false;
    
    if (!bHasItem)
    {
        StoredItem = Item;
        bHasItem = true;
        Count = FMath::Min(Amount, MaxCount);
    }
    else
    {
        if (StoredItem.Type != Item.Type) return false;
        Count = FMath::Min(Count + Amount, MaxCount);
    }
    UpdateDisplay();
    return true;
}

bool UAlchemySlotWidget::RemoveItem(int32 Amount)
{
    if (!bHasItem || Amount <= 0) return false;
    
    Count -= Amount;
    if (Count <= 0)
    {
        Clear();
    }
    else
    {
        UpdateDisplay();
    }
    return true;
}

void UAlchemySlotWidget::Clear()
{
    bHasItem = false;
    Count = 0;
    StoredItem = FInventoryItem();
    UpdateDisplay();
}

FReply UAlchemySlotWidget::NativeOnMouseButtonDoubleClick(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    if (bHasItem && SlotType != EAlchemySlotType::Result)
    {
        APlayerController* PC = GetOwningPlayer();
        AHerbalistPlayerController* HPC = Cast<AHerbalistPlayerController>(PC);
        if (HPC && HPC->InventoryComponent)
        {
            FInventoryItem ItemToAdd = StoredItem;
            ItemToAdd.Count = 1;
            if (HPC->InventoryComponent->AddItem(ItemToAdd, 1))
            {
                RemoveItem(1);
            }
        }
    }
    return FReply::Handled();
}

bool UAlchemySlotWidget::NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
    // Для простоты пока не реализуем перетаскивание, только двойной клик
    return false;
}

void UAlchemySlotWidget::NativeConstruct()
{
    Super::NativeConstruct();
    Clear();
}

void UAlchemySlotWidget::UpdateDisplay()
{
    if (!bHasItem || Count == 0)
    {
        if (IconImage) IconImage->SetVisibility(ESlateVisibility::Hidden);
        if (CountText) CountText->SetText(FText::GetEmpty());
        return;
    }
    
    if (IconImage)
    {
        IconImage->SetVisibility(ESlateVisibility::Visible);
        // TODO: установить иконку из DataAsset (через FHerbalistHarvest::GetResourceName или прямой доступ)
    }
    
    if (CountText)
    {
        if (Count > 1)
            CountText->SetText(FText::AsNumber(Count));
        else
            CountText->SetText(FText::GetEmpty());
    }
}