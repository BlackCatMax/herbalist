// HerbalistPlayerController.cpp
#include "Player/HerbalistPlayerController.h"
#include "Core/Resources/AHerbalistResourceActor.h"
#include "Core/Storage/AlchemyTableActor.h"
#include "Core/Storage/StorageContainer.h"
#include "Core/Subsystems/IngredientRegistrySubsystem.h"
#include "Core/World/GridWorldManager.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "ProjectHerbalist.h"
#include "UI/AlchemyTransferWidget.h"
#include "UI/InventoryTransferWidget.h"
#include "UI/InventoryWidget.h"

AHerbalistPlayerController::AHerbalistPlayerController()
{
    InventoryComponent = CreateDefaultSubobject<UHerbalistInventoryComponent>(TEXT("InventoryComponent"));
}

void AHerbalistPlayerController::BeginPlay()
{
    Super::BeginPlay();
    bShowMouseCursor = false;
    CachedWorldManager = FindWorldManager();

    if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
    {
        if (DefaultMappingContext)
            Subsystem->AddMappingContext(DefaultMappingContext, 0);
    }
}

void AHerbalistPlayerController::SetupInputComponent()
{
    Super::SetupInputComponent();

    if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(InputComponent))
    {
        EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AHerbalistPlayerController::Move);
        EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &AHerbalistPlayerController::Look);
        EnhancedInputComponent->BindAction(HarvestAction, ETriggerEvent::Started, this, &AHerbalistPlayerController::Harvest);
        EnhancedInputComponent->BindAction(InfoAction, ETriggerEvent::Started, this, &AHerbalistPlayerController::Info);
        EnhancedInputComponent->BindAction(InventoryAction, ETriggerEvent::Started, this, &AHerbalistPlayerController::Inventory);
        EnhancedInputComponent->BindAction(ApplyAlchemyAction, ETriggerEvent::Started, this, &AHerbalistPlayerController::ApplyAlchemy);
        EnhancedInputComponent->BindAction(InteractAction, ETriggerEvent::Started, this, &AHerbalistPlayerController::Interact);
        EnhancedInputComponent->BindAction(UsePotionAction, ETriggerEvent::Started, this, &AHerbalistPlayerController::OnUsePotion);
    }
}

void AHerbalistPlayerController::Move(const FInputActionValue& Value)
{
    FVector2D MovementVector = Value.Get<FVector2D>();
    if (APawn* ControlledPawn = GetPawn())
    {
        ControlledPawn->AddMovementInput(ControlledPawn->GetActorForwardVector(), MovementVector.Y);
        ControlledPawn->AddMovementInput(ControlledPawn->GetActorRightVector(), MovementVector.X);
    }
}

void AHerbalistPlayerController::Look(const FInputActionValue& Value)
{
    FVector2D LookVector = Value.Get<FVector2D>();
    AddYawInput(LookVector.X);
    AddPitchInput(LookVector.Y);
}

void AHerbalistPlayerController::Harvest()
{
    FHitResult Hit;
    if (!GetHitResultUnderCursor(ECC_Visibility, false, Hit))
    {
        if (!GetHitResultUnderCursor(ECC_GameTraceChannel1, false, Hit))
            return;
    }

    AHerbalistResourceActor* Resource = Cast<AHerbalistResourceActor>(Hit.GetActor());
    if (Resource)
    {
        TryHarvestResource(Resource);
        return;
    }

    // Автоматический сбор клетки отключён (только через акторы)
}

void AHerbalistPlayerController::Info()
{
    OnRightClick();
}

void AHerbalistPlayerController::ApplyAlchemy()
{
    OnApplyAlchemyKey();
}

