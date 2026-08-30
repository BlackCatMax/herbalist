// HerbalistPlayerController.cpp
#include "Player/HerbalistPlayerController.h"
#include "Core/Resources/AHerbalistResourceActor.h"
#include "Core/Storage/AlchemyTableActor.h"
#include "Core/Zaryana/MemoryFragmentActor.h"
#include "Core/Interaction/Interactable.h"
#include "Core/Storage/StorageContainer.h"
#include "Core/Subsystems/IngredientRegistrySubsystem.h"
#include "Core/Save/HerbalistSaveSubsystem.h"
#include "Core/World/GridWorldManager.h"
#include "Core/Config/HerbalistSettings.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "ProjectHerbalist.h"
#include "HerbalistLogChannels.h"
#include "UI/AlchemyTransferWidget.h"
#include "UI/InventoryTransferWidget.h"
#include "UI/InventoryWidget.h"
#include "UI/JournalLogWidget.h"
#include "UI/MemoryRevealWidget.h"
#include "Core/Simulation/Public/CommandTypes.h"

// ============================================================================
// ЖИЗНЕННЫЙ ЦИКЛ
// ============================================================================

AHerbalistPlayerController::AHerbalistPlayerController()
{
    InventoryComponent = CreateDefaultSubobject<UHerbalistInventoryComponent>(TEXT("InventoryComponent"));
    JournalComponent = CreateDefaultSubobject<UHerbalistJournalComponent>(TEXT("JournalComponent"));
}

void AHerbalistPlayerController::BeginPlay()
{
    Super::BeginPlay();
    bShowMouseCursor = false;
    CachedWorldManager = FindWorldManager();

    if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
    {
        if (DefaultMappingContext)
        {
            Subsystem->AddMappingContext(DefaultMappingContext, 0);
        }
    }
}

void AHerbalistPlayerController::SetupInputComponent()
{
    Super::SetupInputComponent();

    if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(InputComponent))
    {
        EnhancedInputComponent->BindAction(MoveAction,           ETriggerEvent::Triggered, this, &AHerbalistPlayerController::Move);
        EnhancedInputComponent->BindAction(LookAction,           ETriggerEvent::Triggered, this, &AHerbalistPlayerController::Look);
        EnhancedInputComponent->BindAction(HarvestAction,        ETriggerEvent::Started,   this, &AHerbalistPlayerController::Harvest);
        EnhancedInputComponent->BindAction(InfoAction,           ETriggerEvent::Started,   this, &AHerbalistPlayerController::Info);
        EnhancedInputComponent->BindAction(InventoryAction,      ETriggerEvent::Started,   this, &AHerbalistPlayerController::Inventory);
        // JournalAction — Input Action asset ещё не создан в редакторе (см.
        // комментарий у JournalWidgetClass), проверка на null нужна, пока
        // остальные BindAction её не делают, потому что их actions уже
        // назначены в BP и никогда не бывают nullptr на практике.
        if (JournalAction)
        {
            EnhancedInputComponent->BindAction(JournalAction, ETriggerEvent::Started, this, &AHerbalistPlayerController::Journal);
        }
        EnhancedInputComponent->BindAction(ApplyAlchemyAction,   ETriggerEvent::Started,   this, &AHerbalistPlayerController::ApplyAlchemy);
        EnhancedInputComponent->BindAction(InteractAction,       ETriggerEvent::Started,   this, &AHerbalistPlayerController::Interact);
        EnhancedInputComponent->BindAction(UsePotionAction,      ETriggerEvent::Started,   this, &AHerbalistPlayerController::OnUsePotion);
    }
}

// ============================================================================
// ПЕРЕДВИЖЕНИЕ
// ============================================================================

void AHerbalistPlayerController::Move(const FInputActionValue& Value)
{
    const FVector2D MovementVector = Value.Get<FVector2D>();
    if (APawn* ControlledPawn = GetPawn())
    {
        ControlledPawn->AddMovementInput(ControlledPawn->GetActorForwardVector(), MovementVector.Y);
        ControlledPawn->AddMovementInput(ControlledPawn->GetActorRightVector(),   MovementVector.X);
    }
}

