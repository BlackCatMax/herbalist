// Source/ProjectHerbalistTests/Private/Tests/DiegeticOfferingTest.cpp
//
// Диегетический интерфейс, этап 3 (DESIGN_Diegetic_Interface.md,
// 2026-09-21): подношения из руки. Общине -- на камень-жертвенник, Легендарной
// -- у её логова, по одному предмету (решения пользователя). Артефакт в дар,
// на камень и в котёл не идёт: предмет ушёл бы, а владение осталось.

#include "Core/World/GridWorldManager.h"
#include "Core/Community/OfferingStoneActor.h"
#include "Core/Storage/AlchemyTableActor.h"
#include "Core/Inventory/HerbalistInventoryComponent.h"
#include "Player/HerbalistPlayerController.h"
#include "Misc/AutomationTest.h"
#include "Editor.h"
#include "Engine/World.h"
#include "EngineUtils.h"

#if WITH_AUTOMATION_TESTS && WITH_EDITOR

#include "TestWorldHelpers.h"

namespace
{
    FInventoryItem MakeOfferingItem(FName ID, float Purity, float CreationTime)
    {
        FInventoryItem Item;
        Item.IngredientID = ID;
        Item.Count = 1;
        Item.CreationTime = CreationTime;
        Item.State.Magnitude = 0.5f;
        Item.State.Direction.Nature = 1.0f;
        Item.State.Meta.Purity = Purity;
        Item.State.Meta.Corruption = 0.0f;
        return Item;
    }

