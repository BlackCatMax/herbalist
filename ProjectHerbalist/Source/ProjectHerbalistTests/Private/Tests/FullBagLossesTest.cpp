// Source/ProjectHerbalistTests/Private/Tests/FullBagLossesTest.cpp
//
// Потери при полной сумке (ревью кургана, 2026-09-14): предмет, которому нет
// места, AddItem молча отбрасывает, а источник к тому моменту уже списан --
// досушенная стопка занимала по строке на штуку, остаток сплита пропадал,
// артефакт/перо записывались во владение без предмета в сумке.

#include "Core/World/GridWorldManager.h"
#include "Core/Inventory/HerbalistInventoryComponent.h"
#include "Core/Inventory/InventoryDragDropOperation.h"
#include "UI/InventoryDragDropController.h"
#include "Player/HerbalistPlayerController.h"
#include "Misc/AutomationTest.h"
#include "Editor.h"
#include "Engine/World.h"

#if WITH_AUTOMATION_TESTS && WITH_EDITOR

#include "TestWorldHelpers.h"

namespace
{
    // Заполняет сумку до MaxSlots строками из одной штуки разных видов.
    void FillBagForFullBagLosses(UHerbalistInventoryComponent* Bag)
    {
        for (int32 Filler = 0; Bag->GetNumSlots() < Bag->MaxSlots; ++Filler)
        {
            FInventoryItem Item;
            Item.IngredientID = FName(*FString::Printf(TEXT("FullBagLossesFiller%d"), Filler));
            Item.Count = 1;
            if (!Bag->AddItem(Item, 1)) break;
        }
    }

    // Нормированное направление, как у настоящего предмета: у нулевого
    // смешение стопки (NormalizeSum) меняет направление, и третья такая же
    // штука уже не похожа на стопку.
    FInventoryItem MakeStackTestItemForFullBagLosses(const TCHAR* ID)
    {
        FInventoryItem Item;
        Item.IngredientID = FName(ID);
        Item.Count = 1;
        Item.State.Magnitude = 0.5f;
        Item.State.Direction.Body = 0.25f;
        Item.State.Direction.Mind = 0.25f;
        Item.State.Direction.Spirit = 0.25f;
        Item.State.Direction.Nature = 0.25f;
        return Item;
    }

    bool HoldsArtifactForFullBagLosses(const AGridWorldManager* Manager, FName ArtifactID)
    {
        for (const FAcquiredArtifact& Artifact : Manager->GetAcquiredArtifacts())
        {
            if (Artifact.ArtifactID == ArtifactID) return true;
        }
        return false;
    }
}

