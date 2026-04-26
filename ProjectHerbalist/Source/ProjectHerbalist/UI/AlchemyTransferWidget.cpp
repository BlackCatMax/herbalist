// AlchemyTransferWidget.cpp
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
#include "Core/Pipeline/IntentResolver.h"
#include "Core/Pipeline/AlchemySemantics.h"
#include "Core/Pipeline/AlchemySemanticResolver.h"
#include "Core/Pipeline/AlchemyPhysicsPipeline.h"
#include "Core/Pipeline/AlchemyWorldStateApplier.h"
#include "Core/Pipeline/AlchemyTypes.h"
#include "Core/Data/IngredientRegistry.h"
#include "Core/Harvest/HarvestService.h"
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

    // Получаем GridWorldManager
    AGridWorldManager* WorldManager = nullptr;
    UWorld* World = GetWorld();
    if (World)
    {
        for (TActorIterator<AGridWorldManager> It(World); It; ++It)
        {
            WorldManager = *It;
            break;
        }
    }

    // Получаем координаты алхимического стола
    FIntPoint TableCoords = HPC->CurrentAlchemyTable ? HPC->CurrentAlchemyTable->GetGridCoords() : FIntPoint(-1, -1);

    // Определяем состояние биома и контекст графа
    FRealState CurrentBiomeState = FAlatyr::S0;
    FEnvironment Env;
    FMemoryState Memory;
    float BiomeMorokField = 0.0f;
    float BiomeZaryanaField = 0.0f;
    FVector4 BiomeAxisDrift = FVector4(0.25f, 0.25f, 0.25f, 0.25f);
    FGridCell* Cell = nullptr;

    if (WorldManager && TableCoords.X >= 0 && TableCoords.Y >= 0)
    {
        Cell = WorldManager->GetCell(TableCoords.X, TableCoords.Y);
        if (Cell)
        {
            CurrentBiomeState = Cell->State;
            Env = Cell->Environment;
            Memory = Cell->Memory;

            // Получаем контекст биома из графа
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

    // 1. Конвертируем ингредиенты в атомы
    TArray<FAlchemyAtom> Atoms;
    for (const FInventoryItem& Item : Ingredients)
    {
        FAlchemyAtom Atom(
            Item.IngredientID,
            FIngredientRegistry::IsWater(Item.IngredientID),
            Item.State,
            EAtomOrigin::Harvest,
            Memory.AccumulatedDistortion,
            GetWorld()->GetTimeSeconds()
        );
        Atoms.Add(Atom);
    }

    // 2. Семантическое разрешение
    // Пункт 2.3: используем Distortion клетки стола, если доступна
    float GlobalD = 0.3f;
    if (Cell)
    {
        GlobalD = Cell->Memory.AccumulatedDistortion;
    }
    else if (HPC)
    {
        GlobalD = HPC->CurrentGlobalDistortion;
    }
    FAlchemySemanticResult Semantic = FAlchemySemanticResolver::Resolve(Atoms, GlobalD);
    UE_LOG(LogHerbalist, Log, TEXT("OnMixClicked: GlobalDistortion used = %.3f"), GlobalD);

    // 3. Готовим Rng
    FRngState Rng;
    int32 Seed = 12345; // fallback
    if (HPC && HPC->CurrentAlchemyTable)
    {
        FIntPoint Coords = HPC->CurrentAlchemyTable->GetGridCoords();
        Seed = (Coords.X * 7919) ^ (Coords.Y * 7901);
    }
    Rng.Seed = Seed;

    // 4. Запуск физики (только для Valid)
    FRealState ResultState;
    FName ResultID;

    if (Semantic.Outcome == EAlchemyOutcome::Valid)
    {
        TArray<FRealState> IngredientStates, WaterStates;
        for (const FAlchemyAtom& A : Semantic.IngredientAtoms) IngredientStates.Add(A.State);
        for (const FAlchemyAtom& A : Semantic.WaterAtoms) WaterStates.Add(A.State);

        FAlchemyPhysicsResult Physics = FAlchemyPhysicsPipeline::Run(
            IngredientStates, WaterStates,
            CurrentBiomeState, Env, Memory,
            Semantic.Coherence,
            Rng, BiomeMorokField, BiomeZaryanaField, BiomeAxisDrift);

        ResultState = Physics.State;
        if (Physics.bCatastropheTriggered)
        {
            ResultState = HerbalistCore::ApplyCatastropheTransform(ResultState, Physics.bCollapse, Rng);
            Semantic.Outcome = EAlchemyOutcome::Catastrophe;
        }
        ResultID = FName(TEXT("Potion"));
    }
    else if (Semantic.Outcome == EAlchemyOutcome::BoiledWater)
    {
        TArray<FRealState> WaterStates;
        for (const FAlchemyAtom& A : Semantic.WaterAtoms) WaterStates.Add(A.State);
        ResultState = HerbalistCore::ApplyBoiledWaterTransform(WaterStates);
        ResultID = FName(TEXT("BoiledWater"));
    }
    else // Ash
    {
        FMeta CoreMeta;
        ResultState = HerbalistCore::ApplyAshTransform(CoreMeta);
        ResultID = FName(TEXT("Ash"));
    }

    FInventoryItem Potion;
    Potion.IngredientID = ResultID;
    Potion.State = ResultState;
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