void AHerbalistPlayerController::CloseAnyWidget()
{
    if (InventoryWidgetInstance && InventoryWidgetInstance->IsInViewport())
    {
        InventoryWidgetInstance->RemoveFromParent();
        InventoryWidgetInstance = nullptr;
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

bool AHerbalistPlayerController::GetHitResultFromCamera(FHitResult& OutHit)
{
    FVector CameraLocation;
    FRotator CameraRotation;
    GetPlayerViewPoint(CameraLocation, CameraRotation);
    FVector End = CameraLocation + CameraRotation.Vector() * 1000.0f;
    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActor(GetPawn());
    bool bHit = GetWorld()->LineTraceSingleByChannel(OutHit, CameraLocation, End, ECC_Visibility, QueryParams);

    // Визуализация луча для отладки
    DrawDebugLine(GetWorld(), CameraLocation, End, bHit ? FColor::Green : FColor::Red, false, 1.0f, 0, 2.0f);
    if (bHit)
    {
        DrawDebugSphere(GetWorld(), OutHit.Location, 10.0f, 12, FColor::Yellow, false, 1.0f);
    }
    return bHit;
}

AGridWorldManager* AHerbalistPlayerController::FindWorldManager() const
{
    if (CachedWorldManager)
        return CachedWorldManager;

    for (TActorIterator<AGridWorldManager> It(GetWorld()); It; ++It)
    {
        return *It;
    }
    return nullptr;
}

void AHerbalistPlayerController::GetCellFromHit(const FHitResult& Hit, int32& OutX, int32& OutY) const
{
    OutX = -1; OutY = -1;
    AGridWorldManager* WorldManager = FindWorldManager();
    if (!WorldManager) return;

    FVector LocalLoc = Hit.Location - WorldManager->GetActorLocation();
    int32 X = FMath::FloorToInt(LocalLoc.X / WorldManager->CellSize);
    int32 Y = FMath::FloorToInt(LocalLoc.Y / WorldManager->CellSize);
    if (X >= 0 && X < WorldManager->GridSizeX && Y >= 0 && Y < WorldManager->GridSizeY)
    {
        OutX = X;
        OutY = Y;
    }
}

void AHerbalistPlayerController::UpdateDistortionFromCell(int32 X, int32 Y)
{
    AGridWorldManager* WorldManager = FindWorldManager();
    if (!WorldManager) return;
    if (FGridCell* Cell = WorldManager->GetCell(X, Y))
    {
        CurrentGlobalDistortion = Cell->Memory.AccumulatedDistortion;
    }
}

void AHerbalistPlayerController::OnLeftClick()
{
    FHitResult Hit;
    if (!GetHitResultFromCamera(Hit)) return;

    int32 X, Y;
    GetCellFromHit(Hit, X, Y);
    if (X < 0) return;

    UpdateDistortionFromCell(X, Y);
    HarvestTest(X, Y);
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
        UE_LOG(LogHerbalist, Log, TEXT("Cell info: %s"), *WorldManager->GetSelectedCellInfo());
    }
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

    if (AStorageContainer* Storage = Cast<AStorageContainer>(HitActor))
    {
        Storage->OnInteract(this);
        return;
    }

    if (AAlchemyTableActor* AlchemyTable = Cast<AAlchemyTableActor>(HitActor))
    {
        AlchemyTable->OnInteract(this);
        return;
    }
}

void AHerbalistPlayerController::HarvestTest(int32 X, int32 Y)
{
    AGridWorldManager* WorldManager = FindWorldManager();
    if (WorldManager) WorldManager->HarvestTest(X, Y);
}

void AHerbalistPlayerController::ApplyTest(int32 X, int32 Y)
{
    AGridWorldManager* WorldManager = FindWorldManager();
    if (WorldManager) WorldManager->ApplyTest(X, Y);
}

void AHerbalistPlayerController::ShowInventory()
{
    Inventory();
}

void AHerbalistPlayerController::MassHarvestTest(int32 X, int32 Y, int32 Count)
{
    AGridWorldManager* WorldManager = FindWorldManager();
    if (WorldManager) WorldManager->MassHarvestTest(X, Y, Count);
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
    int32 PotionIndex = Items.IndexOfByPredicate([](const FInventoryItem& Item) {
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

bool AHerbalistPlayerController::CanHarvestActor(AActor* TargetActor) const
{
    if (!TargetActor || !GetPawn()) return false;
    float Distance = FVector::Dist(GetPawn()->GetActorLocation(), TargetActor->GetActorLocation());
    return Distance <= MaxHarvestDistance;
}

bool AHerbalistPlayerController::TryHarvestResource(AHerbalistResourceActor* Resource)
{
    if (!Resource) return false;

    if (!CanHarvestActor(Resource))
    {
        UE_LOG(LogHerbalist, Warning, TEXT("Too far to harvest %s"), *Resource->GetName());
        return false;
    }

    if (Resource->IsBeingHarvested())
    {
        UE_LOG(LogHerbalist, Verbose, TEXT("%s is already being harvested"), *Resource->GetName());
        return false;
    }

    Resource->Harvest();
    return true;
}