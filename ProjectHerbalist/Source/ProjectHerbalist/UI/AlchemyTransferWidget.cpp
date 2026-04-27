// UI/AlchemyTransferWidget.cpp
#include "UI/AlchemyTransferWidget.h"
#include "UI/InventoryWidget.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Player/HerbalistPlayerController.h"
#include "Core/Pipeline/HerbalistPipeline.h"
#include "Core/World/GridWorldManager.h"
#include "Core/BiomeGraph/BiomeGraphSubsystem.h"
#include "Core/Types/BiomeTypes.h"
#include "Core/Storage/AlchemyTableActor.h"
#include "Core/Pipeline/AlchemyPipelineFacade.h"   // <-- используем фасад
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

    UWorld* World = GetWorld();
    if (!World)
    {
        SetStatusMessage(TEXT("Ошибка мира."));
        bIsMixing = false;
        return;
    }

    UGameInstance* GameInstance = World->GetGameInstance();
    UIngredientRegistrySubsystem* IngredientSubsystem = GameInstance ? GameInstance->GetSubsystem<UIngredientRegistrySubsystem>() : nullptr;

    // Получаем GridWorldManager
    AGridWorldManager* WorldManager = nullptr;
    for (TActorIterator<AGridWorldManager> It(World); It; ++It)
    {
        WorldManager = *It;
        break;
    }

    FIntPoint TableCoords = HPC->CurrentAlchemyTable ? HPC->CurrentAlchemyTable->GetGridCoords() : FIntPoint(-1, -1);

    FRealState CellState = FAlatyr::S0;
    FEnvironment Env;
    FMemoryState Memory;
    float BiomeMorokField = 0.0f;
    float BiomeZaryanaField = 0.0f;
    FVector4 BiomeAxisDrift = FVector4(0.25f, 0.25f, 0.25f, 0.25f);
    FGridCell* Cell = nullptr;
    float GlobalDistortion = 0.3f;

    if (WorldManager && TableCoords.X >= 0 && TableCoords.Y >= 0)
    {
        Cell = WorldManager->GetCell(TableCoords.X, TableCoords.Y);
        if (Cell)
        {
            CellState = Cell->State;
            Env = Cell->Environment;
            Memory = Cell->Memory;
            GlobalDistortion = Cell->Memory.AccumulatedDistortion;

            if (UBiomeGraphSubsystem* Graph = World->GetSubsystem<UBiomeGraphSubsystem>())
            {
                FName BiomeID = FBiomeDefaults::BiomeTypeToName(Cell->Biome);
                if (const FBiomeGraphNode* Node = Graph->GetNode(BiomeID))
                {
                    BiomeMorokField = Node->MorokField;
                    BiomeZaryanaField = Node->ZaryanaField;
                    BiomeAxisDrift = Node->Memory.AxisDrift;
                }
            }
        }
    }
    else // Если нет координат, используем глобальный distortion контроллера
    {
        GlobalDistortion = HPC->CurrentGlobalDistortion;
    }

    FRngState Rng;
    int32 Seed = 12345;
    if (HPC && HPC->CurrentAlchemyTable)
    {
        FIntPoint Coords = HPC->CurrentAlchemyTable->GetGridCoords();
        Seed = (Coords.X * 7919) ^ (Coords.Y * 7901);
    }
    Rng.Seed = Seed;

    // Используем единый фасад
    FAlchemyFacadeResult Result = FAlchemyPipelineFacade::Execute(
        Ingredients,
        CellState, Env, Memory,
        GlobalDistortion,
        IngredientSubsystem,
        BiomeMorokField, BiomeZaryanaField, BiomeAxisDrift,
        Rng);

    FInventoryItem Potion;
    switch (Result.Outcome)
    {
    case EAlchemyOutcome::BoiledWater:
        Potion.IngredientID = FName(TEXT("BoiledWater"));
        break;
    case EAlchemyOutcome::Ash:
    case EAlchemyOutcome::Catastrophe: // Catastrophe тоже можно считать Ash или особым именем
        Potion.IngredientID = FName(TEXT("Ash"));
        break;
    default:
        Potion.IngredientID = FName(TEXT("Potion"));
        break;
    }
    Potion.State = Result.FinalState;
    Potion.Count = 1;

    ResultSlot->AddItem(Potion, 1);
    ClearIngredientSlots();
    SetStatusMessage(TEXT("Готово."));

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