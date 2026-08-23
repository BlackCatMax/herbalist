// HerbalistSaveSubsystem.cpp
#include "Core/Save/HerbalistSaveSubsystem.h"
#include "Core/Save/HerbalistSaveTypes.h"
#include "Core/World/GridWorldManager.h"
#include "Core/Inventory/HerbalistInventoryComponent.h"
#include "Core/Journal/HerbalistJournalComponent.h"
#include "Player/HerbalistPlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "EngineUtils.h"
#include "ProjectHerbalist.h"
#include "HerbalistLogChannels.h"

const FString UHerbalistSaveSubsystem::DefaultSlotName = TEXT("HerbalistSave");

namespace
{
    AGridWorldManager* FindWorldManager(UWorld* World)
    {
        if (!World) return nullptr;
        for (TActorIterator<AGridWorldManager> It(World); It; ++It)
        {
            return *It;
        }
        return nullptr;
    }
}

bool UHerbalistSaveSubsystem::SaveGame(const FString& SlotName)
{
    const FString Slot = SlotName.IsEmpty() ? DefaultSlotName : SlotName;
    UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
    AGridWorldManager* WorldManager = FindWorldManager(World);
    if (!WorldManager)
    {
        UE_LOG(LogHerbalistWorld, Warning, TEXT("SaveGame: no AGridWorldManager in world, aborted"));
        return false;
    }

    UHerbalistSaveGame* Save = Cast<UHerbalistSaveGame>(UGameplayStatics::CreateSaveGameObject(UHerbalistSaveGame::StaticClass()));
    if (!Save) return false;

    Save->RngBaseSeed = WorldManager->RngBaseSeed;
    Save->GridSizeX = WorldManager->GridSizeX;
    Save->GridSizeY = WorldManager->GridSizeY;
    Save->CurrentTickID = WorldManager->GetCurrentTickID();
    Save->GameClockSeconds = WorldManager->GetGameClockSeconds();
    Save->Cells = WorldManager->CaptureSaveCells();
    Save->EntityLandmarks = WorldManager->GetEntityLandmarks();
    Save->Shrines = WorldManager->GetShrines();

    if (AHerbalistPlayerController* PC = World ? Cast<AHerbalistPlayerController>(World->GetFirstPlayerController()) : nullptr)
    {
        if (PC->InventoryComponent) Save->InventoryItems = PC->InventoryComponent->GetItems();
        if (PC->JournalComponent) Save->JournalEntries = PC->JournalComponent->GetEntries();
        if (APawn* Pawn = PC->GetPawn())
        {
            Save->PlayerLocation = Pawn->GetActorLocation();
            Save->PlayerRotation = Pawn->GetActorRotation();
        }
    }

    const bool bSuccess = UGameplayStatics::SaveGameToSlot(Save, Slot, 0);
    UE_LOG(LogHerbalistWorld, Log, TEXT("SaveGame: slot '%s' %s (%d cells, %d landmarks, %d inventory items, %d journal entries)"),
        *Slot, bSuccess ? TEXT("OK") : TEXT("FAILED"), Save->Cells.Num(), Save->EntityLandmarks.Num(),
        Save->InventoryItems.Num(), Save->JournalEntries.Num());
    return bSuccess;
}

bool UHerbalistSaveSubsystem::LoadGame(const FString& SlotName)
{
    const FString Slot = SlotName.IsEmpty() ? DefaultSlotName : SlotName;
    if (!UGameplayStatics::DoesSaveGameExist(Slot, 0))
    {
        UE_LOG(LogHerbalistWorld, Warning, TEXT("LoadGame: slot '%s' does not exist"), *Slot);
        return false;
    }

    UHerbalistSaveGame* Save = Cast<UHerbalistSaveGame>(UGameplayStatics::LoadGameFromSlot(Slot, 0));
    if (!Save)
    {
        UE_LOG(LogHerbalistWorld, Error, TEXT("LoadGame: slot '%s' failed to deserialize"), *Slot);
        return false;
    }

    UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
    AGridWorldManager* WorldManager = FindWorldManager(World);
    if (!WorldManager)
    {
        UE_LOG(LogHerbalistWorld, Warning, TEXT("LoadGame: no AGridWorldManager in world, aborted"));
        return false;
    }

    // Загрузка v1 не путешествует по уровням — восстанавливает состояние прямо
    // в текущей живой сессии (см. чат-сообщение при старте задачи "Сохранения").
    // Поэтому размер сетки должен совпасть: координаты клеток сохранения не
    // означают ничего на сетке другого размера.
    if (WorldManager->GridSizeX != Save->GridSizeX || WorldManager->GridSizeY != Save->GridSizeY)
    {
        UE_LOG(LogHerbalistWorld, Error, TEXT("LoadGame: grid size mismatch (current %dx%d, saved %dx%d), aborted"),
            WorldManager->GridSizeX, WorldManager->GridSizeY, Save->GridSizeX, Save->GridSizeY);
        return false;
    }

    WorldManager->RngBaseSeed = Save->RngBaseSeed;
    WorldManager->SetCurrentTickID(Save->CurrentTickID);
    WorldManager->SetGameClockSeconds(Save->GameClockSeconds);
    WorldManager->SetEntityLandmarks(Save->EntityLandmarks);
    WorldManager->SetShrines(Save->Shrines);
    WorldManager->ApplySaveCells(Save->Cells);

    if (AHerbalistPlayerController* PC = Cast<AHerbalistPlayerController>(World->GetFirstPlayerController()))
    {
        if (PC->InventoryComponent) PC->InventoryComponent->RestoreItems(Save->InventoryItems);
        if (PC->JournalComponent) PC->JournalComponent->RestoreEntries(Save->JournalEntries);
        if (APawn* Pawn = PC->GetPawn())
        {
            Pawn->SetActorLocationAndRotation(Save->PlayerLocation, Save->PlayerRotation);
        }
    }

    UE_LOG(LogHerbalistWorld, Log, TEXT("LoadGame: slot '%s' OK (%d cells, %d landmarks, %d inventory items, %d journal entries)"),
        *Slot, Save->Cells.Num(), Save->EntityLandmarks.Num(), Save->InventoryItems.Num(), Save->JournalEntries.Num());
    return true;
}
