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
        UE_LOG(LogHerbalistSave, Warning, TEXT("SaveGame: no AGridWorldManager in world, aborted"));
        return false;
    }

    UHerbalistSaveGame* Save = Cast<UHerbalistSaveGame>(UGameplayStatics::CreateSaveGameObject(UHerbalistSaveGame::StaticClass()));
    if (!Save) return false;

    // SaveVersion уже 1 по дефолту UPROPERTY (см. HerbalistSaveTypes.h) —
    // выставляем явно, чтобы будущая смена версии формата была одной
    // видимой строкой здесь, а не тихим переносом дефолта.
    Save->SaveVersion = 1;
    Save->RngBaseSeed = WorldManager->RngBaseSeed;
    Save->GridSizeX = WorldManager->GridSizeX;
    Save->GridSizeY = WorldManager->GridSizeY;
    Save->CurrentTickID = WorldManager->GetCurrentTickID();
    Save->GameClockSeconds = WorldManager->GetGameClockSeconds();
    Save->Cells = WorldManager->CaptureSaveCells();
    Save->EntityLandmarks = WorldManager->GetEntityLandmarks();
    Save->Shrines = WorldManager->GetShrines();
    Save->Molva = WorldManager->Molva;
    Save->GardenPlots = WorldManager->GardenPlots;
    Save->Bases = WorldManager->GetBases();
    Save->AcquiredArtifacts = WorldManager->GetAcquiredArtifacts();
    Save->GlobalPerceptionClarity = WorldManager->GetGlobalPerceptionClarity();
    Save->ClarityAnchor = WorldManager->GetClarityAnchor();
    Save->ClarityResponseSmoothed = WorldManager->GetClarityResponseSmoothed();
    Save->AcquiredFeathers = WorldManager->GetAcquiredFeathers();
    Save->bGamayunPropheticGuaranteed = WorldManager->IsGamayunPropheticGuaranteed();
    Save->bRosaFirstFalseSignalShown = WorldManager->IsRosaFirstFalseSignalShown();
    Save->bBuyanReached = WorldManager->IsBuyanReached();
    Save->ChosenBuyanPath = WorldManager->GetChosenBuyanPath();
    Save->CollectedFragmentIDs = WorldManager->GetCollectedFragmentIDs().Array();
    Save->HomeStorages = WorldManager->CaptureHomeStorages();
    Save->TieredWards = WorldManager->CaptureTieredWards();

    if (AHerbalistPlayerController* PC = World ? Cast<AHerbalistPlayerController>(World->GetFirstPlayerController()) : nullptr)
    {
        if (PC->InventoryComponent)
        {
            Save->InventoryItems = PC->InventoryComponent->GetItems();
            Save->PersonalContainerType = PC->InventoryComponent->ContainerType;
        }
        if (PC->JournalComponent) Save->JournalEntries = PC->JournalComponent->GetEntries();
        Save->bHasMirror = PC->bHasMirror;
        Save->bHasYarnBall = PC->bHasYarnBall;
        if (APawn* Pawn = PC->GetPawn())
        {
            Save->PlayerLocation = Pawn->GetActorLocation();
            Save->PlayerRotation = Pawn->GetActorRotation();
        }
    }

    const bool bSuccess = UGameplayStatics::SaveGameToSlot(Save, Slot, 0);
    UE_LOG(LogHerbalistSave, Log, TEXT("SaveGame: slot '%s' %s (%d cells, %d landmarks, %d inventory items, %d journal entries)"),
        *Slot, bSuccess ? TEXT("OK") : TEXT("FAILED"), Save->Cells.Num(), Save->EntityLandmarks.Num(),
        Save->InventoryItems.Num(), Save->JournalEntries.Num());
    return bSuccess;
}

