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
        PlayerInventory->BindInventory(PlayerInventoryComponent);
}

bool UAlchemyTransferWidget::TryAddItemToSlot(const FInventoryItem& Item)
{
    if (WaterSlot->CanAcceptItem(Item) && WaterSlot->AddItem(Item, 1)) return true;
    if (IngredientSlot1->CanAcceptItem(Item) && IngredientSlot1->AddItem(Item, 1)) return true;
    if (IngredientSlot2->CanAcceptItem(Item) && IngredientSlot2->AddItem(Item, 1)) return true;
    if (IngredientSlot3->CanAcceptItem(Item) && IngredientSlot3->AddItem(Item, 1)) return true;
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
        MixButton->OnClicked.AddDynamic(this, &UAlchemyTransferWidget::OnMixClicked);
    WaterSlot->InitializeSlot(EAlchemySlotType::Water, 1);
    IngredientSlot1->InitializeSlot(EAlchemySlotType::Ingredient, 9);
    IngredientSlot2->InitializeSlot(EAlchemySlotType::Ingredient, 9);
    IngredientSlot3->InitializeSlot(EAlchemySlotType::Ingredient, 9);
    ResultSlot->InitializeSlot(EAlchemySlotType::Result, 1);

    if (PlayerInventoryComponent)
        PlayerInventoryComponent->OnInventoryChanged.AddDynamic(this, &UAlchemyTransferWidget::OnInventoryChanged);

    LoadStateFromTable();
    SetKeyboardFocus();
}

void UAlchemyTransferWidget::NativeDestruct()
{
    SaveStateToTable();
    if (MixButton)
        MixButton->OnClicked.RemoveDynamic(this, &UAlchemyTransferWidget::OnMixClicked);
    if (PlayerInventoryComponent)
        PlayerInventoryComponent->OnInventoryChanged.RemoveDynamic(this, &UAlchemyTransferWidget::OnInventoryChanged);
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
    if (!WaterSlot->GetItem() || WaterSlot->GetCount() == 0)
    {
        SetStatusMessage(TEXT("Вода обязательна для варки зелья."));
        return;
    }

    TArray<FInventoryItem> Ingredients;
    if (WaterSlot->GetItem())
    {
        FInventoryItem WaterItem = *WaterSlot->GetItem();
        WaterItem.Count = WaterSlot->GetCount();
        Ingredients.Add(WaterItem);
    }

    auto AddIngredient = [&](UAlchemySlotWidget* IngSlot)
    {
        if (IngSlot && IngSlot->GetItem() && IngSlot->GetCount() > 0)
        {
            FInventoryItem Item = *IngSlot->GetItem();
            Item.Count = IngSlot->GetCount();
            Ingredients.Add(Item);
        }
    };
    AddIngredient(IngredientSlot1);
    AddIngredient(IngredientSlot2);
    AddIngredient(IngredientSlot3);

    bool bHasNonWater = false;
    for (int32 i = 1; i < Ingredients.Num(); ++i)
    {
        if (Ingredients[i].IngredientID != FName(TEXT("Water")) && Ingredients[i].IngredientID != FName(TEXT("BoiledWater")))
        {
            bHasNonWater = true;
            break;
        }
    }
    if (!bHasNonWater)
    {
        SetStatusMessage(TEXT("Нужен хотя бы один ингредиент (не вода)."));
        return;
    }

    bIsMixing = true;
    AHerbalistPlayerController* HPC = Cast<AHerbalistPlayerController>(GetOwningPlayer());
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

    AGridWorldManager* WorldManager = nullptr;
    for (TActorIterator<AGridWorldManager> It(World); It; ++It)
    {
        WorldManager = *It;
        break;
    }
    if (!WorldManager)
    {
        SetStatusMessage(TEXT("Не найден GridWorldManager."));
        bIsMixing = false;
        return;
    }

    FIntPoint TableCoords = HPC->CurrentAlchemyTable ? HPC->CurrentAlchemyTable->GetGridCoords() : FIntPoint(-1, -1);
    float MorokField = 0.0f, ZaryanaField = 0.0f;
    FVector4 AxisDrift(0.25f, 0.25f, 0.25f, 0.25f);
    if (WorldManager && TableCoords.X >= 0 && TableCoords.Y >= 0)
    {
        if (FGridCell* Cell = WorldManager->GetCell(TableCoords.X, TableCoords.Y))
        {
            if (UBiomeGraphSubsystem* Graph = World->GetSubsystem<UBiomeGraphSubsystem>())
            {
                FName BiomeID = FBiomeDefaults::BiomeTypeToName(Cell->Biome);
                if (const FBiomeGraphNode* Node = Graph->GetNode(BiomeID))
                {
                    MorokField = Node->MorokField;
                    ZaryanaField = Node->ZaryanaField;
                    AxisDrift = Node->Memory.AxisDrift;
                }
            }
        }
    }

    LastCraftTime = World->GetTimeSeconds();

    FCommandEntry Cmd;
    Cmd.Primitive = ECommandPrimitive::Apply;
    Cmd.Apply.TargetCell = FIntPoint(-1, -1);
    Cmd.Apply.Ingredients = Ingredients;
    Cmd.Apply.Intent.Coherence = 0.5f;
    Cmd.Apply.bIsCrafting = true;
    Cmd.Apply.BiomeMorokField = MorokField;
    Cmd.Apply.BiomeZaryanaField = ZaryanaField;
    Cmd.Apply.BiomeAxisDrift = AxisDrift;

    WorldManager->QueueCommand(Cmd);
    ClearIngredientSlots();
    SetStatusMessage(TEXT("Зелье создаётся..."));
    bIsMixing = false;
}