// ---------------------------------------------------------------------------
// Завершённый процесс ставит таймер в 0, а не в -1: прежняя проверка ">= 0"
// держала каждую досушенную штуку в отдельной строке. Идущий процесс
// по-прежнему не складывается.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistFullBagLosses_FinishedProcessesStackRunningDoNot,
    "Herbalist.FullBagLosses.FinishedProcessesStackRunningDoNot",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistFullBagLosses_FinishedProcessesStackRunningDoNot::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AActor* Owner = World->SpawnActor<AActor>();
    UHerbalistInventoryComponent* Inventory = NewObject<UHerbalistInventoryComponent>(Owner);
    Inventory->RegisterComponent();

    FInventoryItem Dried = MakeStackTestItemForFullBagLosses(TEXT("FullBagLossesDriedHerb"));
    Dried.bIsDried = true;
    Dried.DryingTimeRemainingSeconds = 0.0f;   // так оставляет TickDryingItem
    Inventory->AddItem(Dried, 1);
    Inventory->AddItem(Dried, 1);
    TestEqual(TEXT("Две досушенные штуки -- одна строка"), Inventory->GetNumSlots(), 1);
    TestEqual(TEXT("Досушенная стопка -- 2 штуки"), Inventory->GetSlot(0) ? Inventory->GetSlot(0)->Count : 0, 2);

    // Досушенный с таймером -1 (так бывает у предмета, сушёного не станцией)
    // складывается с досчитавшим до 0.
    FInventoryItem DriedNoTimer = Dried;
    DriedNoTimer.DryingTimeRemainingSeconds = -1.0f;
    Inventory->AddItem(DriedNoTimer, 1);
    TestEqual(TEXT("Досушенный без таймера -- в ту же стопку"), Inventory->GetSlot(0) ? Inventory->GetSlot(0)->Count : 0, 3);

    // Сохнущий не складывается с досушенным.
    FInventoryItem StillDrying = Dried;
    StillDrying.bIsDried = false;
    StillDrying.DryingTimeRemainingSeconds = 30.0f;
    Inventory->AddItem(StillDrying, 1);
    TestEqual(TEXT("Сохнущий -- своя строка"), Inventory->GetNumSlots(), 2);
    Inventory->RemoveItem(1, 1);

    FInventoryItem Settled = MakeStackTestItemForFullBagLosses(TEXT("FullBagLossesSettledPotion"));
    Settled.bHasSettled = true;
    Settled.SettlingTimeRemainingSeconds = 0.0f;
    Inventory->AddItem(Settled, 1);
    Inventory->AddItem(Settled, 1);
    TestEqual(TEXT("Два отстоявшихся зелья -- одна строка"), Inventory->GetNumSlots(), 2);

    FInventoryItem Evaporated = MakeStackTestItemForFullBagLosses(TEXT("FullBagLossesEvaporatedPotion"));
    Evaporated.bHasEvaporated = true;
    Evaporated.EvaporationTimeRemainingSeconds = 0.0f;
    Inventory->AddItem(Evaporated, 1);
    Inventory->AddItem(Evaporated, 1);
    TestEqual(TEXT("Два выпаренных зелья -- одна строка"), Inventory->GetNumSlots(), 3);

    // Отстой идёт (таймер 5 с) -- ни со свежим (-1), ни с таким же не складывается.
    FInventoryItem Settling = MakeStackTestItemForFullBagLosses(TEXT("FullBagLossesSettlingPotion"));
    Settling.SettlingTimeRemainingSeconds = 5.0f;
    FInventoryItem FreshPotion = Settling;
    FreshPotion.SettlingTimeRemainingSeconds = -1.0f;
    Inventory->AddItem(FreshPotion, 1);
    Inventory->AddItem(Settling, 1);
    Inventory->AddItem(Settling, 1);
    TestEqual(TEXT("Идущий отстой не складывается ни со свежим, ни с таким же"), Inventory->GetNumSlots(), 6);

    Owner->Destroy();
    return true;
}

// ---------------------------------------------------------------------------
// Сплит ложится целиком или никак: AddItem возвращает true и при частичном
// добавлении, после чего bIsSplit гасился и отмена остаток не возвращала.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistFullBagLosses_SplitDropIsWholeOrNothing,
    "Herbalist.FullBagLosses.SplitDropIsWholeOrNothing",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistFullBagLosses_SplitDropIsWholeOrNothing::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AActor* Owner = World->SpawnActor<AActor>();
    UHerbalistInventoryComponent* Target = NewObject<UHerbalistInventoryComponent>(Owner);
    Target->RegisterComponent();
    Target->MaxSlots = 1;

    // Стопка 7 при MAX_STACK_SIZE=9 и одной строке -- места ровно на 2.
    FInventoryItem Stack;
    Stack.IngredientID = FName(TEXT("FullBagLossesSplitHerb"));
    Stack.Count = 7;
    Target->AddItem(Stack, 7);
    if (!TestEqual(TEXT("Sanity: места под этот вид -- 2"), Target->GetAvailableCapacityFor(Stack), 2)) { Owner->Destroy(); return false; }

    UInventoryDragDropOperation* DragOp = NewObject<UInventoryDragDropOperation>();
    DragOp->bIsSplit = true;
    DragOp->SplitItem = Stack;
    DragOp->SplitItem.Count = 4;

    TestFalse(TEXT("Сплит из 4 при месте на 2 -- отказ"),
        UInventoryDragDropController::TryAddSplitItem(DragOp->SplitItem, nullptr, Target, DragOp));
    TestTrue(TEXT("Отказ -- bIsSplit не погашен, сплит остаётся за перетаскиванием"), DragOp->bIsSplit);
    TestEqual(TEXT("Отказ -- в цели по-прежнему 7"), Target->GetSlot(0) ? Target->GetSlot(0)->Count : 0, 7);

    DragOp->SplitItem.Count = 2;
    TestTrue(TEXT("Сплит из 2 -- лёг"),
        UInventoryDragDropController::TryAddSplitItem(DragOp->SplitItem, nullptr, Target, DragOp));
    TestFalse(TEXT("Лёг -- сплит отдан цели"), DragOp->bIsSplit);
    TestEqual(TEXT("Лёг -- в цели 9"), Target->GetSlot(0) ? Target->GetSlot(0)->Count : 0, 9);

    Owner->Destroy();
    return true;
}