bool UHerbalistSaveSubsystem::LoadGame(const FString& SlotName)
{
    const FString Slot = SlotName.IsEmpty() ? DefaultSlotName : SlotName;
    if (!UGameplayStatics::DoesSaveGameExist(Slot, 0))
    {
        UE_LOG(LogHerbalistSave, Warning, TEXT("LoadGame: slot '%s' does not exist"), *Slot);
        return false;
    }

    UHerbalistSaveGame* Save = Cast<UHerbalistSaveGame>(UGameplayStatics::LoadGameFromSlot(Slot, 0));
    if (!Save)
    {
        UE_LOG(LogHerbalistSave, Error, TEXT("LoadGame: slot '%s' failed to deserialize"), *Slot);
        return false;
    }

    // Версия формата (аудит 2026-09-05, см. подробный довод у
    // UHerbalistSaveGame::SaveVersion) — эта сборка понимает только v1.
    // Файл НОВЕЕ, чем умеет читать текущий код (например, сохранённый более
    // новой версией игры), отклоняется явно, а не десериализуется вслепую с
    // риском тихо потерять/неверно истолковать поля, которых эта версия ещё
    // не знает.
    if (Save->SaveVersion > 1)
    {
        UE_LOG(LogHerbalistSave, Error, TEXT("LoadGame: slot '%s' has SaveVersion %d, newer than this build supports (1), aborted"),
            *Slot, Save->SaveVersion);
        return false;
    }

    UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
    AGridWorldManager* WorldManager = FindWorldManager(World);
    if (!WorldManager)
    {
        UE_LOG(LogHerbalistSave, Warning, TEXT("LoadGame: no AGridWorldManager in world, aborted"));
        return false;
    }

    // Загрузка v1 не путешествует по уровням — восстанавливает состояние прямо
    // в текущей живой сессии (см. чат-сообщение при старте задачи "Сохранения").
    // Поэтому размер сетки должен совпасть: координаты клеток сохранения не
    // означают ничего на сетке другого размера.
    if (WorldManager->GridSizeX != Save->GridSizeX || WorldManager->GridSizeY != Save->GridSizeY)
    {
        UE_LOG(LogHerbalistSave, Error, TEXT("LoadGame: grid size mismatch (current %dx%d, saved %dx%d), aborted"),
            WorldManager->GridSizeX, WorldManager->GridSizeY, Save->GridSizeX, Save->GridSizeY);
        return false;
    }

    WorldManager->RngBaseSeed = Save->RngBaseSeed;
    WorldManager->SetCurrentTickID(Save->CurrentTickID);
    WorldManager->SetGameClockSeconds(Save->GameClockSeconds);

    // Аудит 2026-09-05: таймеры оберегов/артефактных эффектов "короткого
    // окна" (Ward*/InvisibilityCap/YouthApple/Alkonost) осознанно не
    // персистятся — но GameClockSeconds выше ТОЛЬКО ЧТО откатился (вперёд
    // или назад, не важно), а сами таймеры без явного сброса остались бы
    // на прежнем значении. Без этого отступление к более раннему сейву
    // могло прочитать давно истёкший/никогда не активированный в этой
    // временной точке оберег как ещё активный на полный WardDurationSeconds
    // заново — см. подробный довод у ResetSessionOnlyWardTimers.
    WorldManager->ResetSessionOnlyWardTimers();
    WorldManager->SetEntityLandmarks(Save->EntityLandmarks);
    WorldManager->SetShrines(Save->Shrines);
    WorldManager->Molva = Save->Molva;
    WorldManager->GardenPlots = Save->GardenPlots;
    WorldManager->SetBases(Save->Bases);
    WorldManager->SetAcquiredArtifacts(Save->AcquiredArtifacts);
    WorldManager->SetGlobalPerceptionClarity(Save->GlobalPerceptionClarity);
    WorldManager->SetClarityAnchor(Save->ClarityAnchor);
    WorldManager->SetClarityResponseSmoothed(Save->ClarityResponseSmoothed);
    WorldManager->SetAcquiredFeathers(Save->AcquiredFeathers);
    WorldManager->SetGamayunPropheticGuaranteed(Save->bGamayunPropheticGuaranteed);
    WorldManager->SetRosaFirstFalseSignalShown(Save->bRosaFirstFalseSignalShown);
    WorldManager->SetBuyanReached(Save->bBuyanReached);
    WorldManager->SetChosenBuyanPath(Save->ChosenBuyanPath);
    WorldManager->SetCollectedFragmentIDs(TSet<FName>(Save->CollectedFragmentIDs));
    WorldManager->ApplySaveCells(Save->Cells);
    WorldManager->RestoreHomeStorages(Save->HomeStorages);
    WorldManager->RestoreTieredWards(Save->TieredWards);

    if (AHerbalistPlayerController* PC = Cast<AHerbalistPlayerController>(World->GetFirstPlayerController()))
    {
        if (PC->InventoryComponent)
        {
            PC->InventoryComponent->RestoreItems(Save->InventoryItems);
            PC->InventoryComponent->ContainerType = Save->PersonalContainerType;
        }
        if (PC->JournalComponent) PC->JournalComponent->RestoreEntries(Save->JournalEntries);
        PC->bHasMirror = Save->bHasMirror;
        PC->bHasYarnBall = Save->bHasYarnBall;
        if (APawn* Pawn = PC->GetPawn())
        {
            Pawn->SetActorLocationAndRotation(Save->PlayerLocation, Save->PlayerRotation);
        }
    }

    UE_LOG(LogHerbalistSave, Log, TEXT("LoadGame: slot '%s' OK (%d cells, %d landmarks, %d inventory items, %d journal entries)"),
        *Slot, Save->Cells.Num(), Save->EntityLandmarks.Num(), Save->InventoryItems.Num(), Save->JournalEntries.Num());
    return true;
}
