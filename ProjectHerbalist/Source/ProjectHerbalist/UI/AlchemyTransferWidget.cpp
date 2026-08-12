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

    // Подписываемся на изменение инвентаря игрока, если он уже привязан
    if (PlayerInventoryComponent)
    {
        PlayerInventoryComponent->OnInventoryChanged.AddDynamic(this, &UAlchemyTransferWidget::OnInventoryChanged);
    }

    SetKeyboardFocus();
}

void UAlchemyTransferWidget::NativeDestruct()
{
    if (MixButton)
    {
        MixButton->OnClicked.RemoveDynamic(this, &UAlchemyTransferWidget::OnMixClicked);
    }
    if (PlayerInventoryComponent)
    {
        PlayerInventoryComponent->OnInventoryChanged.RemoveDynamic(this, &UAlchemyTransferWidget::OnInventoryChanged);
    }
    Super::NativeDestruct();
}

void UAlchemyTransferWidget::OnMixClicked()
{
    if (bIsMixing) return;

    if (ResultSlot->GetItem() && ResultSlot->GetCount() > 0)
    {
        SetStatusMessage(TEXT("Сначала заберите готовое зелье из слота результата."));
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

    UWorld* World = GetWorld();
    if (!World)
    {
        SetStatusMessage(TEXT("Ошибка мира."));
        bIsMixing = false;
        return;
    }

    // Виджет не ходит в мир напрямую — берёт уже закэшированный у контроллера
    // (HerbalistPlayerController::FindWorldManager), а не через TActorIterator.
    AGridWorldManager* WorldManager = HPC->FindWorldManager();
    if (!WorldManager)
    {
        SetStatusMessage(TEXT("Не найден GridWorldManager."));
        bIsMixing = false;
        return;
    }

    // Запоминаем время крафта для последующего поиска созданного зелья
    LastCraftTime = World->GetTimeSeconds();

    FCommandEntry Cmd;
    Cmd.Primitive = ECommandPrimitive::Apply;
    Cmd.Apply.TargetCell = FIntPoint(-1, -1);
    Cmd.Apply.Ingredients = Ingredients;
    // Coherence считается Pipeline'ом из Ingredients (ComputeIntentCoherence).
    Cmd.Apply.bIsCrafting = true;

    WorldManager->QueueCommand(Cmd);

    ClearIngredientSlots();
    SetStatusMessage(TEXT("Зелье создаётся... Оно появится в слоте результата."));

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

// -----------------------------------------------------------------------------
// Отслеживание созданного зелья
// -----------------------------------------------------------------------------

void UAlchemyTransferWidget::OnInventoryChanged()
{
    CheckForNewPotion();
}

void UAlchemyTransferWidget::CheckForNewPotion()
{
    if (!PlayerInventoryComponent || LastCraftTime <= 0.0f)
        return;

    UWorld* World = GetWorld();
    if (!World) return;

    // Крафт может дать не только "Potion" — при вырожденных исходах (05_Systems.md)
    // Pipeline создаёт "Ash"/"BoiledWater" вместо зелья, см. ProcessApplyCommand.
    const TArray<FInventoryItem>& Items = PlayerInventoryComponent->GetItems();
    for (const FInventoryItem& Item : Items)
    {
        const bool bIsCraftResult = Item.IngredientID == FName(TEXT("Potion"))
            || Item.IngredientID == FName(TEXT("Ash"))
            || Item.IngredientID == FName(TEXT("BoiledWater"));
        if (bIsCraftResult && Item.Count > 0)
        {
            // Если время создания предмета больше времени крафта (с погрешностью 0.1 сек)
            if (Item.CreationTime >= LastCraftTime - 0.1f)
            {
                ResultSlot->Clear();
                ResultSlot->AddItem(Item, 1);
                LastCraftTime = 0.0f;
                SetStatusMessage(FString::Printf(TEXT("Создано зелье (сила: %.2f, искажение: %.2f)"),
                    Item.State.Magnitude, Item.State.Meta.Distortion));
                return;
            }
        }
    }

    float CurrentTime = World->GetTimeSeconds();
    if (CurrentTime - LastCraftTime > 2.0f)
    {
        // Прошло больше 2 секунд, зелье не найдено – сбрасываем ожидание
        LastCraftTime = 0.0f;
    }
}