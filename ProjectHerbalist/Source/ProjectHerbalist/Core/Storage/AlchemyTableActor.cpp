// AlchemyTableActor.cpp
#include "AlchemyTableActor.h"
#include "ProjectHerbalist.h"
#include "HerbalistLogChannels.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Player/HerbalistPlayerController.h"
#include "UI/AlchemyTransferWidget.h"
#include "Core/World/GridWorldManager.h"
#include "Core/Alchemy/RitualTypes.h"
#include "Core/Inventory/HerbalistInventoryComponent.h"
#include "Core/Subsystems/IngredientRegistrySubsystem.h"
#include "Player/HeldItemActor.h"
#include "Engine/GameInstance.h"
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
#if WITH_EDITORONLY_DATA
    // Котёл -- дом: всегда загружен, как регионы биомов. Иначе World Partition,
    // выгрузив его вдали, унёс бы заложенное (EndPlay его чистит) и
    // регистрацию Домового (ревью 2026-09-21). Уже расставленные котлы
    // получают это из дескриптора класса, пересохранять не нужно.
    bIsSpatiallyLoaded = false;
#endif
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
    // в ShrineInfluenceRadiusMeters, а не даётся варке безусловно.
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

            // Явный лог точной клетки (2026-09-07, прямой запрос пользователя
            // после подозрения "порча идёт от стола") -- координаты стола
            // раньше нигде не печатались напрямую, приходилось выводить их из
            // мировых координат в комментарии коммандлета. Теперь видно
            // однозначно, без пересчёта вручную.
            UE_LOG(LogHerbalistAlchemy, Log, TEXT("AlchemyTableActor stands on cell (%d,%d)"), GridCoords.X, GridCoords.Y);
        }
        else
        {
            UE_LOG(LogHerbalistAlchemy, Warning, TEXT("AlchemyTableActor at %s is outside the grid — Домовой/Роса не зарегистрированы"), *GetActorLocation().ToString());
        }
        break;
    }
}

void AAlchemyTableActor::EndPlay(const EEndPlayReason::Type Reason)
{
    if (AGridWorldManager* WorldManager = BoundWorldManager.Get())
    {
        WorldManager->OnBrewCompleted.Remove(BrewCompletedHandle);
    }
    BoundWorldManager.Reset();
    BrewCompletedHandle.Reset();
    Contents.Reset();
    bHasReadyResult = false;
    SyncContentsVisual();
    Super::EndPlay(Reason);
}

void AAlchemyTableActor::OnInteract_Implementation(AHerbalistPlayerController* PC)
{
    // Окно варки ушло (DESIGN_Diegetic_Interface.md, этап 3): пустой рукой
    // котёл мешают, травы кладут рукой (ReceiveHeldItem). Окно осталось
    // только за отладочной командой OpenCauldronWindow.
    const ECauldronStirResult Result = Stir(PC);
    UE_LOG(LogHerbalistAlchemy, Log, TEXT("Cauldron (%d,%d) stirred: result %d"), GridCoords.X, GridCoords.Y, static_cast<int32>(Result));
}

bool AAlchemyTableActor::ReceiveHeldItem(AHerbalistPlayerController* PC, int32 InventoryIndex)
{
    if (!PC || !PC->InventoryComponent || !PC->InventoryComponent->GetItems().IsValidIndex(InventoryIndex)) return false;

    FInventoryItem Portion = PC->InventoryComponent->GetItems()[InventoryIndex];
    Portion.Count = 1;
    if (PC->IsArtifactReceipt(Portion))
    {
        UE_LOG(LogHerbalistAlchemy, Log, TEXT("Cauldron: '%s' -- артефакт, в котёл не идёт"), *Portion.IngredientID.ToString());
        return true;
    }

    // Вода -- по реестру, как решало окно (UAlchemySlotWidget::CanAcceptItem);
    // без реестра (автотесты) -- по флагу самого предмета. Зелье водой не
    // бывает. Флаг ставится явно: ритуал ищет воду по нему (HasRequiredWater).
    bool bWater = Portion.bIsWater;
    if (const UGameInstance* GI = PC->GetGameInstance())
    {
        if (const UIngredientRegistrySubsystem* Registry = GI->GetSubsystem<UIngredientRegistrySubsystem>())
        {
            bWater = bWater || Registry->IsWater(Portion.IngredientID);
        }
    }
    if (Portion.IngredientID == FName(TEXT("Potion"))) bWater = false;
    Portion.bIsWater = bWater;

    int32 WaterPortions = 0;
    for (const FInventoryItem& Laid : Contents)
    {
        if (Laid.bIsWater) ++WaterPortions;
    }
    const int32 HerbPortions = Contents.Num() - WaterPortions;
    if (bWater ? WaterPortions >= MaxWaterPortions : HerbPortions >= MaxHerbPortions)
    {
        // Отказ -- тоже ответ цели: мешать этим жестом нельзя, предмет в руке.
        UE_LOG(LogHerbalistAlchemy, Log, TEXT("Cauldron (%d,%d) is full for %s"), GridCoords.X, GridCoords.Y,
            bWater ? TEXT("water") : TEXT("herbs"));
        return true;
    }

    if (!PC->InventoryComponent->RemoveItem(InventoryIndex, 1)) return true;
    Contents.Add(Portion);
    SyncContentsVisual();
    return true;
}

