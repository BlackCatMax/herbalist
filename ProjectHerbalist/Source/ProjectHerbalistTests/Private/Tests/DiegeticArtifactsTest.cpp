// Source/ProjectHerbalistTests/Private/Tests/DiegeticArtifactsTest.cpp
//
// Диегетический интерфейс, этап 5б (DESIGN_Diegetic_Interface.md,
// 2026-09-21): артефакты из руки. С целью -- на клетку под взглядом; на себя
// -- поднести к лицу и взаимодействие; клубочек спрашивает базу строкой
// выбора; плата Змею после «Сделки» -- артефакт из руки (решения
// пользователя). Пути -- те же, что у команд.

#include "Core/World/GridWorldManager.h"
#include "Core/Entities/LandmarkEntityActor.h"
#include "Core/Inventory/HerbalistInventoryComponent.h"
#include "Player/HerbalistPlayerController.h"
#include "Player/HeldItemComponent.h"
#include "Misc/AutomationTest.h"
#include "Editor.h"
#include "Engine/World.h"

#if WITH_AUTOMATION_TESTS && WITH_EDITOR

#include "TestWorldHelpers.h"

namespace
{
    int32 GiveArtifactReceipt(AGridWorldManager* Manager, AHerbalistPlayerController* PC, FName ID)
    {
        FAcquiredArtifact Owned;
        Owned.ArtifactID = ID;
        TArray<FAcquiredArtifact> All = Manager->GetAcquiredArtifacts();
        All.Add(Owned);
        Manager->SetAcquiredArtifacts(All);

        FInventoryItem Receipt;
        Receipt.IngredientID = ID;
        Receipt.Count = 1;
        Receipt.bSubjectToDecay = false;
        PC->InventoryComponent->AddItem(Receipt, 1);
        return PC->InventoryComponent->GetItems().IndexOfByPredicate([ID](const FInventoryItem& Item) { return Item.IngredientID == ID; });
    }

    bool OwnsArtifact(const AGridWorldManager* Manager, FName ID)
    {
        return Manager->GetAcquiredArtifacts().ContainsByPredicate([ID](const FAcquiredArtifact& A) { return A.ArtifactID == ID; });
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistDiegetic_TargetedArtifactGoesToTheCell,
    "Herbalist.Diegetic.TargetedArtifactGoesToTheCell",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistDiegetic_TargetedArtifactGoesToTheCell::RunTest(const FString& Parameters)
{
    // Гребень -- расходуемый: причесал клетку -- ни владения, ни квитанции
    // (тот же итог, что у команды UseComb, ArtifactInventoryTest.cpp).
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;
    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("Manager spawned"), Manager)) return false;
    AHerbalistPlayerController* PC = SpawnControllerAndBeginPlay(World, Manager);
    if (!TestNotNull(TEXT("Controller spawned"), PC)) { Manager->Destroy(); return false; }

    const FName Comb(TEXT("Гребень"));
    const int32 Index = GiveArtifactReceipt(Manager, PC, Comb);
    FHitResult Hit;
    Hit.Location = Manager->GetCellWorldPosition(2, 2);
    Hit.ImpactPoint = Hit.Location;
    TestTrue(TEXT("Гребень применён к клетке"), PC->ApplyHeldItemToGround(Index, Hit));
    TestFalse(TEXT("Владения нет"), OwnsArtifact(Manager, Comb));
    TestEqual(TEXT("Квитанции нет"), CountItemsWithID(PC->InventoryComponent, Comb), 0);

