// AlchemySlotWidget.cpp
#include "UI/AlchemySlotWidget.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/Border.h"
#include "Player/HerbalistPlayerController.h"
#include "Core/Inventory/HerbalistInventoryComponent.h"
#include "Core/Harvest/HerbalistHarvest.h"
#include "Core/Types/HerbalistIngredient.h"
#include "Engine/AssetManager.h"

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

    // Проверяем, является ли предмет водой
    bool bItemIsWater = false;
    if (Item.IngredientID == FName(TEXT("Water")))
    {
        bItemIsWater = true;
    }
    else if (Item.IngredientID != FName(TEXT("Potion")) && !Item.IngredientID.IsNone())
    {
        FPrimaryAssetId AssetId = FPrimaryAssetId(UHerbalistIngredient::StaticClass()->GetFName(), Item.IngredientID);
        UHerbalistIngredient* Ingredient = Cast<UHerbalistIngredient>(UAssetManager::Get().GetPrimaryAssetObject(AssetId));
        if (Ingredient)
        {
            bItemIsWater = Ingredient->bIsWater;
        }
    }

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
        if (ItemNameText) ItemNameText->SetText(FText::GetEmpty());
        if (CountText) CountText->SetText(FText::GetEmpty());
        return;
    }

    if (IconImage) IconImage->SetVisibility(ESlateVisibility::Visible);

    FString DisplayName;
    if (StoredItem.IngredientID == FName(TEXT("Potion")))
    {
        DisplayName = TEXT("Зелье");
    }
    else if (StoredItem.IngredientID == FName(TEXT("Water")))
    {
        DisplayName = TEXT("Вода");
    }
    else
    {
        FPrimaryAssetId AssetId = FPrimaryAssetId(UHerbalistIngredient::StaticClass()->GetFName(), StoredItem.IngredientID);
        UHerbalistIngredient* Ingredient = Cast<UHerbalistIngredient>(UAssetManager::Get().GetPrimaryAssetObject(AssetId));
        if (Ingredient)
        {
            DisplayName = Ingredient->DisplayName.ToString();
        }
        else
        {
            DisplayName = StoredItem.IngredientID.ToString();
        }
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