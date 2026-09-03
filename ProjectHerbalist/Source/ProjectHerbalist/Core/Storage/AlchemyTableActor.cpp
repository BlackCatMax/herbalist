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

    // Капище стол больше НЕ регистрирует (2026-09-02, прямой запрос
    // пользователя: "хочу, чтобы капища были отдельными местами, которые не
    // зависят от местоположения котла"). До этого §15.5 читалось как "особая
    // аура привязана к самому месту варки", и капище существовало только на
    // клетке котла. Теперь капища расставляются отдельно (AShrineActor), а
    // близость котла к капищу — выбор игрока: надбавка к Coherence работает
    // в ShrineInfluenceRadius, а не даётся варке безусловно.
    //
    // Домовой и Роса Заряны остаются здесь — они про очаг/жилище, а не про
    // капище, и от этой развязки не зависят.
    for (TActorIterator<AGridWorldManager> It(GetWorld()); It; ++It)
    {
        AGridWorldManager* WorldManager = *It;
        int32 X, Y;
        if (WorldManager->WorldPositionToCell(GetActorLocation(), X, Y))
        {
            GridCoords = FIntPoint(X, Y);

            // Домовой (DESIGN_Community_And_Homestead.md §2.1, 2026-08-31) —
            // хозяин очага, не место на карте: там же, где котёл, не через
            // биом-сопоставление.
            WorldManager->RegisterDomovoi(GridCoords);

            // Роса Заряны (19_Rosa_Signal.md §19.2) — дефолт "рядом с домом",
            // не перезаписывает явную расстановку левел-дизайнером.
            WorldManager->SetZaryanaCellIfUnset(GridCoords);
        }
        else
        {
            UE_LOG(LogHerbalistAlchemy, Warning, TEXT("AlchemyTableActor at %s is outside the grid — Домовой/Роса не зарегистрированы"), *GetActorLocation().ToString());
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