// ---------------------------------------------------------------------------
// Отменённый сплит возвращается в свою строку. Стопка в процессе станции не
// складывается ни с чем, даже с собственным сплитом, и прежний возврат через
// AddItem при занятых строках пропадал.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistFullBagLosses_CancelledSplitReturnsToItsRow,
    "Herbalist.FullBagLosses.CancelledSplitReturnsToItsRow",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistFullBagLosses_CancelledSplitReturnsToItsRow::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AActor* Owner = World->SpawnActor<AActor>();
    UHerbalistInventoryComponent* Rack = NewObject<UHerbalistInventoryComponent>(Owner);
    Rack->RegisterComponent();
    Rack->MaxSlots = 1;

    FInventoryItem Drying;
    Drying.IngredientID = FName(TEXT("FullBagLossesRackHerb"));
    Drying.Count = 4;
    Drying.DryingTimeRemainingSeconds = 100.0f;
    Rack->AddItem(Drying, 4);

    FInventoryItem Split;
    if (!TestTrue(TEXT("Sanity: сплит 2 из 4"), Rack->SplitStack(0, 2, Split))) { Owner->Destroy(); return false; }
    TestEqual(TEXT("Sanity: обычным AddItem сплиту некуда"), Rack->GetAvailableCapacityFor(Split), 0);

    TestTrue(TEXT("Возврат в исходную строку"), Rack->ReturnSplitToSlot(0, Split));
    TestEqual(TEXT("В строке снова 4"), Rack->GetSlot(0) ? Rack->GetSlot(0)->Count : 0, 4);
    TestEqual(TEXT("Строка одна"), Rack->GetNumSlots(), 1);

    Owner->Destroy();
    return true;
}