void AHerbalistPlayerController::Look(const FInputActionValue& Value)
{
    const FVector2D LookVector = Value.Get<FVector2D>();
    AddYawInput(LookVector.X);
    AddPitchInput(LookVector.Y);
}

// ============================================================================
// ВСПОМОГАТЕЛЬНЫЕ
// ============================================================================

AGridWorldManager* AHerbalistPlayerController::FindWorldManager() const
{
    if (CachedWorldManager)
    {
        return CachedWorldManager;
    }

    for (TActorIterator<AGridWorldManager> It(GetWorld()); It; ++It)
    {
        return *It;
    }
    return nullptr;
}

void AHerbalistPlayerController::GetCellFromHit(const FHitResult& Hit, int32& OutX, int32& OutY) const
{
    OutX = -1;
    OutY = -1;

    AGridWorldManager* WorldManager = FindWorldManager();
    if (!WorldManager) return;

    WorldManager->WorldPositionToCell(Hit.Location, OutX, OutY);
}

void AHerbalistPlayerController::UpdateDistortionFromCell(int32 X, int32 Y)
{
    AGridWorldManager* WorldManager = FindWorldManager();
    if (!WorldManager) return;

    // ComputePerceptionDistortion (16_Entity_Manifestation) — базовое Distortion
    // клетки плюс ночная надбавка (Морочники) и местные проявления.
    if (WorldManager->GetCell(X, Y))
    {
        CurrentGlobalDistortion = WorldManager->ComputePerceptionDistortion(X, Y);
    }
}

bool AHerbalistPlayerController::GetHitResultFromCamera(FHitResult& OutHit, ECollisionChannel Channel)
{
    FVector CameraLocation;
    FRotator CameraRotation;
    GetPlayerViewPoint(CameraLocation, CameraRotation);

    const FVector End = CameraLocation + CameraRotation.Vector() * 1000.0f;
    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActor(GetPawn());

    const bool bHit = GetWorld()->LineTraceSingleByChannel(OutHit, CameraLocation, End, Channel, QueryParams);

    DrawDebugLine(GetWorld(), CameraLocation, End, bHit ? FColor::Green : FColor::Red, false, 1.0f, 0, 2.0f);
    if (bHit)
    {
        DrawDebugSphere(GetWorld(), OutHit.Location, 10.0f, 12, FColor::Yellow, false, 1.0f);
    }
    return bHit;
}

bool AHerbalistPlayerController::CanHarvestActor(AActor* TargetActor) const
{
    if (!TargetActor || !GetPawn()) return false;
    const float Distance = FVector::Dist(GetPawn()->GetActorLocation(), TargetActor->GetActorLocation());
    return Distance <= MaxHarvestDistance;
}

// ============================================================================
// СБОР УРОЖАЯ
// ============================================================================

void AHerbalistPlayerController::Harvest()
{
    // GetHitResultUnderCursor читает позицию МЫШИ -- бессмысленно от первого
    // лица (bShowMouseCursor=false в BeginPlay, курсор не двигается вместе с
    // камерой). Каждый другой интерактивный обработчик в этом файле (Info/
    // Interact/ApplyAlchemy/UsePotion) уже использует GetHitResultFromCamera
    // -- сбор урожая, главный игровой глагол, был единственным исключением.
    // Второй канал (ECC_GameTraceChannel1) сохранён как и был -- отдельный
    // trace channel специально для собираемых ресурсов, не блокирующих
    // обычную видимость.
    FHitResult Hit;
    if (!GetHitResultFromCamera(Hit, ECC_Visibility))
    {
        if (!GetHitResultFromCamera(Hit, ECC_GameTraceChannel1))
        {
            return;
        }
    }

    // Сбор актора ресурса
    if (AHerbalistResourceActor* Resource = Cast<AHerbalistResourceActor>(Hit.GetActor()))
    {
        TryHarvestResource(Resource);
        return;
    }

    // Сбор воды (без актора)
    AGridWorldManager* WorldManager = FindWorldManager();
    if (!WorldManager) return;

    if (GetPawn())
    {
        const float Dist = FVector::Dist(GetPawn()->GetActorLocation(), Hit.Location);
        if (Dist > MaxHarvestDistance) return;
    }

    int32 X, Y;
    GetCellFromHit(Hit, X, Y);
    if (X < 0) return;

    FGridCell* Cell = WorldManager->GetCell(X, Y);
    if (!Cell || !Cell->bIsWater) return;

    WorldManager->CollectWater(X, Y);
    UE_LOG(LogHerbalistPlayer, Log, TEXT("Collected water from cell (%d,%d)"), X, Y);
}

