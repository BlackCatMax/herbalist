// Source/ProjectHerbalistTests/Private/Tests/OrderCacheAndWindowTest.cpp
//
// Тайники и окно заказов (2026-09-21). Тайник -- любое место на уровне,
// какое именно -- для заказа неважно (решение пользователя); зелье кладут в
// него, а не отдают у порога. Пока тайников нет вовсе, отдавать можно где
// угодно -- иначе заказы встали бы до расстановки.
//
// Окно заказов заменяет три консольные команды: записки слева, зелья
// справа, «Отдать» и «Отказаться». Проверяется его логика (строки, выбор,
// отдача), а не пиксели -- интерактивный UI headless не гоняется
// (ROADMAP, «Известные пробелы в тестировании»).

#include "Core/World/GridWorldManager.h"
#include "Core/Community/OrderCacheActor.h"
#include "Core/Community/OrderTypes.h"
#include "Core/Inventory/HerbalistInventoryComponent.h"
#include "Player/HerbalistPlayerController.h"
#include "UI/OrdersWindowWidget.h"
#include "Blueprint/UserWidget.h"
#include "Misc/AutomationTest.h"
#include "Editor.h"
#include "Engine/World.h"
#include "EngineUtils.h"

#if WITH_AUTOMATION_TESTS && WITH_EDITOR

#include "TestWorldHelpers.h"

namespace
{
    FInventoryItem MakeCacheTestPotion()
    {
        FInventoryItem Item;
        Item.IngredientID = FName(TEXT("Potion"));
        Item.Count = 1;
        Item.State.Magnitude = 0.5f;
        Item.State.Direction.Body = 0.7f;
        Item.State.Direction.Mind = 0.1f;
        Item.State.Direction.Spirit = 0.1f;
        Item.State.Direction.Nature = 0.1f;
        Item.State.Meta.Purity = 0.9f;
        Item.State.Meta.Corruption = 0.05f;
        return Item;
    }

    // У контроллера в котомке с рождения лежат стартовые вещи -- зелье не
    // обязательно в нулевой ячейке, ищем его явно.
    int32 FindPotionIndex(const UHerbalistInventoryComponent* Inventory)
    {
        return Inventory->GetItems().IndexOfByPredicate([](const FInventoryItem& Item)
        {
            return HerbalistOrders::IsDeliverable(Item);
        });
    }

    int32 CountPotions(const UHerbalistInventoryComponent* Inventory)
    {
        int32 Count = 0;
        for (const FInventoryItem& Item : Inventory->GetItems())
        {
            Count += HerbalistOrders::IsDeliverable(Item) ? Item.Count : 0;
        }
        return Count;
    }

