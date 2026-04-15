#include "HerbalistPlayerController.h"
#include "ProjectHerbalist.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "DrawDebugHelpers.h"
#include "Core/World/GridWorldManager.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "ProjectHerbalistGameModeBase.h"

void AHerbalistPlayerController::BeginPlay()
{
    Super::BeginPlay();
    // Курсор не отображаем, чтобы не мешать управлению камерой
    bShowMouseCursor = false;

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
        EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AHerbalistPlayerController::Move);
        EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &AHerbalistPlayerController::Look);
        EnhancedInputComponent->BindAction(HarvestAction, ETriggerEvent::Started, this, &AHerbalistPlayerController::Harvest);
        EnhancedInputComponent->BindAction(InfoAction, ETriggerEvent::Started, this, &AHerbalistPlayerController::Info);
        EnhancedInputComponent->BindAction(InventoryAction, ETriggerEvent::Started, this, &AHerbalistPlayerController::Inventory);
        EnhancedInputComponent->BindAction(ApplyAlchemyAction, ETriggerEvent::Started, this, &AHerbalistPlayerController::ApplyAlchemy);
        EnhancedInputComponent->BindAction(SelectResource1Action, ETriggerEvent::Started, this, &AHerbalistPlayerController::SelectResource1);
        EnhancedInputComponent->BindAction(SelectResource2Action, ETriggerEvent::Started, this, &AHerbalistPlayerController::SelectResource2);
        EnhancedInputComponent->BindAction(SelectResource3Action, ETriggerEvent::Started, this, &AHerbalistPlayerController::SelectResource3);
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
    OnLeftClick();
}

void AHerbalistPlayerController::Info()
{
    OnRightClick();
}

void AHerbalistPlayerController::Inventory()
{
    ShowInventory();
}

void AHerbalistPlayerController::ApplyAlchemy()
{
    OnApplyAlchemyKey();
}

void AHerbalistPlayerController::SelectResource1()
{
    CurrentResourceType = 0;
    GEngine->AddOnScreenDebugMessage(-1, 1.0f, FColor::Green, TEXT("Selected: Nettle"));
    UE_LOG(LogHerbalist, Log, TEXT("Selected resource type: Nettle (0)"));
}

void AHerbalistPlayerController::SelectResource2()
{
    CurrentResourceType = 1;
    GEngine->AddOnScreenDebugMessage(-1, 1.0f, FColor::Green, TEXT("Selected: Fern"));
    UE_LOG(LogHerbalist, Log, TEXT("Selected resource type: Fern (1)"));
}

void AHerbalistPlayerController::SelectResource3()
{
    CurrentResourceType = 2;
    GEngine->AddOnScreenDebugMessage(-1, 1.0f, FColor::Green, TEXT("Selected: Mushroom"));
    UE_LOG(LogHerbalist, Log, TEXT("Selected resource type: Mushroom (2)"));
}

bool AHerbalistPlayerController::GetHitResultFromCamera(FHitResult& OutHit)
{
    // Получаем точку обзора (камера)
    FVector CameraLocation;
    FRotator CameraRotation;
    GetPlayerViewPoint(CameraLocation, CameraRotation);

    // Луч из камеры вперёд на заданную дистанцию
    FVector End = CameraLocation + CameraRotation.Vector() * TraceDistance;

    // Линейная трассировка по каналу Visibility
    FCollisionQueryParams QueryParams;
    QueryParams.bTraceComplex = false;
    QueryParams.AddIgnoredActor(GetPawn()); // игнорируем своего персонажа

    bool bHit = GetWorld()->LineTraceSingleByChannel(OutHit, CameraLocation, End, ECC_Visibility, QueryParams);
    if (bHit)
    {
        // Рисуем линию для отладки (зелёную)
        DrawDebugLine(GetWorld(), CameraLocation, End, FColor::Green, false, 0.5f, 0, 1.0f);
    }
    return bHit;
}

void AHerbalistPlayerController::OnLeftClick()
{
    FHitResult Hit;
    if (GetHitResultFromCamera(Hit))
    {
        AGridWorldManager* WorldManager = nullptr;
        for (TActorIterator<AGridWorldManager> It(GetWorld()); It; ++It)
        {
            WorldManager = *It;
            break;
        }
        if (WorldManager)
        {
            // Преобразуем точку попадания в локальные координаты менеджера
            FVector LocalLoc = Hit.Location - WorldManager->GetActorLocation();
            int32 X = FMath::RoundToInt(LocalLoc.X / WorldManager->CellSize);
            int32 Y = FMath::RoundToInt(LocalLoc.Y / WorldManager->CellSize);
            if (X >= 0 && X < WorldManager->GridSizeX && Y >= 0 && Y < WorldManager->GridSizeY)
            {
                // Жёлтая линия от точки попадания вверх
                DrawDebugLine(GetWorld(), Hit.Location, Hit.Location + FVector(0, 0, 100), FColor::Yellow, false, 1.0f, 0, 2.0f);
                HarvestTest(X, Y, CurrentResourceType);
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

// ========== Exec-команды ==========
void AHerbalistPlayerController::HarvestTest(int32 X, int32 Y, int32 ResourceType)
{
    AGridWorldManager* WorldManager = nullptr;
    for (TActorIterator<AGridWorldManager> It(GetWorld()); It; ++It) { WorldManager = *It; break; }
    if (WorldManager) WorldManager->HarvestTest(X, Y, ResourceType);
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
    AGridWorldManager* WorldManager = nullptr;
    for (TActorIterator<AGridWorldManager> It(GetWorld()); It; ++It) { WorldManager = *It; break; }
    if (WorldManager) WorldManager->ShowInventory();
    else UE_LOG(LogHerbalist, Warning, TEXT("No GridWorldManager found"));
}

void AHerbalistPlayerController::MassHarvestTest(int32 X, int32 Y, int32 ResourceType, int32 Count)
{
    AGridWorldManager* WorldManager = nullptr;
    for (TActorIterator<AGridWorldManager> It(GetWorld()); It; ++It) { WorldManager = *It; break; }
    if (WorldManager) WorldManager->MassHarvestTest(X, Y, ResourceType, Count);
    else UE_LOG(LogHerbalist, Warning, TEXT("No GridWorldManager found"));
}