// ---------------------------------------------------------------------------
// OfferForArtifact при полной сумке: подношение из одной штуки освобождает
// свою строку (артефакт кладётся после списания), из стопки -- нет, и тогда
// артефакт не добывается вовсе.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistFullBagLosses_ArtifactNeedsRoomBeforeAcquisition,
    "Herbalist.FullBagLosses.ArtifactNeedsRoomBeforeAcquisition",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistFullBagLosses_ArtifactNeedsRoomBeforeAcquisition::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("Manager spawned"), Manager)) return false;
    AHerbalistPlayerController* PC = SpawnControllerAndBeginPlay(World, Manager);
    if (!TestNotNull(TEXT("Controller spawned"), PC) || !TestNotNull(TEXT("Inventory present"), PC->InventoryComponent))
    {
        Manager->Destroy();
        if (PC) PC->Destroy();
        return false;
    }

    const FName LegendaryID(TEXT("Индрик-зверь"));
    const FIntPoint* Anchor = Manager->GetLegendaryAnchors().Find(LegendaryID);
    if (!TestNotNull(TEXT("Индрик-зверь has a seeded anchor cell"), Anchor)) { Manager->Destroy(); PC->Destroy(); return false; }
    if (FGridCell* Cell = Manager->GetCell(Anchor->X, Anchor->Y))
    {
        Cell->ManifestedEntityID = LegendaryID;
    }

    const FName ArtifactID(TEXT("Рог"));
    const FName OfferingID(TEXT("TestOffering"));
    UHerbalistInventoryComponent* Bag = PC->InventoryComponent;

    FInventoryItem Offering;
    Offering.IngredientID = OfferingID;
    Offering.State.Meta.Purity = 0.9f;   // то же честное подношение, что у HonestOfferingAddsArtifactToPlayerInventory
    Offering.Count = 2;
    Bag->AddItem(Offering, 2);
    FillBagForFullBagLosses(Bag);
    if (!TestEqual(TEXT("Sanity: сумка полна"), Bag->GetNumSlots(), Bag->MaxSlots)) { Manager->Destroy(); PC->Destroy(); return false; }

    TestFalse(TEXT("Полная сумка -- места под артефакт нет"), PC->HasRoomForArtifactItem(ArtifactID));

    // Стопка из 2: списание одной штуки строку не освобождает.
    PC->OfferForArtifact(ArtifactID.ToString(), OfferingID.ToString());
    TestFalse(TEXT("Стопка подношения -- артефакт не добыт"), HoldsArtifactForFullBagLosses(Manager, ArtifactID));
    TestEqual(TEXT("Стопка подношения -- подношение не тронуто"), CountItemsWithID(Bag, OfferingID), 2);
    TestEqual(TEXT("Стопка подношения -- артефакта в сумке нет"), CountItemsWithID(Bag, ArtifactID), 0);

    // Одна штука: её строка освобождается раньше, чем кладётся артефакт.
    // Индекс подношения ищется: у контроллера после BeginPlay в сумке уже
    // могут лежать свои предметы.
    int32 OfferingIndex = INDEX_NONE;
    for (int32 i = 0; i < Bag->GetNumSlots(); ++i)
    {
        if (Bag->GetSlot(i) && Bag->GetSlot(i)->IngredientID == OfferingID) { OfferingIndex = i; break; }
    }
    if (!TestTrue(TEXT("Sanity: подношение в сумке"), OfferingIndex != INDEX_NONE)) { Manager->Destroy(); PC->Destroy(); return false; }
    Bag->RemoveItem(OfferingIndex, 1);
    if (!TestEqual(TEXT("Sanity: сумка всё ещё полна"), Bag->GetNumSlots(), Bag->MaxSlots)) { Manager->Destroy(); PC->Destroy(); return false; }
    PC->OfferForArtifact(ArtifactID.ToString(), OfferingID.ToString());
    TestTrue(TEXT("Подношение из одной штуки -- артефакт добыт"), HoldsArtifactForFullBagLosses(Manager, ArtifactID));
    TestEqual(TEXT("Подношение из одной штуки -- списано"), CountItemsWithID(Bag, OfferingID), 0);
    TestEqual(TEXT("Подношение из одной штуки -- артефакт в сумке"), CountItemsWithID(Bag, ArtifactID), 1);

    Manager->Destroy();
    PC->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistFullBagLosses_FeatherNeedsRoomBeforeAcquisition,
    "Herbalist.FullBagLosses.FeatherNeedsRoomBeforeAcquisition",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistFullBagLosses_FeatherNeedsRoomBeforeAcquisition::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("Manager spawned"), Manager)) return false;
    AHerbalistPlayerController* PC = SpawnControllerAndBeginPlay(World, Manager);
    if (!TestNotNull(TEXT("Controller spawned"), PC) || !TestNotNull(TEXT("Inventory present"), PC->InventoryComponent))
    {
        Manager->Destroy();
        if (PC) PC->Destroy();
        return false;
    }

    const FName EntityID(TEXT("жар-птица"));
    const FIntPoint* Anchor = Manager->GetLegendaryAnchors().Find(EntityID);
    if (!TestNotNull(TEXT("жар-птица has a seeded anchor cell"), Anchor)) { Manager->Destroy(); PC->Destroy(); return false; }
    if (FGridCell* Cell = Manager->GetCell(Anchor->X, Anchor->Y))
    {
        Cell->ManifestedEntityID = EntityID;
    }

    const FName FeatherID(TEXT("Перо Жар-птицы"));
    UHerbalistInventoryComponent* Bag = PC->InventoryComponent;
    FillBagForFullBagLosses(Bag);
    if (!TestEqual(TEXT("Sanity: сумка полна"), Bag->GetNumSlots(), Bag->MaxSlots)) { Manager->Destroy(); PC->Destroy(); return false; }

    PC->AcquireFeather(FeatherID.ToString());
    TestFalse(TEXT("Полная сумка -- перо не добыто"), Manager->GetAcquiredFeathers().Contains(FeatherID));
    TestEqual(TEXT("Полная сумка -- пера в сумке нет"), CountItemsWithID(Bag, FeatherID), 0);

    Bag->RemoveItem(Bag->GetNumSlots() - 1, 1);
    PC->AcquireFeather(FeatherID.ToString());
    TestTrue(TEXT("Место есть -- перо добыто"), Manager->GetAcquiredFeathers().Contains(FeatherID));
    TestEqual(TEXT("Место есть -- перо в сумке"), CountItemsWithID(Bag, FeatherID), 1);

    Manager->Destroy();
    PC->Destroy();
    return true;
}

#endif // WITH_AUTOMATION_TESTS && WITH_EDITOR