bool AHerbalistPlayerController::TryHarvestResource(AHerbalistResourceActor* Resource)
{
    if (!Resource) return false;

    if (!CanHarvestActor(Resource))
    {
        UE_LOG(LogHerbalistPlayer, Warning, TEXT("Too far to harvest %s"), *Resource->GetName());
        return false;
    }

    if (Resource->IsBeingHarvested())
    {
        UE_LOG(LogHerbalistPlayer, Verbose, TEXT("%s is already being harvested"), *Resource->GetName());
        return false;
    }

    Resource->Harvest();
    return true;
}

// ============================================================================
// ИНФОРМАЦИЯ / ПРАВЫЙ КЛИК
// ============================================================================

void AHerbalistPlayerController::Info()
{
    OnRightClick();
}

void AHerbalistPlayerController::OnRightClick()
{
    FHitResult Hit;
    if (!GetHitResultFromCamera(Hit)) return;

    int32 X, Y;
    GetCellFromHit(Hit, X, Y);
    if (X < 0) return;

    UpdateDistortionFromCell(X, Y);

    AGridWorldManager* WorldManager = FindWorldManager();
    if (WorldManager)
    {
        WorldManager->SelectCell(X, Y);
        UE_LOG(LogHerbalistPlayer, Log, TEXT("Cell info: %s"), *WorldManager->GetSelectedCellInfo());
    }
}

// ============================================================================
// ИНВЕНТАРЬ
// ============================================================================

void AHerbalistPlayerController::Inventory()
{
    if (bIsAnyWidgetOpen && InventoryWidgetInstance && InventoryWidgetInstance->IsInViewport())
    {
        CloseAnyWidget();
        return;
    }

    if (bIsAnyWidgetOpen) return;

    if (InventoryWidgetInstance)
    {
        InventoryWidgetInstance->RemoveFromParent();
        InventoryWidgetInstance = nullptr;
    }

    if (!InventoryWidgetClass || !InventoryComponent) return;

    InventoryWidgetInstance = CreateWidget<UInventoryWidget>(GetWorld(), InventoryWidgetClass);
    if (!InventoryWidgetInstance) return;

    InventoryWidgetInstance->BindInventory(InventoryComponent);
    InventoryWidgetInstance->AddToViewport();

    bShowMouseCursor = true;
    FInputModeGameAndUI InputMode;
    InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
    SetInputMode(InputMode);
    SetIgnoreLookInput(true);
    bIsAnyWidgetOpen = true;
}

// ============================================================================
// АЛХИМИЯ
// ============================================================================

void AHerbalistPlayerController::ApplyAlchemy()
{
    OnApplyAlchemyKey();
}

void AHerbalistPlayerController::OnApplyAlchemyKey()
{
    FHitResult Hit;
    if (!GetHitResultFromCamera(Hit)) return;

    int32 X, Y;
    GetCellFromHit(Hit, X, Y);
    if (X < 0) return;

    ApplyTest(X, Y);
}

void AHerbalistPlayerController::OnUsePotion()
{
    UsePotion();
}

void AHerbalistPlayerController::UsePotion()
{
    if (!InventoryComponent) return;

    AGridWorldManager* WorldManager = FindWorldManager();
    if (!WorldManager) return;

    const TArray<FInventoryItem>& Items = InventoryComponent->GetItems();
    const int32 PotionIndex = Items.IndexOfByPredicate([](const FInventoryItem& Item)
    {
        return Item.IngredientID == FName(TEXT("Potion")) && Item.Count > 0;
    });

    if (PotionIndex == INDEX_NONE) return;

    FHitResult Hit;
    if (!GetHitResultFromCamera(Hit)) return;

    int32 X, Y;
    GetCellFromHit(Hit, X, Y);
    if (X < 0 || !WorldManager->GetCell(X, Y)) return;

    WorldManager->ApplyPotionToCell(X, Y, Items[PotionIndex].State);
    InventoryComponent->RemoveItem(PotionIndex, 1);
}

