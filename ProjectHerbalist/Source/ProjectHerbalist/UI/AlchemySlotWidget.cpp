// AlchemySlotWidget.cpp
#include "UI/AlchemySlotWidget.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/Border.h"
#include "Player/HerbalistPlayerController.h"
#include "Core/Inventory/HerbalistInventoryComponent.h"
#include "Core/Subsystems/IngredientRegistrySubsystem.h"
#include "UI/ItemTooltipWidget.h"
#include "Core/Inventory/InventoryDragDropOperation.h"
#include "Core/Types/HerbalistNameUtils.h"   // <-- добавлено
#include "Core/Simulation/Private/PerceptionService.h"

void UAlchemySlotWidget::InitializeSlot(EAlchemySlotType InType, int32 InMaxCount)
{
    SlotType = InType;
    MaxCount = InMaxCount;
    Clear();
}

bool UAlchemySlotWidget::CanAcceptItem(const FInventoryItem& Item) const
{
    if (SlotType != EAlchemySlotType::Result && bHasItem && Count >= MaxCount) return false;
    if (SlotType == EAlchemySlotType::Result) return false;

    AHerbalistPlayerController* PC = Cast<AHerbalistPlayerController>(GetOwningPlayer());
    UIngredientRegistrySubsystem* IngredientSubsystem = nullptr;
    if (PC)
    {
        UGameInstance* GameInstance = PC->GetGameInstance();
        IngredientSubsystem = GameInstance ? GameInstance->GetSubsystem<UIngredientRegistrySubsystem>() : nullptr;
    }

    bool bItemIsWater = IngredientSubsystem ? IngredientSubsystem->IsWater(Item.IngredientID) : false;
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
        // Считаем один раз, на входе в слот — не при каждом UpdateDisplay,
        // иначе имя зелья/ингредиента мигало бы новым искажением на любой
        // перерисовке (ROADMAP.md §3.1: иллюзия должна быть устойчивой).
        PerceivedState = Simulation::FPerceptionService::PerceiveRealState(StoredItem.State, PerceptionRng);
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
    PerceivedState = FRealState();
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

    if (AddItem(ItemToMove, ItemToMove.Count))
    {
        if (DragOp->bIsSplit)
        {
            DragOp->bIsSplit = false;
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

    // Имя зелья зависит от State (доминирующая ось, Distortion/Purity) — должно
    // строиться по искажённому восприятию, а не по реальному составу (тот же
    // принцип, что уже в InventorySlotWidget::UpdateDisplay). Остальные
    // названия (Ash/BoiledWater/Water/из реестра) от State не зависят — там
    // нечему течь.
    FString DisplayName;
    if (StoredItem.IngredientID == FName(TEXT("Potion")))
    {
        DisplayName = GeneratePotionName(PerceivedState).ToString();
    }
    else
    {
        UIngredientRegistrySubsystem* IngSub = nullptr;
        if (AHerbalistPlayerController* PC = Cast<AHerbalistPlayerController>(GetOwningPlayer()))
        {
            if (PC->GetGameInstance())
                IngSub = PC->GetGameInstance()->GetSubsystem<UIngredientRegistrySubsystem>();
        }
        DisplayName = GetItemDisplayName(StoredItem, IngSub);
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