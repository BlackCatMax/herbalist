// AlchemyTransferWidget.cpp
#include "UI/AlchemyTransferWidget.h"
#include "UI/InventoryWidget.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Player/HerbalistPlayerController.h"
#include "Core/World/GridWorldManager.h"
#include "Core/BiomeGraph/BiomeGraphSubsystem.h"
#include "Core/Types/BiomeTypes.h"
#include "Core/Storage/AlchemyTableActor.h"
#include "Core/Subsystems/IngredientRegistrySubsystem.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "ProjectHerbalist.h"

UAlchemyTransferWidget::UAlchemyTransferWidget(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    SetIsFocusable(true);
}

void UAlchemyTransferWidget::BindInventory(UHerbalistInventoryComponent* InPlayerInventory)
{
    PlayerInventoryComponent = InPlayerInventory;
    if (PlayerInventory)
    {
        PlayerInventory->BindInventory(PlayerInventoryComponent);
    }
}

bool UAlchemyTransferWidget::TryAddItemToSlot(const FInventoryItem& Item)
{
    if (WaterSlot->CanAcceptItem(Item) && WaterSlot->AddItem(Item, 1))
        return true;
    if (IngredientSlot1->CanAcceptItem(Item) && IngredientSlot1->AddItem(Item, 1))
        return true;
    if (IngredientSlot2->CanAcceptItem(Item) && IngredientSlot2->AddItem(Item, 1))
        return true;
    if (IngredientSlot3->CanAcceptItem(Item) && IngredientSlot3->AddItem(Item, 1))
        return true;
    return false;
}

UAlchemySlotWidget* UAlchemyTransferWidget::FindSuitableSlot(const FInventoryItem& Item) const
{
    if (WaterSlot && WaterSlot->CanAcceptItem(Item)) return WaterSlot;
    if (IngredientSlot1 && IngredientSlot1->CanAcceptItem(Item)) return IngredientSlot1;
    if (IngredientSlot2 && IngredientSlot2->CanAcceptItem(Item)) return IngredientSlot2;
    if (IngredientSlot3 && IngredientSlot3->CanAcceptItem(Item)) return IngredientSlot3;
    return nullptr;
}

void UAlchemyTransferWidget::NativeConstruct()
{
    Super::NativeConstruct();
    if (MixButton)
    {
        MixButton->OnClicked.AddDynamic(this, &UAlchemyTransferWidget::OnMixClicked);
    }
    WaterSlot->InitializeSlot(EAlchemySlotType::Water, 1);
    IngredientSlot1->InitializeSlot(EAlchemySlotType::Ingredient, 9);
    IngredientSlot2->InitializeSlot(EAlchemySlotType::Ingredient, 9);
    IngredientSlot3->InitializeSlot(EAlchemySlotType::Ingredient, 9);
    ResultSlot->InitializeSlot(EAlchemySlotType::Result, 1);
    SetKeyboardFocus();
}

void UAlchemyTransferWidget::NativeDestruct()
{
    if (MixButton)
    {
        MixButton->OnClicked.RemoveDynamic(this, &UAlchemyTransferWidget::OnMixClicked);
    }
    Super::NativeDestruct();
}

void UAlchemyTransferWidget::OnMixClicked()
{
    // ВРЕМЕННО ОТКЛЮЧЕНО (старый пайплайн удалён)
    SetStatusMessage(TEXT("Алхимия временно недоступна"));
}

bool UAlchemyTransferWidget::CollectIngredients(TArray<FInventoryItem>& OutIngredients)
{
    OutIngredients.Empty();
    auto AddIfPresent = [&](UAlchemySlotWidget* InSlot)
    {
        if (InSlot && InSlot->GetItem() && InSlot->GetCount() > 0)
        {
            FInventoryItem Item = *InSlot->GetItem();
            Item.Count = InSlot->GetCount();
            OutIngredients.Add(Item);
        }
    };
    AddIfPresent(WaterSlot);
    AddIfPresent(IngredientSlot1);
    AddIfPresent(IngredientSlot2);
    AddIfPresent(IngredientSlot3);
    return OutIngredients.Num() > 0;
}

void UAlchemyTransferWidget::ClearIngredientSlots()
{
    WaterSlot->Clear();
    IngredientSlot1->Clear();
    IngredientSlot2->Clear();
    IngredientSlot3->Clear();
}

void UAlchemyTransferWidget::SetStatusMessage(const FString& Message)
{
    if (StatusText)
        StatusText->SetText(FText::FromString(Message));
}

FReply UAlchemyTransferWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
    if (InKeyEvent.GetKey() == EKeys::E)
    {
        APlayerController* PC = GetOwningPlayer();
        AHerbalistPlayerController* HPC = Cast<AHerbalistPlayerController>(PC);
        if (HPC)
        {
            HPC->CloseAnyWidget();
            return FReply::Handled();
        }
    }
    return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}