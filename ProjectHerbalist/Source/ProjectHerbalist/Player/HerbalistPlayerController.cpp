// HerbalistPlayerController.cpp
#include "Player/HerbalistPlayerController.h"
#include "ProjectHerbalist.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "DrawDebugHelpers.h"
#include "Core/World/GridWorldManager.h"
#include "Core/Storage/StorageContainer.h"
#include "Core/Storage/AlchemyTableActor.h"
#include "UI/InventoryWidget.h"
#include "UI/AlchemyTransferWidget.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"

AHerbalistPlayerController::AHerbalistPlayerController()
{
    InventoryComponent = CreateDefaultSubobject<UHerbalistInventoryComponent>(TEXT("InventoryComponent"));
}

void AHerbalistPlayerController::BeginPlay()
{
    Super::BeginPlay();
    bShowMouseCursor = false;

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

void AHerbalistPlayerController::Harvest() { OnLeftClick(); }
void AHerbalistPlayerController::Info() { OnRightClick(); }
void AHerbalistPlayerController::ApplyAlchemy() { OnApplyAlchemyKey(); }

void AHerbalistPlayerController::CloseAnyWidget()
{
    UE_LOG(LogHerbalist, Log, TEXT("CloseAnyWidget called"));

    if (InventoryWidgetInstance && InventoryWidgetInstance->IsInViewport())
    {
        InventoryWidgetInstance->RemoveFromParent();
        InventoryWidgetInstance = nullptr;
        UE_LOG(LogHerbalist, Log, TEXT("Inventory widget removed"));
    }
    
    if (CurrentAlchemyWidget && CurrentAlchemyWidget->IsInViewport())
    {
        CurrentAlchemyWidget->RemoveFromParent();
        CurrentAlchemyWidget = nullptr;
        CurrentAlchemyTable = nullptr;
        UE_LOG(LogHerbalist, Log, TEXT("Alchemy widget removed"));
    }

    bShowMouseCursor = false;
    FInputModeGameOnly GameMode;
    SetInputMode(GameMode);
    SetIgnoreLookInput(false);
    bIsAnyWidgetOpen = false;

    UE_LOG(LogHerbalist, Log, TEXT("CloseAnyWidget finished, bIsAnyWidgetOpen=%d"), bIsAnyWidgetOpen);
}

void AHerbalistPlayerController::Inventory()
{
    UE_LOG(LogHerbalist, Log, TEXT("Inventory() called, bIsAnyWidgetOpen=%d, InventoryWidgetInstance=%p"),
        bIsAnyWidgetOpen, InventoryWidgetInstance);

    if (bIsAnyWidgetOpen && InventoryWidgetInstance && InventoryWidgetInstance->IsInViewport())
    {
        UE_LOG(LogHerbalist, Log, TEXT("Inventory widget is open, closing"));
        CloseAnyWidget();
        return;
    }

    if (bIsAnyWidgetOpen)
    {
        UE_LOG(LogHerbalist, Log, TEXT("Inventory: another widget is open, ignoring"));
        return;
    }

    if (InventoryWidgetInstance)
    {
        InventoryWidgetInstance->Destruct();
        InventoryWidgetInstance = nullptr;
    }

    if (!InventoryWidgetClass)
    {
        UE_LOG(LogHerbalist, Error, TEXT("InventoryWidgetClass is null"));
        return;
    }

    InventoryWidgetInstance = CreateWidget<UInventoryWidget>(GetWorld(), InventoryWidgetClass);
    if (!InventoryWidgetInstance)
    {
        UE_LOG(LogHerbalist, Error, TEXT("Failed to create widget"));
        return;
    }

    if (!InventoryComponent)
    {
        UE_LOG(LogHerbalist, Error, TEXT("InventoryComponent is null"));
        return;
    }

    InventoryWidgetInstance->BindInventory(InventoryComponent);
    InventoryWidgetInstance->AddToViewport();

    bShowMouseCursor = true;
    FInputModeGameAndUI InputMode;
    InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
    SetInputMode(InputMode);
    SetIgnoreLookInput(true);
    bIsAnyWidgetOpen = true;

    UE_LOG(LogHerbalist, Log, TEXT("Inventory widget created and opened, bIsAnyWidgetOpen=%d"), bIsAnyWidgetOpen);
}

bool AHerbalistPlayerController::GetHitResultFromCamera(FHitResult& OutHit)
{
    FVector CameraLocation;
    FRotator CameraRotation;
    GetPlayerViewPoint(CameraLocation, CameraRotation);
    FVector End = CameraLocation + CameraRotation.Vector() * 500.0f;
    DrawDebugLine(GetWorld(), CameraLocation, End, FColor::Green, false, 0.5f, 0, 1.0f);
    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActor(GetPawn());
    return GetWorld()->LineTraceSingleByChannel(OutHit, CameraLocation, End, ECC_Visibility, QueryParams);
}

void AHerbalistPlayerController::OnLeftClick()
{
    FHitResult Hit;
    if (GetHitResultFromCamera(Hit))
    {
        AGridWorldManager* WorldManager = nullptr;
        for (TActorIterator<AGridWorldManager> It(GetWorld()); It; ++It) { WorldManager = *It; break; }
        if (WorldManager)
        {
            FVector LocalLoc = Hit.Location - WorldManager->GetActorLocation();
            int32 X = FMath::RoundToInt(LocalLoc.X / WorldManager->CellSize);
            int32 Y = FMath::RoundToInt(LocalLoc.Y / WorldManager->CellSize);
            if (X >= 0 && X < WorldManager->GridSizeX && Y >= 0 && Y < WorldManager->GridSizeY)
            {
                DrawDebugLine(GetWorld(), Hit.Location, Hit.Location + FVector(0, 0, 100), FColor::Yellow, false, 1.0f, 0, 2.0f);
                HarvestTest(X, Y);
            }
        }
    }
}

void AHerbalistPlayerController::OnRightClick()
{
    FHitResult Hit;
    if (GetHitResultFromCamera(Hit))
    {
        AGridWorldManager* WorldManager = nullptr;
        for (TActorIterator<AGridWorldManager> It(GetWorld()); It; ++It) { WorldManager = *It; break; }
        if (WorldManager)
        {
            FVector LocalLoc = Hit.Location - WorldManager->GetActorLocation();
            int32 X = FMath::RoundToInt(LocalLoc.X / WorldManager->CellSize);
            int32 Y = FMath::RoundToInt(LocalLoc.Y / WorldManager->CellSize);
            if (X >= 0 && X < WorldManager->GridSizeX && Y >= 0 && Y < WorldManager->GridSizeY)
            {
                DrawDebugLine(GetWorld(), Hit.Location, Hit.Location + FVector(0, 0, 100), FColor::Red, false, 1.0f, 0, 2.0f);
                WorldManager->SelectCell(X, Y);
                FString Info = WorldManager->GetSelectedCellInfo();
                UE_LOG(LogHerbalist, Log, TEXT("Cell info: %s"), *Info);
            }
        }
    }
}

void AHerbalistPlayerController::OnApplyAlchemyKey()
{
    FHitResult Hit;
    if (GetHitResultFromCamera(Hit))
    {
        AGridWorldManager* WorldManager = nullptr;
        for (TActorIterator<AGridWorldManager> It(GetWorld()); It; ++It) { WorldManager = *It; break; }
        if (WorldManager)
        {
            FVector LocalLoc = Hit.Location - WorldManager->GetActorLocation();
            int32 X = FMath::RoundToInt(LocalLoc.X / WorldManager->CellSize);
            int32 Y = FMath::RoundToInt(LocalLoc.Y / WorldManager->CellSize);
            if (X >= 0 && X < WorldManager->GridSizeX && Y >= 0 && Y < WorldManager->GridSizeY)
            {
                DrawDebugLine(GetWorld(), Hit.Location, Hit.Location + FVector(0, 0, 100), FColor::Blue, false, 1.0f, 0, 2.0f);
                ApplyTest(X, Y);
            }
        }
    }
}

void AHerbalistPlayerController::Interact()
{
    UE_LOG(LogHerbalist, Log, TEXT("Interact() called (E key), bIsAnyWidgetOpen=%d"), bIsAnyWidgetOpen);

    FHitResult Hit;
    if (GetHitResultFromCamera(Hit))
    {
        AActor* HitActor = Hit.GetActor();
        UE_LOG(LogHerbalist, Log, TEXT("Hit actor: %s"), HitActor ? *HitActor->GetName() : TEXT("None"));

        AStorageContainer* Storage = Cast<AStorageContainer>(HitActor);
        if (Storage)
        {
            UE_LOG(LogHerbalist, Log, TEXT("Storage detected, calling OnInteract"));
            Storage->OnInteract(this);
            return;
        }
        
        AAlchemyTableActor* AlchemyTable = Cast<AAlchemyTableActor>(HitActor);
        if (AlchemyTable)
        {
            UE_LOG(LogHerbalist, Log, TEXT("Alchemy table detected, calling OnInteract"));
            AlchemyTable->OnInteract(this);
            return;
        }

        // Иначе – сбор ресурса (не мешаем виджетам)
        AGridWorldManager* WorldManager = nullptr;
        for (TActorIterator<AGridWorldManager> It(GetWorld()); It; ++It) { WorldManager = *It; break; }
        if (WorldManager)
        {
            FVector LocalLoc = Hit.Location - WorldManager->GetActorLocation();
            int32 X = FMath::RoundToInt(LocalLoc.X / WorldManager->CellSize);
            int32 Y = FMath::RoundToInt(LocalLoc.Y / WorldManager->CellSize);
            if (X >= 0 && X < WorldManager->GridSizeX && Y >= 0 && Y < WorldManager->GridSizeY)
            {
                DrawDebugLine(GetWorld(), Hit.Location, Hit.Location + FVector(0, 0, 100), FColor::Cyan, false, 1.0f, 0, 2.0f);
                HarvestTest(X, Y);
            }
        }
    }
}

void AHerbalistPlayerController::HarvestTest(int32 X, int32 Y)
{
    AGridWorldManager* WorldManager = nullptr;
    for (TActorIterator<AGridWorldManager> It(GetWorld()); It; ++It) { WorldManager = *It; break; }
    if (WorldManager) WorldManager->HarvestTest(X, Y);
    else UE_LOG(LogHerbalist, Warning, TEXT("No GridWorldManager found"));
}

void AHerbalistPlayerController::ApplyTest(int32 X, int32 Y)
{
    AGridWorldManager* WorldManager = nullptr;
    for (TActorIterator<AGridWorldManager> It(GetWorld()); It; ++It) { WorldManager = *It; break; }
    if (WorldManager) WorldManager->ApplyTest(X, Y);
    else UE_LOG(LogHerbalist, Warning, TEXT("No GridWorldManager found"));
}

void AHerbalistPlayerController::ShowInventory()
{
    Inventory();
}

void AHerbalistPlayerController::MassHarvestTest(int32 X, int32 Y, int32 Count)
{
    AGridWorldManager* WorldManager = nullptr;
    for (TActorIterator<AGridWorldManager> It(GetWorld()); It; ++It) { WorldManager = *It; break; }
    if (WorldManager) WorldManager->MassHarvestTest(X, Y, Count);
    else UE_LOG(LogHerbalist, Warning, TEXT("No GridWorldManager found"));
}

void AHerbalistPlayerController::OnUsePotion()
{
    UsePotion();
}

void AHerbalistPlayerController::UsePotion()
{
    if (!InventoryComponent) return;

    // Найти WorldManager
    AGridWorldManager* WorldManager = nullptr;
    for (TActorIterator<AGridWorldManager> It(GetWorld()); It; ++It)
    {
        WorldManager = *It;
        break;
    }
    if (!WorldManager) return;

    int32 PotionSlotIndex = -1;
    FInventoryItem PotionItem;
    const TArray<FInventoryItem>& Items = InventoryComponent->GetItems();
    for (int32 i = 0; i < Items.Num(); ++i)
    {
        if (Items[i].Type == EResourceType::Potion && Items[i].Count > 0)
        {
            PotionSlotIndex = i;
            PotionItem = Items[i];
            break;
        }
    }

    if (PotionSlotIndex == -1)
    {
        UE_LOG(LogHerbalist, Warning, TEXT("No potion in inventory"));
        return;
    }

    FHitResult Hit;
    if (!GetHitResultFromCamera(Hit)) return;

    FVector LocalLoc = Hit.Location - WorldManager->GetActorLocation();
    int32 X = FMath::RoundToInt(LocalLoc.X / WorldManager->CellSize);
    int32 Y = FMath::RoundToInt(LocalLoc.Y / WorldManager->CellSize);
    if (!WorldManager->GetCell(X, Y)) return;

    WorldManager->ApplyPotionToCell(X, Y, PotionItem.State);
    InventoryComponent->RemoveItem(PotionSlotIndex, 1);
}