bool UAlchemyTransferWidget::CollectIngredients(TArray<FInventoryItem>& OutIngredients)
{
    OutIngredients.Empty();
    auto AddPresent = [&](UAlchemySlotWidget* IngSlot)
    {
        if (IngSlot && IngSlot->GetItem() && IngSlot->GetCount() > 0)
        {
            FInventoryItem Item = *IngSlot->GetItem();
            Item.Count = IngSlot->GetCount();
            OutIngredients.Add(Item);
        }
    };
    AddPresent(WaterSlot);
    AddPresent(IngredientSlot1);
    AddPresent(IngredientSlot2);
    AddPresent(IngredientSlot3);
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
    if (StatusText) StatusText->SetText(FText::FromString(Message));
}

FReply UAlchemyTransferWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
    if (InKeyEvent.GetKey() == EKeys::E)
    {
        if (AHerbalistPlayerController* HPC = Cast<AHerbalistPlayerController>(GetOwningPlayer()))
        {
            HPC->CloseAnyWidget();
            return FReply::Handled();
        }
    }
    return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void UAlchemyTransferWidget::OnInventoryChanged()
{
    CheckForNewPotion();
}

void UAlchemyTransferWidget::CheckForNewPotion()
{
    if (!PlayerInventoryComponent || LastCraftTime <= 0.0f) return;
    UWorld* World = GetWorld();
    if (!World) return;
    const TArray<FInventoryItem>& Items = PlayerInventoryComponent->GetItems();
    for (const FInventoryItem& Item : Items)
    {
        if (Item.IngredientID == FName(TEXT("Potion")) && Item.Count > 0)
        {
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
    if (CurrentTime - LastCraftTime > 2.0f) LastCraftTime = 0.0f;
}

void UAlchemyTransferWidget::SaveStateToTable()
{
    AHerbalistPlayerController* HPC = Cast<AHerbalistPlayerController>(GetOwningPlayer());
    if (!HPC || !HPC->CurrentAlchemyTable) return;
    AAlchemyTableActor* Table = HPC->CurrentAlchemyTable;
    Table->SetSlotItem(0, WaterSlot->GetItem() ? *WaterSlot->GetItem() : FInventoryItem());

    auto SaveIngredient = [&](UAlchemySlotWidget* IngSlot, int32 Index)
    {
        if (IngSlot->GetItem())
            Table->SetSlotItem(Index, *IngSlot->GetItem());
        else
            Table->ClearSlot(Index);
    };
    SaveIngredient(IngredientSlot1, 1);
    SaveIngredient(IngredientSlot2, 2);
    SaveIngredient(IngredientSlot3, 3);
}

void UAlchemyTransferWidget::LoadStateFromTable()
{
    AHerbalistPlayerController* HPC = Cast<AHerbalistPlayerController>(GetOwningPlayer());
    if (!HPC || !HPC->CurrentAlchemyTable) return;
    AAlchemyTableActor* Table = HPC->CurrentAlchemyTable;
    FInventoryItem WaterItem = Table->GetSlotItem(0);
    if (WaterItem.IsValid())
        WaterSlot->AddItem(WaterItem, WaterItem.Count);
    else
        WaterSlot->Clear();

    auto LoadIngredient = [&](UAlchemySlotWidget* IngSlot, int32 Index)
    {
        FInventoryItem Item = Table->GetSlotItem(Index);
        if (Item.IsValid())
            IngSlot->AddItem(Item, Item.Count);
        else
            IngSlot->Clear();
    };
    LoadIngredient(IngredientSlot1, 1);
    LoadIngredient(IngredientSlot2, 2);
    LoadIngredient(IngredientSlot3, 3);
}