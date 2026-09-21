// Source/ProjectHerbalistTests/Private/Tests/DiegeticStorageTest.cpp
//
// Диегетический интерфейс, этап 3 (DESIGN_Diegetic_Interface.md,
// 2026-09-21): хранилище без окна переноса. Положить -- из руки, по штуке;
// пустой рукой содержимое раскладывается пестерем перед камерой, взять --
// взглядом и взаимодействием: предмет в котомке и в руке (решение
// пользователя «раскладка, как у пестеря»).

#include "Core/World/GridWorldManager.h"
#include "Core/Storage/StorageContainer.h"
#include "Core/Inventory/HerbalistInventoryComponent.h"
#include "Player/HerbalistPlayerController.h"
#include "Player/HeldItemComponent.h"
#include "Player/PesterComponent.h"
#include "Player/PesterItemActor.h"
#include "Misc/AutomationTest.h"
#include "Editor.h"
#include "Engine/World.h"
#include "EngineUtils.h"

#if WITH_AUTOMATION_TESTS && WITH_EDITOR

#include "TestWorldHelpers.h"

namespace
{
    FInventoryItem MakeStorageTestItem(FName ID, float CreationTime)
    {
        FInventoryItem Item;
        Item.IngredientID = ID;
        Item.Count = 1;
        Item.CreationTime = CreationTime;
        Item.State.Magnitude = 0.5f;
        Item.State.Direction.Nature = 1.0f;
        Item.State.Meta.Purity = 0.6f;
        return Item;
    }