ECauldronStirResult AAlchemyTableActor::Stir(AHerbalistPlayerController* PC)
{
    if (!PC || !PC->InventoryComponent) return ECauldronStirResult::NoWorld;
    UHerbalistInventoryComponent* Inventory = PC->InventoryComponent;

    // Завершённый ритуал, которому не нашлось места, ждёт в котле: его
    // зачерпывают первым делом.
    if (bHasReadyResult)
    {
        if (!Inventory->AddItem(ReadyResult, 1)) return ECauldronStirResult::BagFull;
        bHasReadyResult = false;
        SyncContentsVisual();
        return ECauldronStirResult::ResultTaken;
    }

    if (Contents.Num() == 0) return ECauldronStirResult::Empty;
    AGridWorldManager* WorldManager = PC->FindWorldManager();
    if (!WorldManager) return ECauldronStirResult::NoWorld;

    // Под каждое ожидаемое зелье -- своё свободное место: состояние
    // результата заранее неизвестно, влезет ли он в похожую стопку -- не
    // угадать. Шагу ритуала места не нужно (ревью 2026-09-21: полная котомка
    // не должна отнимать у ритуала его час), поэтому проверка -- после него.
    const bool bBagFull = Inventory->GetItems().Num() + PendingBrews >= Inventory->MaxSlots;

    // Сначала ритуал (решение пользователя 2026-09-21): совпали заложенное,
    // вода и час с шагом -- это шаг ритуала, игрок не выбирает «ритуал», он
    // просто делает правильно. Сид -- тот же, что у RitualStep: часы плюс
    // клетка котла, общий счётчик мира не сдвигается.
    FRandomStream Rng(static_cast<int32>(WorldManager->GetGameClockSeconds()) + GetTypeHash(GridCoords));
    FInventoryItem RitualResult;
    const ERitualStepResult Step = WorldManager->TryAdvanceRitual(GridCoords, Contents, Rng, RitualResult);
    if (Step == ERitualStepResult::Progressed)
    {
        ClearContents();
        return ECauldronStirResult::RitualProgressed;
    }
    if (Step == ERitualStepResult::Completed)
    {
        Contents.Reset();
        if (bBagFull || !Inventory->AddItem(RitualResult, 1))
        {
            ReadyResult = RitualResult;
            bHasReadyResult = true;
        }
        SyncContentsVisual();
        return ECauldronStirResult::RitualCompleted;
    }

    // Обычной варке место нужно; заложенное остаётся в котле.
    if (bBagFull)
    {
        return ECauldronStirResult::BagFull;
    }

    // Обычная варка -- тем же путём, что было у окна: менеджер собирает
    // команду с модификаторами варки, травы уже изъяты из котомки закладкой.
    if (BoundWorldManager.Get() != WorldManager)
    {
        if (AGridWorldManager* Previous = BoundWorldManager.Get())
        {
            Previous->OnBrewCompleted.Remove(BrewCompletedHandle);
        }
        BrewCompletedHandle = WorldManager->OnBrewCompleted.AddUObject(this, &AAlchemyTableActor::HandleBrewCompleted);
        BoundWorldManager = WorldManager;
    }
    WorldManager->QueueCauldronBrew(GridCoords, Contents);
    ++PendingBrews;
    ClearContents();
    return ECauldronStirResult::Brewing;
}

void AAlchemyTableActor::HandleBrewCompleted(const FInventoryItem& Produced, const FIntPoint& BrewCell)
{
    // Сам предмет уже в котомке (RunSimulationStep) -- здесь только счёт мест,
    // и только своих варок: зелье соседнего котла чужое место не освобождает.
    if (BrewCell == GridCoords && PendingBrews > 0)
    {
        --PendingBrews;
    }
}

void AAlchemyTableActor::CaptureSaved(TArray<FInventoryItem>& OutContents, bool& bOutHasReadyResult, FInventoryItem& OutReadyResult) const
{
    OutContents = Contents;
    bOutHasReadyResult = bHasReadyResult;
    OutReadyResult = ReadyResult;
}

void AAlchemyTableActor::RestoreSaved(const TArray<FInventoryItem>& InContents, bool bInHasReadyResult, const FInventoryItem& InReadyResult)
{
    Contents = InContents;
    bHasReadyResult = bInHasReadyResult;
    ReadyResult = InReadyResult;
    SyncContentsVisual();
}

void AAlchemyTableActor::ClearContents()
{
    Contents.Reset();
    SyncContentsVisual();
}

void AAlchemyTableActor::SyncContentsVisual()
{
    for (AHeldItemActor* Actor : ContentActors)
    {
        if (Actor)
        {
            Actor->Destroy();
        }
    }
    ContentActors.Reset();

    // Готовый результат ритуала показывается так же, как заложенное.
    TArray<FInventoryItem> Shown = Contents;
    if (bHasReadyResult)
    {
        Shown.Add(ReadyResult);
    }
    UWorld* World = GetWorld();
    if (!World || Shown.Num() == 0) return;

    // Заложенное лежит над котлом в ряд, в порядке закладки: пока нет пара и
    // жидкости (арт, 07_UX §7.4), так хотя бы видно, что в котле.
    FVector Origin;
    FVector Extent;
    GetActorBounds(true, Origin, Extent);
    const FVector Top(Origin.X, Origin.Y, Origin.Z + Extent.Z + 6.0f);
    constexpr float Spacing = 7.0f;
    for (int32 Index = 0; Index < Shown.Num(); ++Index)
    {
        FActorSpawnParameters Params;
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        AHeldItemActor* Actor = World->SpawnActor<AHeldItemActor>(AHeldItemActor::StaticClass(), FTransform::Identity, Params);
        if (!Actor) continue;
        const FInventoryItem& Laid = Shown[Index];
        Actor->ShowItem(Laid, false, Laid.bIsWater);
        Actor->SetActorScale3D(FVector(0.05f));
        const float Offset = (Index - (Shown.Num() - 1) * 0.5f) * Spacing;
        Actor->SetActorLocation(Top + GetActorRightVector() * Offset);
        ContentActors.Add(Actor);
    }
}

void AAlchemyTableActor::OpenWindow(AHerbalistPlayerController* PC)
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
