// AlchemyTableActor.cpp
#include "AlchemyTableActor.h"
#include "ProjectHerbalist.h"
#include "HerbalistLogChannels.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Player/HerbalistPlayerController.h"
#include "UI/AlchemyTransferWidget.h"
#include "Core/World/GridWorldManager.h"
#include "EngineUtils.h"

AAlchemyTableActor::AAlchemyTableActor()
{
    PrimaryActorTick.bCanEverTick = false;
    Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
    RootComponent = Mesh;
    InteractionBox = CreateDefaultSubobject<UBoxComponent>(TEXT("InteractionBox"));
    InteractionBox->SetupAttachment(RootComponent);
    InteractionBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    InteractionBox->SetCollisionResponseToAllChannels(ECR_Ignore);
    InteractionBox->SetCollisionResponseToChannel(ECC_GameTraceChannel1, ECR_Block);
}

void AAlchemyTableActor::BeginPlay()
{
    Super::BeginPlay();

    // Капище v1 (02_GDD/15_Cycles_And_Shrines.md §15.5) — не отдельная сущность,
    // ищущаяся в мире: "особая аура" привязана к самому месту варки. Каждый
    // стол автоматически регистрирует капище на своей клетке. GridCoords тоже
    // раньше была объявлена (SetGridCoords), но никогда никем не вызывалась —
    // закрываем это заодно, не отдельной задачей.
    for (TActorIterator<AGridWorldManager> It(GetWorld()); It; ++It)
    {
        AGridWorldManager* WorldManager = *It;
        int32 X, Y;
        if (WorldManager->WorldPositionToCell(GetActorLocation(), X, Y))
        {
            GridCoords = FIntPoint(X, Y);
            WorldManager->RegisterShrine(GridCoords, WorldManager->ResolveShrineTypeForCell(GridCoords));

            // Домовой (DESIGN_Community_And_Homestead.md §2.1, 2026-08-31) —
            // хозяин очага, не место на карте: там же, где котёл, не через
            // биом-сопоставление, тот же принцип, что и у капища выше.
            WorldManager->RegisterDomovoi(GridCoords);
        }
        else
        {
            UE_LOG(LogHerbalistAlchemy, Warning, TEXT("AlchemyTableActor at %s is outside the grid — no shrine registered"), *GetActorLocation().ToString());
        }
        break;
    }
}

void AAlchemyTableActor::OnInteract_Implementation(AHerbalistPlayerController* PC)
{
    if (!PC || !AlchemyWidgetClass) return;

    if (AlchemyWidgetInstance && AlchemyWidgetInstance->IsInViewport())
    {
        AlchemyWidgetInstance->RemoveFromParent();
        AlchemyWidgetInstance = nullptr;
        PC->bIsAnyWidgetOpen = false;
        PC->SetInputMode(FInputModeGameOnly());
        PC->bShowMouseCursor = false;
        return;
    }

    if (PC->bIsAnyWidgetOpen) return;

    AlchemyWidgetInstance = CreateWidget<UAlchemyTransferWidget>(GetWorld(), AlchemyWidgetClass);
    if (AlchemyWidgetInstance)
    {
        AlchemyWidgetInstance->BindInventory(PC->InventoryComponent);
        AlchemyWidgetInstance->AddToViewport();

        PC->CurrentAlchemyWidget = AlchemyWidgetInstance;
        PC->CurrentAlchemyTable = this;

        PC->bIsAnyWidgetOpen = true;
        PC->SetInputMode(FInputModeUIOnly());
        PC->bShowMouseCursor = true;
    }
}