    AStorageContainer* SpawnTestStorage(UWorld* World, const FVector& Location)
    {
        AStorageContainer* Storage = World->SpawnActor<AStorageContainer>(AStorageContainer::StaticClass(), Location, FRotator::ZeroRotator);
        if (Storage)
        {
            // Меш и корень хранилищу даёт Blueprint; голому C++-классу корень
            // нужен, чтобы у него было место (дальность раскладки).
            USceneComponent* Root = NewObject<USceneComponent>(Storage, TEXT("TestRoot"));
            Storage->SetRootComponent(Root);
            Root->RegisterComponent();
            Storage->SetActorLocation(Location);
            Storage->DispatchBeginPlay();
        }
        return Storage;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistDiegetic_StorageTakesFromTheHandAndGivesBack,
    "Herbalist.Diegetic.StorageTakesFromTheHandAndGivesBack",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistDiegetic_StorageTakesFromTheHandAndGivesBack::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;
    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("Manager spawned"), Manager)) return false;
    AHerbalistPlayerController* PC = SpawnControllerAndBeginPlay(World, Manager);
    if (!TestNotNull(TEXT("Controller spawned"), PC)) { Manager->Destroy(); return false; }
    AStorageContainer* Storage = SpawnTestStorage(World, FVector(200.0f, 0.0f, 0.0f));
    if (!TestNotNull(TEXT("Хранилище создано"), Storage)) { PC->Destroy(); Manager->Destroy(); return false; }
    Storage->InventoryComponent->Clear();

    // Положить: горсть из двух -- одна штука за жест, горсть в руке остаётся.
    const FInventoryItem Herb = MakeStorageTestItem(FName(TEXT("bol_01")), 501.0f);
    PC->InventoryComponent->AddItem(Herb, 2);
    PC->HeldItemComponent->TakeFromInventory(PC->InventoryComponent->FindItemIndex(Herb));
    TestTrue(TEXT("Хранилище приняло"), Storage->ReceiveHeldItem(PC, PC->HeldItemComponent->ResolveHeldIndex()));
    TestEqual(TEXT("В хранилище одна"), CountItemsWithID(Storage->InventoryComponent, FName(TEXT("bol_01"))), 1);
    TestEqual(TEXT("В котомке одна"), CountItemsWithID(PC->InventoryComponent, FName(TEXT("bol_01"))), 1);
    PC->HeldItemComponent->PutAway();

    // Пустой рукой -- раскладка хранилища.
    Storage->OnInteract_Implementation(PC);
    TestTrue(TEXT("Раскрыто"), PC->PesterComponent->IsOpen());
    TestTrue(TEXT("Раскрыто именно хранилище"), PC->PesterComponent->GetViewedContainer() == Storage);
    TestEqual(TEXT("Разложено содержимое хранилища"), PC->PesterComponent->GetLaidOutItems().Num(), Storage->InventoryComponent->GetNumSlots());

    // Взять -- штука в котомке и в руке, раскладка следует за хранилищем.
    APesterItemActor* Laid = PC->PesterComponent->GetLaidOutItems().Num() > 0 ? PC->PesterComponent->GetLaidOutItems()[0].Get() : nullptr;
    if (TestNotNull(TEXT("Есть что взять"), Laid))
    {
        Laid->OnInteract_Implementation(PC);
        TestEqual(TEXT("Хранилище опустело"), CountItemsWithID(Storage->InventoryComponent, FName(TEXT("bol_01"))), 0);
        TestEqual(TEXT("В котомке снова две"), CountItemsWithID(PC->InventoryComponent, FName(TEXT("bol_01"))), 2);
        TestTrue(TEXT("Взятое в руке"), PC->HeldItemComponent->IsHolding() && PC->HeldItemComponent->GetHeldItem().IngredientID == FName(TEXT("bol_01")));
        TestTrue(TEXT("Раскладка хранилища открыта"), PC->PesterComponent->IsOpen());
        TestEqual(TEXT("Раскладка пуста"), PC->PesterComponent->GetLaidOutItems().Num(), 0);
    }

    // Повторно пустой рукой -- закрыть.
    Storage->OnInteract_Implementation(PC);
    TestFalse(TEXT("Закрыто"), PC->PesterComponent->IsOpen());
    TestTrue(TEXT("Хранилище отпущено"), PC->PesterComponent->GetViewedContainer() == nullptr);

    // Ревью 2026-09-21: пока раскрыто хранилище, котомка раскладку не
    // трогает, а после -- своя раскладка снова следует за котомкой.
    PC->HeldItemComponent->PutAway();
    PC->Inventory();
    const int32 LaidBefore = PC->PesterComponent->GetLaidOutItems().Num();
    PC->InventoryComponent->AddItem(MakeStorageTestItem(FName(TEXT("storage_after_close")), 509.0f), 1);
    TestEqual(TEXT("Своя раскладка следует за котомкой"), PC->PesterComponent->GetLaidOutItems().Num(), LaidBefore + 1);
    PC->PesterComponent->Close();

    PC->HeldItemComponent->PutAway();
    Storage->Destroy();
    PC->Destroy();
    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistDiegetic_WalkingAwayClosesTheStorage,
    "Herbalist.Diegetic.WalkingAwayClosesTheStorage",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistDiegetic_WalkingAwayClosesTheStorage::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;
    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("Manager spawned"), Manager)) return false;
    AHerbalistPlayerController* PC = SpawnControllerAndBeginPlay(World, Manager);
    if (!TestNotNull(TEXT("Controller spawned"), PC)) { Manager->Destroy(); return false; }
    APawn* Pawn = World->SpawnActor<APawn>();
    if (!TestNotNull(TEXT("Pawn spawned"), Pawn)) { PC->Destroy(); Manager->Destroy(); return false; }
    PC->Possess(Pawn);

    const FVector PawnPos = Pawn->GetActorLocation();
    AStorageContainer* Storage = SpawnTestStorage(World, PawnPos + FVector(100.0f, 0.0f, 0.0f));
    if (!TestNotNull(TEXT("Хранилище создано"), Storage)) { Pawn->Destroy(); PC->Destroy(); Manager->Destroy(); return false; }

    Storage->OnInteract_Implementation(PC);
    PC->PesterComponent->TickComponent(0.1f, LEVELTICK_All, nullptr);
    TestTrue(TEXT("Рядом -- раскрыто"), PC->PesterComponent->IsOpen());

    Storage->SetActorLocation(PawnPos + FVector(UPesterComponent::ContainerReachCm + 200.0f, 0.0f, 0.0f));
    PC->PesterComponent->TickComponent(0.1f, LEVELTICK_All, nullptr);
    TestFalse(TEXT("Отошёл -- закрыто"), PC->PesterComponent->IsOpen());

    // Хранилище исчезло, пока раскрыто, -- тоже закрыто, без падения.
    Storage->SetActorLocation(PawnPos + FVector(100.0f, 0.0f, 0.0f));
    Storage->OnInteract_Implementation(PC);
    Storage->Destroy();
    PC->PesterComponent->TickComponent(0.1f, LEVELTICK_All, nullptr);
    TestFalse(TEXT("Хранилища нет -- закрыто"), PC->PesterComponent->IsOpen());

    PC->UnPossess();
    Pawn->Destroy();
    PC->Destroy();
    Manager->Destroy();
    return true;
}

#endif // WITH_AUTOMATION_TESTS && WITH_EDITOR
