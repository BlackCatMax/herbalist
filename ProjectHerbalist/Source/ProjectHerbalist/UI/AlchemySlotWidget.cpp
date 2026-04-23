// AlchemySlotWidget.cpp
#include "UI/AlchemySlotWidget.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/Border.h"
#include "Player/HerbalistPlayerController.h"
#include "Core/Inventory/HerbalistInventoryComponent.h"
#include "Core/Data/IngredientRegistry.h"
#include "UI/ItemTooltipWidget.h"
#include "Core/Inventory/InventoryDragDropOperation.h"   // <-- добавлено

extern FText GeneratePotionName(const FRealState& State);

void UAlchemySlotWidget::InitializeSlot(EAlchemySlotType InType, int32 InMaxCount)
{
    SlotType = InType;
    MaxCount = InMaxCount;
    Clear();
}

bool UAlchemySlotWidget::CanAcceptItem(const FInventoryItem& Item) const
{
    // Если слот полон (не Result) – предмет не влезает
    if (SlotType != EAlchemySlotType::Result && bHasItem && Count >= MaxCount) return false;
    if (SlotType == EAlchemySlotType::Result) return false;

    // Вода?
    bool bItemIsWater = FIngredientRegistry::IsWater(Item.IngredientID);
    if (Item.IngredientID == FName(TEXT("Potion"))) bItemIsWater = false;

    if (SlotType == EAlchemySlotType::Water && !bItemIsWater) return false;
    if (SlotType == EAlchemySlotType::Ingredient && bItemIsWater) return false;
    return true;
}

bool UAlchemySlotWidget::AddItem(const FInventoryItem& Item, int32 Amount)
{
    if (SlotType != EAlchemySlotType::Result && !CanAcceptItem(Item))
        return false;
    if (Amount <= 0) return false;

    if (!bHasItem)
    {
        StoredItem = Item;
        bHasItem = true;
        Count = FMath::Min(Amount, MaxCount);
    }
    else
    {
        if (StoredItem.IngredientID != Item.IngredientID) return false;
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
    if (bHasItem)
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
    UInventoryDragDropOperation* DragOp = Cast<UInventoryDragDropOperation>(InOperation);
    if (!DragOp || !DragOp->SourceInventory)
        return false;

    AHerbalistPlayerController* HPC = Cast<AHerbalistPlayerController>(GetWorld()->GetFirstPlayerController());
    if (!HPC || !HPC->InventoryComponent)
        return false;

    // Строим предмет для переноса (одна единица)
    FInventoryItem ItemToMove;
    if (DragOp->bIsSplit)
    {
        ItemToMove = DragOp->SplitItem;
    }
    else
    {
        const FInventoryItem* SourceItem = DragOp->SourceInventory->GetSlot(DragOp->SourceIndex);
        if (!SourceItem) return false;
        ItemToMove = *SourceItem;
        ItemToMove.Count = 1;
    }

    if (!CanAcceptItem(ItemToMove))
        return false;

    // Пытаемся добавить в этот слот
    if (AddItem(ItemToMove, 1))
    {
        // Удаляем из исходного инвентаря
        if (DragOp->bIsSplit)
        {
            DragOp->bIsSplit = false; // сплит-предмет уже перенесён
        }
        else
        {
            DragOp->SourceInventory->RemoveItem(DragOp->SourceIndex, 1);
        }
        return true;
    }
    return false;
}

void UAlchemySlotWidget::NativeOnDragCancelled(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
    // Ничего не делаем
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
        if (ItemNameText) ItemNameText->SetText(FText::GetEmpty());
        if (CountText) CountText->SetText(FText::GetEmpty());
        return;
    }

    if (IconImage) IconImage->SetVisibility(ESlateVisibility::Visible);

    FString DisplayName;
    if (StoredItem.IngredientID == FName(TEXT("Potion")))
    {
        DisplayName = GeneratePotionName(StoredItem.State).ToString();
    }
    else if (StoredItem.IngredientID == FName(TEXT("Ash")))
    {
        DisplayName = TEXT("Зола");
    }
    else if (StoredItem.IngredientID == FName(TEXT("BoiledWater")))
    {
        DisplayName = TEXT("Кипячёная вода");
    }
    else if (StoredItem.IngredientID == FName(TEXT("Water")))
    {
        DisplayName = TEXT("Вода");
    }
    else
    {
        DisplayName = StoredItem.IngredientID.ToString();
    }

    if (ItemNameText) ItemNameText->SetText(FText::FromString(DisplayName));

    if (CountText)
    {
        if (Count > 1)
            CountText->SetText(FText::AsNumber(Count));
        else
            CountText->SetText(FText::GetEmpty());
    }
}