    PC->Destroy();
    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistDiegetic_ArtifactsUsedOnSelfFromTheFace,
    "Herbalist.Diegetic.ArtifactsUsedOnSelfFromTheFace",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistDiegetic_ArtifactsUsedOnSelfFromTheFace::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;
    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("Manager spawned"), Manager)) return false;
    AHerbalistPlayerController* PC = SpawnControllerAndBeginPlay(World, Manager);
    if (!TestNotNull(TEXT("Controller spawned"), PC)) { Manager->Destroy(); return false; }

    // Молодильное яблоко -- поднести к лицу и съесть.
    const FName Apple(TEXT("Молодильное яблоко"));
    PC->HeldItemComponent->TakeFromInventory(GiveArtifactReceipt(Manager, PC, Apple));
    PC->HeldItemComponent->ToggleInspect();
    TestTrue(TEXT("У лица"), PC->HeldItemComponent->IsInspecting());
    TestTrue(TEXT("Съедено"), PC->UseHeldOnSelf());
    TestFalse(TEXT("Владения нет"), OwnsArtifact(Manager, Apple));
    TestEqual(TEXT("Квитанции нет"), CountItemsWithID(PC->InventoryComponent, Apple), 0);

    // Трава у лица -- не на себя.
    FInventoryItem Herb;
    Herb.IngredientID = FName(TEXT("bol_01"));
    Herb.Count = 1;
    PC->InventoryComponent->AddItem(Herb, 1);
    PC->HeldItemComponent->TakeFromInventory(PC->InventoryComponent->GetItems().IndexOfByPredicate([](const FInventoryItem& Item) { return Item.IngredientID == FName(TEXT("bol_01")); }));
    TestFalse(TEXT("Трава -- не артефакт"), PC->UseHeldOnSelf());

    // Клубочек -- база строкой выбора.
    Manager->RegisterBase(FIntPoint(2, 2));
    Manager->RegisterBase(FIntPoint(8, 8));
    FInventoryItem Yarn;
    Yarn.IngredientID = FName(TEXT("Клубочек"));
    Yarn.Count = 1;
    Yarn.bSubjectToDecay = false;
    PC->InventoryComponent->AddItem(Yarn, 1);
    PC->HeldItemComponent->TakeFromInventory(PC->InventoryComponent->GetItems().IndexOfByPredicate([](const FInventoryItem& Item) { return Item.IngredientID == FName(TEXT("Клубочек")); }));
    TestTrue(TEXT("Клубочек ответил"), PC->UseHeldOnSelf());
    TestTrue(TEXT("Строка выбора баз"), PC->IsChoosing() && PC->GetChoiceOptions().Num() == Manager->GetBases().Num());
    PC->CancelChoice();

    PC->HeldItemComponent->PutAway();
    PC->Destroy();
    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistDiegetic_ArtifactFromTheHandPaysTheSerpent,
    "Herbalist.Diegetic.ArtifactFromTheHandPaysTheSerpent",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistDiegetic_ArtifactFromTheHandPaysTheSerpent::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;
    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("Manager spawned"), Manager)) return false;
    AHerbalistPlayerController* PC = SpawnControllerAndBeginPlay(World, Manager);
    if (!TestNotNull(TEXT("Controller spawned"), PC)) { Manager->Destroy(); return false; }

    ALandmarkEntityActor* Host = World->SpawnActor<ALandmarkEntityActor>(ALandmarkEntityActor::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator);
    if (!TestNotNull(TEXT("Хозяин создан"), Host)) { PC->Destroy(); Manager->Destroy(); return false; }
    Host->Init(FName(TEXT("Змей Горыныч")), FIntPoint(5, 5), Manager);
    Manager->SetKalinovMostSite(FIntPoint(5, 5));
    // Другой хозяин -- не Змей: ему сделка ни к чему (ревью 2026-09-21).
    ALandmarkEntityActor* Other = World->SpawnActor<ALandmarkEntityActor>(ALandmarkEntityActor::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator);
    if (Other) Other->Init(FName(TEXT("Домовой")), FIntPoint(6, 6), Manager);

    const FName Horn(TEXT("Рог"));
    const int32 Index = GiveArtifactReceipt(Manager, PC, Horn);

    // Без сделки артефакт хозяину ни к чему -- взаимодействие как пустой рукой.
    TestFalse(TEXT("Без сделки -- не плата"), Host->ReceiveHeldItem(PC, Index));
    TestTrue(TEXT("Рог при себе"), OwnsArtifact(Manager, Horn));

    Manager->ArmKalinovMostDeal();
    if (Other)
    {
        TestFalse(TEXT("Не Змей -- не плата"), Other->ReceiveHeldItem(PC, Index));
        TestTrue(TEXT("Рог ещё при себе"), OwnsArtifact(Manager, Horn));
        Other->Destroy();
    }
    TestTrue(TEXT("Сделка -- плата"), Host->ReceiveHeldItem(PC, Index));
    TestFalse(TEXT("Рог отдан"), OwnsArtifact(Manager, Horn));
    // Находка этапа: квитанция оставалась в котомке после платы командой.
    TestEqual(TEXT("И квитанции нет"), CountItemsWithID(PC->InventoryComponent, Horn), 0);

    Host->Destroy();
    PC->Destroy();
    Manager->Destroy();
    return true;
}

#endif // WITH_AUTOMATION_TESTS && WITH_EDITOR
