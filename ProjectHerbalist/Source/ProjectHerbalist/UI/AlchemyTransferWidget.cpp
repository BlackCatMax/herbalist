// AlchemyTransferWidget.cpp
#include "UI/AlchemyTransferWidget.h"
#include "UI/InventoryWidget.h"
#include "UI/HerbalistWidgetSizing.h"
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
    HerbalistUI::LetSizeBoxesGrowWithContent(WidgetTree);
    if (MixButton)
    {
        MixButton->OnClicked.AddDynamic(this, &UAlchemyTransferWidget::OnMixClicked);
    }
    WaterSlot->InitializeSlot(EAlchemySlotType::Water, 1);
    IngredientSlot1->InitializeSlot(EAlchemySlotType::Ingredient, 9);
    IngredientSlot2->InitializeSlot(EAlchemySlotType::Ingredient, 9);
    IngredientSlot3->InitializeSlot(EAlchemySlotType::Ingredient, 9);
    ResultSlot->InitializeSlot(EAlchemySlotType::Result, 1);

    // Результат варки -- уведомлением менеджера с настоящим предметом.
    // Проверка хэндла: NativeConstruct повторяется при повторном показе окна.
    if (!BrewCompletedHandle.IsValid())
    {
        if (AHerbalistPlayerController* HPC = Cast<AHerbalistPlayerController>(GetOwningPlayer()))
        {
            if (AGridWorldManager* WorldManager = HPC->FindWorldManager())
            {
                BoundWorldManager = WorldManager;
                BrewCompletedHandle = WorldManager->OnBrewCompleted.AddUObject(this, &UAlchemyTransferWidget::HandleBrewCompleted);
            }
        }
    }

    SetKeyboardFocus();
}

void UAlchemyTransferWidget::NativeDestruct()
{
    // Потеря ингредиентов при закрытии котла (аудит 2026-09-05): предмет,
    // перетащенный в WaterSlot/IngredientSlotN, изымается из реального
    // инвентаря игрока в момент дропа (NativeOnDrop) и живёт дальше только в
    // StoredItem этого виджета -- ResultSlot не в счёт, он ничего не изымает
    // (см. NativeOnMouseButtonDoubleClick выше). Раньше NativeDestruct просто
    // отписывался от делегатов, ничего не возвращая: закрытие котла (клавиша
    // E/ESC) без нажатия "Смешать" молча уничтожало всё, что лежало в
    // слотах. Возвращаем содержимое обратно, тем же AddItem, что и обычный
    // возврат по двойному клику.
    if (PlayerInventoryComponent)
    {
        auto ReturnSlot = [this](UAlchemySlotWidget* SlotWidget)
        {
            if (!SlotWidget || !SlotWidget->GetItem() || SlotWidget->GetCount() <= 0) return;
            FInventoryItem ItemToReturn = *SlotWidget->GetItem();
            ItemToReturn.Count = SlotWidget->GetCount();
            PlayerInventoryComponent->AddItem(ItemToReturn, ItemToReturn.Count);
            SlotWidget->Clear();
        };
        ReturnSlot(WaterSlot);
        ReturnSlot(IngredientSlot1);
        ReturnSlot(IngredientSlot2);
        ReturnSlot(IngredientSlot3);
    }

    if (MixButton)
    {
        MixButton->OnClicked.RemoveDynamic(this, &UAlchemyTransferWidget::OnMixClicked);
    }
    if (AGridWorldManager* WorldManager = BoundWorldManager.Get())
    {
        WorldManager->OnBrewCompleted.Remove(BrewCompletedHandle);
    }
    BoundWorldManager.Reset();
    BrewCompletedHandle.Reset();
    Super::NativeDestruct();
}

void UAlchemyTransferWidget::OnMixClicked()
{
    if (bIsMixing) return;

    // Занятая витрина варку не блокирует: зелье прошлой варки уже в сумке,
    // забирать из слота результата нечего.
    TArray<FInventoryItem> Ingredients;
    if (!CollectIngredients(Ingredients) || Ingredients.Num() == 0)
    {
        SetStatusMessage(TEXT("Нет ингредиентов."));
        return;
    }

    // Полная сумка (ревью 2026-09-14): AddItem откажет, а травы уже в котле --
    // зелье пропало бы. Состояние результата заранее неизвестно, влезет ли он
    // в похожую стопку -- не угадать, поэтому нужен свободный слот, и не один
    // на все варки: зелье приходит следующим тиком, и две варки до него при
    // одной свободной строке теряли второе. Травы остаются в слотах.
    if (PlayerInventoryComponent && PlayerInventoryComponent->GetItems().Num() + PendingBrewCount >= PlayerInventoryComponent->MaxSlots)
    {
        SetStatusMessage(TEXT("Сумка полна: освободите место под зелье."));
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

    // Виджет не ходит в мир напрямую — берёт уже закэшированный у контроллера
    // (HerbalistPlayerController::FindWorldManager), а не через TActorIterator.
    AGridWorldManager* WorldManager = HPC->FindWorldManager();
    if (!WorldManager)
    {
        SetStatusMessage(TEXT("Не найден GridWorldManager."));
        bIsMixing = false;
        return;
    }

    // Команду собирает менеджер: модификаторы варки те же, что у применения
    // на клетку, а ингредиенты помечены изъятыми -- они ушли из сумки ещё
    // при переносе в слоты.
    const FIntPoint TableCell = HPC->CurrentAlchemyTable ? HPC->CurrentAlchemyTable->GetGridCoords() : HerbalistCore::InvalidCell();
    WorldManager->QueueCauldronBrew(TableCell, Ingredients);
    ++PendingBrewCount;

    // Ингредиенты израсходованы -- в сумку не возвращаются.
    ClearIngredientSlots();
    ResultSlot->Clear();
    SetStatusMessage(TEXT("Варится..."));

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
// Витрина результата
// -----------------------------------------------------------------------------

void UAlchemyTransferWidget::HandleBrewCompleted(const FInventoryItem& Produced, const FIntPoint& BrewCell)
{
    if (PendingBrewCount <= 0) return;
    --PendingBrewCount;

    ResultSlot->Clear();
    ResultSlot->AddItem(Produced, 1);

    // Крафт может дать не только "Potion" — при вырожденных исходах (05_Systems.md)
    // Pipeline создаёт "Ash" (нет воды) или "BoiledWater" (одна вода), см.
    // ProcessApplyCommand.
    if (Produced.IngredientID == FName(TEXT("Ash")))
    {
        SetStatusMessage(TEXT("Без воды травы сгорели: вышла зола. Она в сумке."));
        return;
    }
    if (Produced.IngredientID == FName(TEXT("BoiledWater")))
    {
        SetStatusMessage(TEXT("Без трав вышла кипячёная вода. Она в сумке."));
        return;
    }

    // Не Produced.State: S_real игроку не показывается (07_UX, CHANGELOG.md
    // 2026-08-24) -- то же искажённое значение, что уже посчитал ResultSlot.
    const FRealState& Perceived = ResultSlot->GetPerceivedState();
    SetStatusMessage(FString::Printf(TEXT("Зелье готово и уже в сумке (сила: %.2f, искажение: %.2f)."),
        Perceived.Magnitude, Perceived.Meta.Distortion));
}