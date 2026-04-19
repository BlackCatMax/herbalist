// AlchemyTransferWidget.cpp
#include "UI/AlchemyTransferWidget.h"
#include "UI/InventoryWidget.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Player/HerbalistPlayerController.h"
#include "Core/Pipeline/HerbalistPipeline.h"

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
    if (bIsMixing) return;
    
    if (ResultSlot->GetItem() && ResultSlot->GetCount() > 0)
    {
        SetStatusMessage(TEXT("Сначала заберите готовое зелье."));
        return;
    }
    
    TArray<FInventoryItem> Ingredients;
    if (!CollectIngredients(Ingredients) || Ingredients.Num() == 0)
    {
        SetStatusMessage(TEXT("Нет ингредиентов."));
        return;
    }
    
    bIsMixing = true;
    
    APlayerController* PC = GetOwningPlayer();
    AHerbalistPlayerController* HPC = Cast<AHerbalistPlayerController>(PC);
    if (!HPC)
    {
        SetStatusMessage(TEXT("Ошибка системы."));
        bIsMixing = false;
        return;
    }
    
    const FRealState& BiomeState = FAlatyr::S0;
    FIntent DefaultIntent;
    DefaultIntent.Coherence = 0.5f;
    FRngState Rng;
    Rng.Seed = FMath::Rand();
    
    FRealState ResultState = HerbalistCore::Pipeline::ApplyMorok(
        Ingredients,
        BiomeState,
        FEnvironment(),
        FMemoryState(),
        DefaultIntent,
        Rng
    );
    
    FInventoryItem Potion;
    Potion.Type = EResourceType::Potion;
    Potion.State = ResultState;
    Potion.Count = 1;
    
    ResultSlot->AddItem(Potion, 1);
    ClearIngredientSlots();
    SetStatusMessage(TEXT("Зелье готово."));
    
    bIsMixing = false;
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