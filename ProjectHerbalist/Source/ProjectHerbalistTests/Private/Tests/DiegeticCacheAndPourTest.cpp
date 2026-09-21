// Source/ProjectHerbalistTests/Private/Tests/DiegeticCacheAndPourTest.cpp
//
// Диегетический интерфейс, этап 3 (DESIGN_Diegetic_Interface.md,
// 2026-09-21): зелье из руки -- в тайник и на клетку. В тайнике оно
// исполняет открытый заказ с ближайшим сроком (решение пользователя); на
// земле -- поливает клетку тем же путём, что UsePotion.

#include "Core/World/GridWorldManager.h"
#include "Core/Community/OrderCacheActor.h"
#include "Core/Community/OrderTypes.h"
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
    FInventoryItem MakeHandPotion(float CreationTime)
    {
        FInventoryItem Item;
        Item.IngredientID = FName(TEXT("Potion"));
        Item.Count = 1;
        Item.CreationTime = CreationTime;
        Item.State.Magnitude = 0.5f;
        Item.State.Direction.Body = 0.7f;
        Item.State.Direction.Mind = 0.1f;
        Item.State.Direction.Spirit = 0.1f;
        Item.State.Direction.Nature = 0.1f;
        Item.State.Meta.Purity = 0.9f;
        Item.State.Meta.Corruption = 0.05f;
        return Item;
    }

    FActiveOrder MakeHandOrder(int32 Number, double DeadlineClock)
    {
        FActiveOrder Order;
        Order.Number = Number;
        Order.DefinitionID = FName(TEXT("VIL_HEAL"));
        Order.State = EOrderState::Open;
        Order.bNoteRead = true;
        Order.DeadlineClock = DeadlineClock;
        return Order;
    }

    const FActiveOrder* FindHandOrder(const AGridWorldManager* Manager, int32 Number)
    {
        return Manager->GetActiveOrders().FindByPredicate([Number](const FActiveOrder& Order) { return Order.Number == Number; });
    }

    int32 CountHandPotions(const UHerbalistInventoryComponent* Inventory)
    {
        int32 Count = 0;
        for (const FInventoryItem& Item : Inventory->GetItems())
        {
            Count += HerbalistOrders::IsDeliverable(Item) ? Item.Count : 0;
        }
        return Count;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistDiegetic_PotionInTheCacheFulfilsTheMostUrgentOrder,
    "Herbalist.Diegetic.PotionInTheCacheFulfilsTheMostUrgentOrder",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistDiegetic_PotionInTheCacheFulfilsTheMostUrgentOrder::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;
    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("Manager spawned"), Manager)) return false;
    AHerbalistPlayerController* PC = SpawnControllerAndBeginPlay(World, Manager);
    if (!TestNotNull(TEXT("Controller spawned"), PC)) { Manager->Destroy(); return false; }
    AOrderCacheActor* Cache = World->SpawnActor<AOrderCacheActor>(AOrderCacheActor::StaticClass(), FVector(400.0f, 400.0f, 0.0f), FRotator::ZeroRotator);
    if (!TestNotNull(TEXT("Тайник создан"), Cache)) { PC->Destroy(); Manager->Destroy(); return false; }
    Manager->RegisterOrderCache(Cache);

    // Второй заказ пришёл позже, но срок у него раньше -- он и исполняется.
    Manager->SetOrdersState({ MakeHandOrder(1, 5000.0), MakeHandOrder(2, 3000.0) }, {}, 3, 0);
    TestEqual(TEXT("Ближайший срок -- заказ 2"), Manager->FindMostUrgentOpenOrder(), 2);

    const FInventoryItem Potion = MakeHandPotion(301.0f);
    PC->InventoryComponent->AddItem(Potion, 1);
    const int32 Before = CountHandPotions(PC->InventoryComponent);

    // Не зелье тайнику ни к чему -- взаимодействие идёт дальше.
    FInventoryItem Herb = MakeHandPotion(302.0f);
    Herb.IngredientID = FName(TEXT("bol_01"));
    PC->InventoryComponent->AddItem(Herb, 1);
    TestFalse(TEXT("Трава -- не для тайника"), Cache->ReceiveHeldItem(PC, PC->InventoryComponent->FindItemIndex(Herb)));

    TestTrue(TEXT("Тайник принял зелье"), Cache->ReceiveHeldItem(PC, PC->InventoryComponent->FindItemIndex(Potion)));
    TestEqual(TEXT("Зелье ушло из котомки"), CountHandPotions(PC->InventoryComponent), Before - 1);
    const FActiveOrder* Urgent = FindHandOrder(Manager, 2);
    const FActiveOrder* Later = FindHandOrder(Manager, 1);
    TestTrue(TEXT("Исполнен заказ с ближайшим сроком"), Urgent && Urgent->State == EOrderState::Delivered);
    TestTrue(TEXT("Второй ждёт"), Later && Later->State == EOrderState::Open);

    // Открытых не осталось -- зелье остаётся в котомке.
    Manager->SetOrdersState({}, {}, 3, 0);
    const FInventoryItem Spare = MakeHandPotion(303.0f);
    PC->InventoryComponent->AddItem(Spare, 1);
    const int32 WithSpare = CountHandPotions(PC->InventoryComponent);
    TestTrue(TEXT("Ответ тайника -- отказ"), Cache->ReceiveHeldItem(PC, PC->InventoryComponent->FindItemIndex(Spare)));
    TestEqual(TEXT("Без заказа зелье не уходит"), CountHandPotions(PC->InventoryComponent), WithSpare);

    Manager->UnregisterOrderCache(Cache);
    Cache->Destroy();
    PC->Destroy();
    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistDiegetic_PotionPouredOnTheGroundReachesTheCell,
    "Herbalist.Diegetic.PotionPouredOnTheGroundReachesTheCell",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistDiegetic_PotionPouredOnTheGroundReachesTheCell::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;
    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("Manager spawned"), Manager)) return false;
    AHerbalistPlayerController* PC = SpawnControllerAndBeginPlay(World, Manager);
    if (!TestNotNull(TEXT("Controller spawned"), PC)) { Manager->Destroy(); return false; }

    FHitResult Hit;
    Hit.Location = Manager->GetCellWorldPosition(3, 3);
    Hit.ImpactPoint = Hit.Location;

    // Трава -- не поливают.
    FInventoryItem Herb = MakeHandPotion(311.0f);
    Herb.IngredientID = FName(TEXT("bol_01"));
    PC->InventoryComponent->AddItem(Herb, 1);
    TestFalse(TEXT("Траву не льют"), PC->PourPotionOnCell(PC->InventoryComponent->FindItemIndex(Herb), Hit));

    const FInventoryItem Potion = MakeHandPotion(312.0f);
    PC->InventoryComponent->AddItem(Potion, 1);
    const int32 Before = CountHandPotions(PC->InventoryComponent);
    TestTrue(TEXT("Зелье вылито"), PC->PourPotionOnCell(PC->InventoryComponent->FindItemIndex(Potion), Hit));
    TestEqual(TEXT("Зелье ушло из котомки"), CountHandPotions(PC->InventoryComponent), Before - 1);

    // Мимо сетки -- зелье остаётся.
    const FInventoryItem Second = MakeHandPotion(313.0f);
    PC->InventoryComponent->AddItem(Second, 1);
    const int32 WithSecond = CountHandPotions(PC->InventoryComponent);
    FHitResult Outside;
    Outside.Location = FVector(1.0e7f, 1.0e7f, 0.0f);
    TestFalse(TEXT("Мимо сетки"), PC->PourPotionOnCell(PC->InventoryComponent->FindItemIndex(Second), Outside));
    TestEqual(TEXT("Зелье цело"), CountHandPotions(PC->InventoryComponent), WithSecond);

    PC->Destroy();
    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistDiegetic_CacheOutOfReachKeepsThePotion,
    "Herbalist.Diegetic.CacheOutOfReachKeepsThePotion",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistDiegetic_CacheOutOfReachKeepsThePotion::RunTest(const FString& Parameters)
{
    // Ревью 2026-09-21: без пешки проверка досягаемости не идёт вовсе --
    // здесь пешка есть, и тайник в десяти метрах зелья не берёт.
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;
    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("Manager spawned"), Manager)) return false;
    AHerbalistPlayerController* PC = SpawnControllerAndBeginPlay(World, Manager);
    if (!TestNotNull(TEXT("Controller spawned"), PC)) { Manager->Destroy(); return false; }
    APawn* Pawn = World->SpawnActor<APawn>();
    if (!TestNotNull(TEXT("Pawn spawned"), Pawn)) { PC->Destroy(); Manager->Destroy(); return false; }
    PC->Possess(Pawn);
    if (!TestTrue(TEXT("Пешка у контроллера"), PC->GetPawn() == Pawn)) { Pawn->Destroy(); PC->Destroy(); Manager->Destroy(); return false; }

    const FVector PawnPos = Pawn->GetActorLocation();
    AOrderCacheActor* Cache = World->SpawnActor<AOrderCacheActor>(AOrderCacheActor::StaticClass(), PawnPos + FVector(1000.0f, 0.0f, 0.0f), FRotator::ZeroRotator);
    if (!TestNotNull(TEXT("Тайник создан"), Cache)) { Pawn->Destroy(); PC->Destroy(); Manager->Destroy(); return false; }
    Manager->RegisterOrderCache(Cache);
    Manager->SetOrdersState({ MakeHandOrder(1, 5000.0) }, {}, 2, 0);

    const FInventoryItem Potion = MakeHandPotion(321.0f);
    PC->InventoryComponent->AddItem(Potion, 1);
    const int32 Before = CountHandPotions(PC->InventoryComponent);
    TestTrue(TEXT("Ответ тайника -- отказ"), Cache->ReceiveHeldItem(PC, PC->InventoryComponent->FindItemIndex(Potion)));
    TestEqual(TEXT("Далеко -- зелье в котомке"), CountHandPotions(PC->InventoryComponent), Before);
    const FActiveOrder* Order = FindHandOrder(Manager, 1);
    TestTrue(TEXT("Заказ открыт"), Order && Order->State == EOrderState::Open);

    // Подошёл -- положил.
    Cache->SetActorLocation(PawnPos + FVector(100.0f, 0.0f, 0.0f));
    TestTrue(TEXT("Рядом -- принял"), Cache->ReceiveHeldItem(PC, PC->InventoryComponent->FindItemIndex(Potion)));
    TestEqual(TEXT("Зелье ушло"), CountHandPotions(PC->InventoryComponent), Before - 1);

    Manager->UnregisterOrderCache(Cache);
    Cache->Destroy();
    PC->UnPossess();
    Pawn->Destroy();
    PC->Destroy();
    Manager->Destroy();
    return true;
}

#endif // WITH_AUTOMATION_TESTS && WITH_EDITOR