// ============================================================================
// ВЗАИМОДЕЙСТВИЕ
// ============================================================================

void AHerbalistPlayerController::Interact()
{
    if (CurrentAlchemyWidget && CurrentAlchemyWidget->IsInViewport())
    {
        CloseAnyWidget();
        return;
    }

    FHitResult Hit;
    if (!GetHitResultFromCamera(Hit)) return;

    AActor* HitActor = Hit.GetActor();

    // Один интерфейс вместо цепочки Cast<> на каждый интерактивный класс
    // (2026-08-30, "заводим родительские классы для сущностей и связки") —
    // см. Core/Interaction/Interactable.h. AHerbalistResourceActor::Harvest()
    // намеренно не через этот путь, см. комментарий там же.
    if (HitActor && HitActor->Implements<UInteractable>())
    {
        IInteractable::Execute_OnInteract(HitActor, this);
    }
}

// ============================================================================
// ЗАКРЫТИЕ ВИДЖЕТОВ
// ============================================================================

void AHerbalistPlayerController::CloseAnyWidget()
{
    if (InventoryWidgetInstance && InventoryWidgetInstance->IsInViewport())
    {
        InventoryWidgetInstance->RemoveFromParent();
        InventoryWidgetInstance = nullptr;
    }

    if (JournalWidgetInstance && JournalWidgetInstance->IsInViewport())
    {
        JournalWidgetInstance->RemoveFromParent();
        JournalWidgetInstance = nullptr;
    }

    if (CurrentAlchemyWidget && CurrentAlchemyWidget->IsInViewport())
    {
        CurrentAlchemyWidget->RemoveFromParent();
        CurrentAlchemyWidget = nullptr;
        CurrentAlchemyTable = nullptr;
    }

    if (CurrentTransferWidget && CurrentTransferWidget->IsInViewport())
    {
        CurrentTransferWidget->RemoveFromParent();
        CurrentTransferWidget = nullptr;
    }

    bShowMouseCursor = false;
    FInputModeGameOnly GameMode;
    SetInputMode(GameMode);
    SetIgnoreLookInput(false);
    bIsAnyWidgetOpen = false;
}

// ============================================================================
// ТЕСТОВЫЕ КОМАНДЫ (СТАРЫЕ)
// ============================================================================

void AHerbalistPlayerController::HarvestTest(int32 X, int32 Y)
{
    if (AGridWorldManager* WorldManager = FindWorldManager())
    {
        WorldManager->HarvestTest(X, Y);
    }
}

void AHerbalistPlayerController::ApplyTest(int32 X, int32 Y)
{
    if (AGridWorldManager* WorldManager = FindWorldManager())
    {
        WorldManager->ApplyTest(X, Y);
    }
}

void AHerbalistPlayerController::ShowInventory()
{
    Inventory();
}

void AHerbalistPlayerController::Journal()
{
    if (bIsAnyWidgetOpen && JournalWidgetInstance && JournalWidgetInstance->IsInViewport())
    {
        CloseAnyWidget();
        return;
    }

    if (bIsAnyWidgetOpen) return;

    if (JournalWidgetInstance)
    {
        JournalWidgetInstance->RemoveFromParent();
        JournalWidgetInstance = nullptr;
    }

    if (!JournalComponent) return;

    // JournalLogWidget не требует JournalWidgetClass/WBP вовсе — строит своё
    // дерево в C++ (см. UI/JournalLogWidget.h). Голый StaticClass() уже
    // достаточно, работает без единого шага в редакторе.
    UJournalLogWidget* LogWidget = CreateWidget<UJournalLogWidget>(GetWorld(), UJournalLogWidget::StaticClass());
    if (!LogWidget) return;

    LogWidget->BindJournal(JournalComponent);
    LogWidget->BindWorldManager(FindWorldManager());
    LogWidget->AddToViewport();
    JournalWidgetInstance = LogWidget;

    bShowMouseCursor = true;
    FInputModeGameAndUI InputMode;
    InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
    SetInputMode(InputMode);
    SetIgnoreLookInput(true);
    bIsAnyWidgetOpen = true;
}