    FInventoryItem MakeArtifactReceipt(FName ArtifactID)
    {
        FInventoryItem Item;
        Item.IngredientID = ArtifactID;
        Item.Count = 1;
        Item.bSubjectToDecay = false;
        return Item;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistDiegetic_OfferingStoneTakesAGiftForTheCommunity,
    "Herbalist.Diegetic.OfferingStoneTakesAGiftForTheCommunity",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistDiegetic_OfferingStoneTakesAGiftForTheCommunity::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;
    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("Manager spawned"), Manager)) return false;
    AHerbalistPlayerController* PC = SpawnControllerAndBeginPlay(World, Manager);
    if (!TestNotNull(TEXT("Controller spawned"), PC)) { Manager->Destroy(); return false; }
    AOfferingStoneActor* Stone = World->SpawnActor<AOfferingStoneActor>(AOfferingStoneActor::StaticClass(), FVector(300.0f, 0.0f, 0.0f), FRotator::ZeroRotator);
    if (!TestNotNull(TEXT("Камень создан"), Stone)) { PC->Destroy(); Manager->Destroy(); return false; }

    Manager->Molva = 0.0f;
    const FInventoryItem Gift = MakeOfferingItem(FName(TEXT("bol_01")), 0.9f, 401.0f);
    PC->InventoryComponent->AddItem(Gift, 1);
    TestTrue(TEXT("Камень принял"), Stone->ReceiveHeldItem(PC, PC->InventoryComponent->FindItemIndex(Gift)));
    TestTrue(TEXT("Чистый дар поднял Молву"), Manager->Molva > 0.0f);
    TestEqual(TEXT("Дар ушёл из котомки"), PC->InventoryComponent->FindItemIndex(Gift), (int32)INDEX_NONE);

    // Артефакт на камень не кладут.
    const FInventoryItem Horn = MakeArtifactReceipt(FName(TEXT("Рог")));
    PC->InventoryComponent->AddItem(Horn, 1);
    const float MolvaBefore = Manager->Molva;
    TestTrue(TEXT("Отказ -- тоже ответ"), Stone->ReceiveHeldItem(PC, PC->InventoryComponent->FindItemIndex(Horn)));
    TestEqual(TEXT("Рог в котомке"), CountItemsWithID(PC->InventoryComponent, FName(TEXT("Рог"))), 1);
    TestEqual(TEXT("Молва не тронута"), Manager->Molva, MolvaBefore);

    Manager->Molva = 0.0f;
    Stone->Destroy();
    PC->Destroy();
    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistDiegetic_GiftAtTheLairEarnsTheArtifact,
    "Herbalist.Diegetic.GiftAtTheLairEarnsTheArtifact",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistDiegetic_GiftAtTheLairEarnsTheArtifact::RunTest(const FString& Parameters)
{
    // Тот же сценарий, что Herbalist.ArtifactInventory.HonestOfferingAddsArtifactToPlayerInventory,
    // но дар -- предметом из руки на клетку логова, без названия артефакта.
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;
    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("Manager spawned"), Manager)) return false;
    AHerbalistPlayerController* PC = SpawnControllerAndBeginPlay(World, Manager);
    if (!TestNotNull(TEXT("Controller spawned"), PC)) { Manager->Destroy(); return false; }

    const FName LegendaryID(TEXT("Индрик-зверь"));
    const FIntPoint* Anchor = Manager->GetLegendaryAnchors().Find(LegendaryID);
    if (!TestNotNull(TEXT("У Индрика есть логово"), Anchor)) { PC->Destroy(); Manager->Destroy(); return false; }
    const FIntPoint Lair = *Anchor;
    TestEqual(TEXT("Логово Индрика -- за Рог"), PC->FindArtifactOfLairAt(Lair), FName(TEXT("Рог")));
    if (FGridCell* Cell = Manager->GetCell(Lair.X, Lair.Y))
    {
        Cell->ManifestedEntityID = LegendaryID;
    }
    FHitResult Hit;
    Hit.Location = Manager->GetCellWorldPosition(Lair.X, Lair.Y);
    Hit.ImpactPoint = Hit.Location;

    // Слабый дар не принят, и зелье у логова не выливается -- это дар, а не полив.
    FInventoryItem WeakPotion = MakeOfferingItem(FName(TEXT("Potion")), 0.1f, 411.0f);
    PC->InventoryComponent->AddItem(WeakPotion, 1);
    TestTrue(TEXT("Логово ответило"), PC->ApplyHeldItemToGround(PC->InventoryComponent->FindItemIndex(WeakPotion), Hit));
    TestTrue(TEXT("Слабое зелье осталось"), PC->InventoryComponent->FindItemIndex(WeakPotion) != INDEX_NONE);
    TestEqual(TEXT("Рога нет"), CountItemsWithID(PC->InventoryComponent, FName(TEXT("Рог"))), 0);

    // Чистый дар -- Рог.
    const FInventoryItem Gift = MakeOfferingItem(FName(TEXT("TestOffering")), 0.9f, 412.0f);
    PC->InventoryComponent->AddItem(Gift, 1);
    TestTrue(TEXT("Дар принят"), PC->ApplyHeldItemToGround(PC->InventoryComponent->FindItemIndex(Gift), Hit));
    TestEqual(TEXT("Дар ушёл"), CountItemsWithID(PC->InventoryComponent, FName(TEXT("TestOffering"))), 0);
    TestEqual(TEXT("Рог в котомке"), CountItemsWithID(PC->InventoryComponent, FName(TEXT("Рог"))), 1);

    // Ревью 2026-09-21: добытый артефакт логово больше не ждёт -- клетка
    // снова обычная земля.
    TestTrue(TEXT("После Рога клетка -- не логово"), PC->FindArtifactOfLairAt(Lair).IsNone());

    PC->Destroy();
    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistDiegetic_ArtifactsDoNotGoIntoTheCauldron,
    "Herbalist.Diegetic.ArtifactsDoNotGoIntoTheCauldron",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistDiegetic_ArtifactsDoNotGoIntoTheCauldron::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;
    for (TActorIterator<AAlchemyTableActor> It(World); It; ++It)
    {
        It->Destroy();
    }
    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("Manager spawned"), Manager)) return false;
    AAlchemyTableActor* Table = World->SpawnActor<AAlchemyTableActor>(AAlchemyTableActor::StaticClass(),
        Manager->GetCellWorldPosition(4, 4), FRotator::ZeroRotator);
    if (!TestNotNull(TEXT("Table spawned"), Table)) { Manager->Destroy(); return false; }
    Table->DispatchBeginPlay();
    AHerbalistPlayerController* PC = SpawnControllerAndBeginPlay(World, Manager);
    if (!TestNotNull(TEXT("Controller spawned"), PC)) { Table->Destroy(); Manager->Destroy(); return false; }

    const FInventoryItem Horn = MakeArtifactReceipt(FName(TEXT("Рог")));
    PC->InventoryComponent->AddItem(Horn, 1);
    TestTrue(TEXT("Отказ котла"), Table->ReceiveHeldItem(PC, PC->InventoryComponent->FindItemIndex(Horn)));
    TestEqual(TEXT("В котле пусто"), Table->GetContents().Num(), 0);
    TestEqual(TEXT("Рог в котомке"), CountItemsWithID(PC->InventoryComponent, FName(TEXT("Рог"))), 1);

    PC->Destroy();
    Table->Destroy();
    Manager->Destroy();
    return true;
}

#endif // WITH_AUTOMATION_TESTS && WITH_EDITOR