    void DestroyOrderCaches(UWorld* World)
    {
        for (TActorIterator<AOrderCacheActor> It(World); It; ++It)
        {
            It->Destroy();
        }
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistOrderCache_DeliveryOnlyAtACacheOnceCachesExist,
    "Herbalist.Orders.DeliveryOnlyAtACacheOnceCachesExist",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistOrderCache_DeliveryOnlyAtACacheOnceCachesExist::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;
    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("Manager spawned"), Manager)) return false;

    const FVector Far(100000.0f, 100000.0f, 0.0f);
    TestFalse(TEXT("Тайников нет"), Manager->HasAnyOrderCache());
    TestTrue(TEXT("Без тайников отдать можно где угодно"), Manager->IsDeliveryAllowedAt(Far));

    const FVector CachePos(500.0f, 500.0f, 0.0f);
    AOrderCacheActor* Cache = World->SpawnActor<AOrderCacheActor>(AOrderCacheActor::StaticClass(), CachePos, FRotator::ZeroRotator);
    if (!TestNotNull(TEXT("Тайник создан"), Cache)) { Manager->Destroy(); return false; }
    Manager->RegisterOrderCache(Cache);

    TestTrue(TEXT("Тайник зарегистрирован"), Manager->HasAnyOrderCache());
    TestFalse(TEXT("Вдали от тайника отдать нельзя"), Manager->IsDeliveryAllowedAt(Far));
    TestTrue(TEXT("У тайника -- можно"), Manager->IsDeliveryAllowedAt(CachePos + FVector(100.0f, 0.0f, 0.0f)));

    // Какой тайник -- неважно: второй, в другом месте, тоже годится.
    const FVector SecondPos(-3000.0f, 2000.0f, 0.0f);
    AOrderCacheActor* Second = World->SpawnActor<AOrderCacheActor>(AOrderCacheActor::StaticClass(), SecondPos, FRotator::ZeroRotator);
    if (TestNotNull(TEXT("Второй тайник создан"), Second))
    {
        Manager->RegisterOrderCache(Second);
        TestTrue(TEXT("У любого тайника -- можно"), Manager->IsDeliveryAllowedAt(SecondPos));
    }

    // Все тайники убрали -- правило снова не действует.
    Manager->UnregisterOrderCache(Cache);
    Manager->UnregisterOrderCache(Second);
    TestTrue(TEXT("Без тайников -- снова где угодно"), Manager->IsDeliveryAllowedAt(Far));

    DestroyOrderCaches(World);
    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistOrderCache_ControllerRefusesAwayFromCache,
    "Herbalist.Orders.ControllerRefusesAwayFromCache",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistOrderCache_ControllerRefusesAwayFromCache::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;
    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("Manager spawned"), Manager)) return false;
    AHerbalistPlayerController* PC = SpawnControllerAndBeginPlay(World, Manager);
    if (!TestNotNull(TEXT("Controller spawned"), PC)) { Manager->Destroy(); return false; }

    const int32 Number = Manager->IssueOrder(FName(TEXT("VIL_HEAL")));
    if (!TestTrue(TEXT("Заказ выдан"), Number > 0)) { PC->Destroy(); Manager->Destroy(); return false; }
    PC->InventoryComponent->AddItem(MakeCacheTestPotion());

    // Тайник есть, а у контроллера нет пешки -- стоять ему негде, у тайника
    // он быть не может: отдача запрещена, и зелье остаётся в котомке.
    AOrderCacheActor* Cache = World->SpawnActor<AOrderCacheActor>(AOrderCacheActor::StaticClass(), FVector(500.0f, 500.0f, 0.0f), FRotator::ZeroRotator);
    if (TestNotNull(TEXT("Тайник создан"), Cache))
    {
        Manager->RegisterOrderCache(Cache);
    }
    FText Reason;
    const int32 PotionIndex = FindPotionIndex(PC->InventoryComponent);
    TestTrue(TEXT("Зелье в котомке"), PotionIndex != INDEX_NONE);
    TestFalse(TEXT("Не у тайника -- отдать нельзя"), PC->TryDeliverOrder(Number, PotionIndex, Reason));
    TestTrue(FString::Printf(TEXT("Причина про тайник: %s"), *Reason.ToString()), Reason.ToString().Contains(TEXT("тайник")));
    TestEqual(TEXT("Зелье осталось в котомке"), CountPotions(PC->InventoryComponent), 1);

    // Тайники убрали -- отдать можно где угодно, как до них.
    Manager->UnregisterOrderCache(Cache);
    TestTrue(TEXT("Без тайников -- отдано"), PC->TryDeliverOrder(Number, FindPotionIndex(PC->InventoryComponent), Reason));
    TestEqual(TEXT("Зелье ушло"), CountPotions(PC->InventoryComponent), 0);

    DestroyOrderCaches(World);
    PC->Destroy();
    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistOrderCache_WindowListsOrdersAndDeliversSelected,
    "Herbalist.Orders.WindowListsOrdersAndDeliversSelected",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistOrderCache_WindowListsOrdersAndDeliversSelected::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;
    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("Manager spawned"), Manager)) return false;
    AHerbalistPlayerController* PC = SpawnControllerAndBeginPlay(World, Manager);
    if (!TestNotNull(TEXT("Controller spawned"), PC)) { Manager->Destroy(); return false; }

    const int32 Number = Manager->IssueOrder(FName(TEXT("VIL_HEAL")));
    if (!TestTrue(TEXT("Заказ выдан"), Number > 0)) { PC->Destroy(); Manager->Destroy(); return false; }

    // Трава и зелье: в списке зелий -- только зелье, по заказу отдают
    // сваренное (HerbalistOrders::IsDeliverable).
    FInventoryItem Herb;
    Herb.IngredientID = FName(TEXT("bol_01"));
    Herb.Count = 1;
    PC->InventoryComponent->AddItem(Herb);
    PC->InventoryComponent->AddItem(MakeCacheTestPotion());

    UOrdersWindowWidget* Window = CreateWidget<UOrdersWindowWidget>(World, UOrdersWindowWidget::StaticClass());
    if (!TestNotNull(TEXT("Окно создано"), Window)) { PC->Destroy(); Manager->Destroy(); return false; }
    Window->BindController(PC);
    Window->TakeWidget();   // строит дерево и зовёт NativeConstruct

    TestEqual(TEXT("Одна записка в списке"), Window->GetOrderRowCount(), 1);
    TestEqual(TEXT("Одно зелье в списке, трава не показана"), Window->GetPotionRowCount(), 1);

    // Без выбора отдать нельзя -- окно подсказывает.
    Window->DeliverSelected();
    TestTrue(TEXT("Заказ ещё открыт"), Manager->GetActiveOrders()[0].State == EOrderState::Open);

    // Выбрали записку и зелье, отдали.
    Window->SelectOrder(Number);
    Window->SelectPotion(FindPotionIndex(PC->InventoryComponent));
    Window->DeliverSelected();
    TestTrue(TEXT("Заказ отдан"), Manager->GetActiveOrders()[0].State == EOrderState::Delivered);
    TestTrue(FString::Printf(TEXT("Строка состояния говорит об отдаче: %s"), *Window->GetStatusText().ToString()),
        Window->GetStatusText().ToString().Contains(TEXT("оставлено")));
    TestEqual(TEXT("Зелий больше нет в списке"), Window->GetPotionRowCount(), 0);

    Window->RemoveFromParent();
    PC->Destroy();
    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistOrderCache_WindowRefusesSelectedOrder,
    "Herbalist.Orders.WindowRefusesSelectedOrder",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistOrderCache_WindowRefusesSelectedOrder::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;
    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("Manager spawned"), Manager)) return false;
    AHerbalistPlayerController* PC = SpawnControllerAndBeginPlay(World, Manager);
    if (!TestNotNull(TEXT("Controller spawned"), PC)) { Manager->Destroy(); return false; }

    const int32 Number = Manager->IssueOrder(FName(TEXT("VIL_HEAL")));
    if (!TestTrue(TEXT("Заказ выдан"), Number > 0)) { PC->Destroy(); Manager->Destroy(); return false; }

    UOrdersWindowWidget* Window = CreateWidget<UOrdersWindowWidget>(World, UOrdersWindowWidget::StaticClass());
    if (!TestNotNull(TEXT("Окно создано"), Window)) { PC->Destroy(); Manager->Destroy(); return false; }
    Window->BindController(PC);
    Window->TakeWidget();

    Window->SelectOrder(Number);
    Window->RefuseSelected();
    TestEqual(TEXT("Заказ снят"), Manager->GetActiveOrders().Num(), 0);
    TestEqual(TEXT("Список пуст"), Window->GetOrderRowCount(), 0);

    Window->RemoveFromParent();
    PC->Destroy();
    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistOrderCache_WindowDeliversTheChosenPotionEvenIfTheBagShifted,
    "Herbalist.Orders.WindowDeliversTheChosenPotionEvenIfTheBagShifted",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistOrderCache_WindowDeliversTheChosenPotionEvenIfTheBagShifted::RunTest(const FString& Parameters)
{
    // Ревью 2026-09-21: окно хранило выбранное зелье номером ячейки. Если
    // из котомки уходило что-то выше (отдали, потратили), номер сдвигался --
    // и «Отдать» тихо отдавало СОСЕДНЕЕ зелье. Заказ сверяется по настоящему
    // зелью, поэтому это ломало саму механику.
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;
    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("Manager spawned"), Manager)) return false;
    AHerbalistPlayerController* PC = SpawnControllerAndBeginPlay(World, Manager);
    if (!TestNotNull(TEXT("Controller spawned"), PC)) { Manager->Destroy(); return false; }

    const int32 Number = Manager->IssueOrder(FName(TEXT("VIL_HEAL")));
    if (!TestTrue(TEXT("Заказ выдан"), Number > 0)) { PC->Destroy(); Manager->Destroy(); return false; }

    // Два разных зелья: А -- то, что игрок выберет, Б -- соседнее.
    FInventoryItem PotionA = MakeCacheTestPotion();
    PotionA.CreationTime = 10.0f;
    PotionA.State.Meta.Purity = 0.91f;
    FInventoryItem PotionB = MakeCacheTestPotion();
    PotionB.CreationTime = 20.0f;
    PotionB.State.Meta.Purity = 0.2f;
    PC->InventoryComponent->AddItem(PotionA);
    PC->InventoryComponent->AddItem(PotionB);

    UOrdersWindowWidget* Window = CreateWidget<UOrdersWindowWidget>(World, UOrdersWindowWidget::StaticClass());
    if (!TestNotNull(TEXT("Окно создано"), Window)) { PC->Destroy(); Manager->Destroy(); return false; }
    Window->BindController(PC);
    Window->TakeWidget();

    const int32 IndexA = PC->InventoryComponent->GetItems().IndexOfByPredicate([](const FInventoryItem& Item) { return Item.CreationTime == 10.0f; });
    if (!TestTrue(TEXT("Зелье А в котомке"), IndexA > 0)) { Window->RemoveFromParent(); PC->Destroy(); Manager->Destroy(); return false; }
    Window->SelectOrder(Number);
    Window->SelectPotion(IndexA);

    // Пока окно открыто, из котомки уходит предмет ВЫШЕ зелья А -- все
    // номера ниже сдвигаются на один.
    PC->InventoryComponent->RemoveItem(0, PC->InventoryComponent->GetItems()[0].Count);
    TestEqual(TEXT("Окно нашло зелье А на новом месте"), Window->GetSelectedPotion(), IndexA - 1);

    Window->DeliverSelected();
    const FActiveOrder& Order = Manager->GetActiveOrders()[0];
    TestTrue(TEXT("Заказ отдан"), Order.State == EOrderState::Delivered);
    TestEqual(TEXT("Отдано именно зелье А, а не соседнее"), Order.DeliveredState.Meta.Purity, 0.91f);

    Window->RemoveFromParent();
    PC->Destroy();
    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistOrderCache_CacheRegistersItselfOnBeginPlay,
    "Herbalist.Orders.CacheRegistersItselfOnBeginPlay",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistOrderCache_CacheRegistersItselfOnBeginPlay::RunTest(const FString& Parameters)
{
    // Ревью 2026-09-21: остальные тесты регистрируют тайник руками и обходят
    // его собственный BeginPlay/EndPlay -- тот самый путь, которым тайник
    // регистрируется в игре.
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;
    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("Manager spawned"), Manager)) return false;

    AOrderCacheActor* Cache = World->SpawnActor<AOrderCacheActor>(AOrderCacheActor::StaticClass(), FVector(300.0f, 300.0f, 0.0f), FRotator::ZeroRotator);
    if (!TestNotNull(TEXT("Тайник создан"), Cache)) { Manager->Destroy(); return false; }
    TestFalse(TEXT("До BeginPlay тайник не зарегистрирован"), Manager->HasAnyOrderCache());

    Cache->DispatchBeginPlay();
    TestTrue(TEXT("BeginPlay зарегистрировал тайник сам"), Manager->HasAnyOrderCache());

    Cache->Destroy();   // EndPlay -- отписка
    TestFalse(TEXT("EndPlay снял тайник"), Manager->HasAnyOrderCache());

    Manager->Destroy();
    return true;
}

#endif // WITH_AUTOMATION_TESTS && WITH_EDITOR