// UMemoryRevealWidget.h -- вызывается извне (AGridWorldManager::
// CollectMemoryFragment/CheckBuyanCondition), не завязан на состояние
// bIsAnyWidgetOpen/CloseAnyWidget() намеренно: попап всплывает поверх
// обычной игры сам по себе, не блокирует ввод и не конкурирует с
// Инвентарём/Травником за тот же канал -- игрок может продолжать двигаться,
// пока текст на экране.
void AHerbalistPlayerController::ShowMemoryRevealText(const FText& Text)
{
    if (!MemoryRevealWidgetInstance)
    {
        MemoryRevealWidgetInstance = CreateWidget<UMemoryRevealWidget>(GetWorld(), UMemoryRevealWidget::StaticClass());
    }
    if (!MemoryRevealWidgetInstance) return;

    const UHerbalistSettings* Settings = GetHerbalistSettings();
    const float DisplaySeconds = Settings ? Settings->MemoryRevealDisplaySeconds : 7.0f;
    MemoryRevealWidgetInstance->Show(Text, DisplaySeconds);
}

void AHerbalistPlayerController::ToggleJournalUI()
{
    Journal();
}

void AHerbalistPlayerController::SaveGame()
{
    if (UGameInstance* GI = GetGameInstance())
    {
        if (UHerbalistSaveSubsystem* SaveSubsystem = GI->GetSubsystem<UHerbalistSaveSubsystem>())
        {
            SaveSubsystem->SaveGame();
        }
    }
}

void AHerbalistPlayerController::LoadGame()
{
    if (UGameInstance* GI = GetGameInstance())
    {
        if (UHerbalistSaveSubsystem* SaveSubsystem = GI->GetSubsystem<UHerbalistSaveSubsystem>())
        {
            SaveSubsystem->LoadGame();
        }
    }
}

void AHerbalistPlayerController::MassHarvestTest(int32 X, int32 Y, int32 Count)
{
    if (AGridWorldManager* WorldManager = FindWorldManager())
    {
        WorldManager->MassHarvestTest(X, Y, Count);
    }
}

// ============================================================================
// ТЕСТОВЫЕ КОМАНДЫ (НОВЫЙ ПАЙПЛАЙН)
// ============================================================================

void AHerbalistPlayerController::TestNewHarvest(int32 X, int32 Y, FName IngredientID)
{
    AGridWorldManager* Grid = FindWorldManager();
    if (!Grid) return;

    FCommandEntry Cmd;
    Cmd.Primitive                = ECommandPrimitive::Harvest;
    Cmd.Harvest.TargetCell       = FIntPoint(X, Y);
    Cmd.Harvest.IngredientID     = IngredientID;
    Cmd.Harvest.Amount           = 1;
    Grid->QueueCommand(Cmd);
}

void AHerbalistPlayerController::TestNewTransfer(FName IngredientID, int32 Amount)
{
    AGridWorldManager* Grid = FindWorldManager();
    if (!Grid) return;

    FCommandEntry Cmd;
    Cmd.Primitive                     = ECommandPrimitive::Transfer;
    Cmd.Transfer.SourceContainerID    = 0;
    Cmd.Transfer.TargetContainerID    = 1;
    Cmd.Transfer.IngredientID         = IngredientID;
    Cmd.Transfer.Amount               = Amount;
    Grid->QueueCommand(Cmd);
}

void AHerbalistPlayerController::TestNewApply(int32 X, int32 Y, FString IngredientList)
{
    if (!InventoryComponent) return;

    TArray<FInventoryItem> Items;
    TArray<FString> Names;
    IngredientList.ParseIntoArray(Names, TEXT(","), true);
    for (const FString& Name : Names)
    {
        const FName IngID(*Name);
        for (const FInventoryItem& Item : InventoryComponent->GetItems())
        {
            if (Item.IngredientID == IngID)
            {
                Items.Add(Item);
                break;
            }
        }
    }

    if (Items.Num() == 0) return;

    AGridWorldManager* Grid = FindWorldManager();
    if (!Grid) return;

    FCommandEntry Cmd;
    Cmd.Primitive             = ECommandPrimitive::Apply;
    Cmd.Apply.TargetCell      = FIntPoint(X, Y);
    Cmd.Apply.Ingredients     = Items;
    // Coherence считается Pipeline'ом из Ingredients (ComputeIntentCoherence).
    Grid->QueueCommand(Cmd);
}