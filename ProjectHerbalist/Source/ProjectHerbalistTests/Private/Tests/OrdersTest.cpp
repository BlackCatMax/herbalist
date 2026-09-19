// Source/ProjectHerbalistTests/Private/Tests/OrdersTest.cpp
//
// Заказы и слава травника (2026-09-19, 02_GDD/24_Orders_And_Repute.md): доли
// кругов гостей по Молве, заказ как область в осях, каталог DT_Orders, исход
// наутро и ΔМолва, последствия тёмных заказов, обман лихого, срок и отказ,
// записки по суткам.

#include "Core/World/GridWorldManager.h"
#include "Player/HerbalistPlayerController.h"
#include "Core/Inventory/HerbalistInventoryComponent.h"
#include "Core/Community/OrderTypes.h"
#include "Core/Community/OrderNoteActor.h"
#include "Core/Data/IngredientTableRow.h"
#include "Commandlets/OrdersCreateCommandlet.h"
#include "Engine/DataTable.h"
#include "EngineUtils.h"
#include "Misc/AutomationTest.h"
#include "Editor.h"
#include "Engine/World.h"

#if WITH_AUTOMATION_TESTS && WITH_EDITOR

#include "TestWorldHelpers.h"

namespace
{
    FRealState MakeOrderTestState(float Body, float Mind, float Spirit, float Nature, float Purity, float Corruption,
        float Potency = 0.5f, float Stability = 0.5f)
    {
        FRealState State;
        State.Magnitude = 0.5f;
        State.Direction.Body = Body;
        State.Direction.Mind = Mind;
        State.Direction.Spirit = Spirit;
        State.Direction.Nature = Nature;
        State.Meta.Purity = Purity;
        State.Meta.Corruption = Corruption;
        State.Meta.Potency = Potency;
        State.Meta.Stability = Stability;
        return State;
    }

    FInventoryItem MakeOrderTestPotion(const FRealState& State)
    {
        FInventoryItem Item;
        Item.IngredientID = FName(TEXT("Potion"));
        Item.State = State;
        return Item;
    }

    // Записки, поставленные у порога, не остаются в редакторском мире.
    void DestroyOrderNotes(UWorld* World)
    {
        for (TActorIterator<AOrderNoteActor> It(World); It; ++It)
        {
            It->Destroy();
        }
    }

    // Часы -- на исход заказа number.
    void JumpToResolve(AGridWorldManager* Manager, int32 Number)
    {
        for (const FActiveOrder& Order : Manager->GetActiveOrders())
        {
            if (Order.Number == Number)
            {
                Manager->SetGameClockSeconds(Order.ResolveClock);
            }
        }
        Manager->ResolveDueOrders();
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistOrders_CircleWeightsFollowMolva,
    "Herbalist.Orders.CircleWeightsFollowMolva",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistOrders_CircleWeightsFollowMolva::RunTest(const FString& Parameters)
{
    float V = 0.0f, W = 0.0f, O = 0.0f;
    HerbalistOrders::ComputeCircleWeights(1.0f, V, W, O);
    TestEqual(TEXT("Добрая слава: селяне 71%"), V, 1.2f / 1.7f, 1e-3f);
    TestEqual(TEXT("Добрая слава: лихих нет"), O, 0.0f);
    HerbalistOrders::ComputeCircleWeights(0.0f, V, W, O);
    TestEqual(TEXT("Ноль: селяне 31%"), V, 0.2f / 0.65f, 1e-3f);
    TestEqual(TEXT("Ноль: ратные 38%"), W, 0.25f / 0.65f, 1e-3f);
    TestEqual(TEXT("Ноль: лихие 31%"), O, 0.2f / 0.65f, 1e-3f);
    HerbalistOrders::ComputeCircleWeights(-0.5f, V, W, O);
    TestEqual(TEXT("Минус половина: только лихие"), O, 1.0f, 1e-4f);
    HerbalistOrders::ComputeCircleWeights(-1.0f, V, W, O);
    TestEqual(TEXT("Дурная слава: только лихие"), O, 1.0f, 1e-4f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistOrders_RegionReadsLeadingAxesAndMeta,
    "Herbalist.Orders.RegionReadsLeadingAxesAndMeta",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistOrders_RegionReadsLeadingAxesAndMeta::RunTest(const FString& Parameters)
{
    // Лечение: Тело преобладает, Чистота >= 0.6, Порча <= 0.2.
    FOrderDefinition Heal;
    Heal.LeadingAxes = { EOrderAxis::Body };
    Heal.MetaMin.Purity = 0.6f;
    Heal.MetaMax.Corruption = 0.2f;

    float Distance = -1.0f;
    TestEqual(TEXT("Тело, чисто, без порчи -- точно"),
        HerbalistOrders::EvaluateOrderMatch(Heal, MakeOrderTestState(0.7f, 0.1f, 0.1f, 0.1f, 0.9f, 0.05f), 0.05f, 0.05f, Distance), EOrderMatch::Exact);
    TestEqual(TEXT("Внутри -- отклонение 0"), Distance, 0.0f);
    TestEqual(TEXT("Чистота у самой границы -- сойдёт"),
        HerbalistOrders::EvaluateOrderMatch(Heal, MakeOrderTestState(0.7f, 0.1f, 0.1f, 0.1f, 0.62f, 0.05f), 0.05f, 0.05f, Distance), EOrderMatch::Edge);
    TestEqual(TEXT("Разум преобладает -- мимо"),
        HerbalistOrders::EvaluateOrderMatch(Heal, MakeOrderTestState(0.1f, 0.7f, 0.1f, 0.1f, 0.9f, 0.05f), 0.05f, 0.05f, Distance), EOrderMatch::Miss);
    TestTrue(TEXT("Мимо -- отклонение больше нуля"), Distance > 0.0f);

    // Две ведущие оси: обе выше любой из остальных.
    FOrderDefinition Hearth;
    Hearth.LeadingAxes = { EOrderAxis::Spirit, EOrderAxis::Mind };
    TestEqual(TEXT("Дух и Разум сверху -- точно"),
        HerbalistOrders::EvaluateOrderMatch(Hearth, MakeOrderTestState(0.1f, 0.4f, 0.4f, 0.1f, 0.5f, 0.0f), 0.05f, 0.05f, Distance), EOrderMatch::Exact);
    TestEqual(TEXT("Тело выше Разума -- мимо"),
        HerbalistOrders::EvaluateOrderMatch(Hearth, MakeOrderTestState(0.35f, 0.2f, 0.4f, 0.05f, 0.5f, 0.0f), 0.05f, 0.05f, Distance), EOrderMatch::Miss);

    // Ровное или пустое направление ни во что не попадает; пустышка -- тоже.
    TestEqual(TEXT("Ровное направление -- мимо"),
        HerbalistOrders::EvaluateOrderMatch(Heal, MakeOrderTestState(0.25f, 0.25f, 0.25f, 0.25f, 0.9f, 0.0f), 0.05f, 0.05f, Distance), EOrderMatch::Miss);
    TestEqual(TEXT("Нулевое направление -- мимо"),
        HerbalistOrders::EvaluateOrderMatch(Heal, MakeOrderTestState(0.0f, 0.0f, 0.0f, 0.0f, 0.9f, 0.0f), 0.05f, 0.05f, Distance), EOrderMatch::Miss);
    FRealState Empty = MakeOrderTestState(0.7f, 0.1f, 0.1f, 0.1f, 0.9f, 0.05f);
    Empty.Magnitude = 0.0f;
    TestEqual(TEXT("Пустышка без силы -- мимо"), HerbalistOrders::EvaluateOrderMatch(Heal, Empty, 0.05f, 0.05f, Distance), EOrderMatch::Miss);

    // По заказу отдают только сваренное.
    FInventoryItem Herb;
    Herb.IngredientID = FName(TEXT("broad_04"));
    TestFalse(TEXT("Трава -- не зелье"), HerbalistOrders::IsDeliverable(Herb));
    TestTrue(TEXT("Зелье -- можно"), HerbalistOrders::IsDeliverable(MakeOrderTestPotion(Empty)));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistOrders_CatalogueMatchesTheChapter,
    "Herbalist.Orders.CatalogueMatchesTheChapter",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistOrders_CatalogueMatchesTheChapter::RunTest(const FString& Parameters)
{
    const TArray<FOrderDefinition>& All = HerbalistOrders::GetAllOrderDefinitions();
    TestEqual(TEXT("DT_Orders -- одиннадцать заказов"), All.Num(), 11);
    TestEqual(TEXT("Ассет совпадает с каталогом коммандлета"), All.Num(), UOrdersCreateCommandlet::BuildOrderRows().Num());

    UDataTable* Items = LoadObject<UDataTable>(nullptr, TEXT("/Game/Herbalist/Data/DT_IngredientClass.DT_IngredientClass"));
    if (!TestNotNull(TEXT("DT_IngredientClass"), Items)) return false;
    auto CheckPayment = [this, Items](const FOrderDefinition& Def, const FOrderPayment& Payment, const TCHAR* What)
    {
        if (!Payment.ItemID.IsNone())
        {
            TestNotNull(*FString::Printf(TEXT("%s: %s %s есть в DT_IngredientClass"), *Def.ID.ToString(), What, *Payment.ItemID.ToString()),
                Items->FindRow<FIngredientTableRow>(Payment.ItemID, TEXT("OrdersTest"), false));
        }
    };

    int32 Counts[3] = { 0, 0, 0 };
    for (const FOrderDefinition& Def : All)
    {
        ++Counts[static_cast<int32>(Def.Circle)];
        TestFalse(*FString::Printf(TEXT("%s: записка не пустая"), *Def.ID.ToString()), Def.NoteText.IsEmpty());
        TestTrue(*FString::Printf(TEXT("%s: есть ведущая ось"), *Def.ID.ToString()), Def.LeadingAxes.Num() > 0);
        CheckPayment(Def, Def.Deposit, TEXT("задаток"));
        CheckPayment(Def, Def.Payment, TEXT("плата"));
        CheckPayment(Def, Def.ExactBonus, TEXT("прибавка"));
        if (Def.Circle == EOrderCircle::Outlaws)
        {
            TestTrue(*FString::Printf(TEXT("%s: тёмный заказ срабатывает в мире"), *Def.ID.ToString()), Def.Consequence != EOrderConsequence::None);
            TestFalse(*FString::Printf(TEXT("%s: о нём ходит слух"), *Def.ID.ToString()), Def.ConsequenceRumor.IsEmpty());
        }
    }
    TestEqual(TEXT("Селяне -- четыре заказа"), Counts[0], 4);
    TestEqual(TEXT("Ратные -- три"), Counts[1], 3);
    TestEqual(TEXT("Лихие -- четыре"), Counts[2], 4);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistOrders_GoodOrderIsJudgedByTheRealPotionNextMorning,
    "Herbalist.Orders.GoodOrderIsJudgedByTheRealPotionNextMorning",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistOrders_GoodOrderIsJudgedByTheRealPotionNextMorning::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;
    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;
    Manager->Molva = 0.0f;

    // Попал: Молва растёт -- но только наутро.
    const int32 Healed = Manager->IssueOrder(FName(TEXT("VIL_HEAL")));
    TestTrue(TEXT("Заказ выдан"), Healed > 0);
    TestTrue(TEXT("Отдано"), Manager->DeliverOrder(Healed, MakeOrderTestPotion(MakeOrderTestState(0.7f, 0.1f, 0.1f, 0.1f, 0.9f, 0.05f))));
    Manager->ResolveDueOrders();
    TestEqual(TEXT("До утра Молва та же"), Manager->Molva, 0.0f);
    JumpToResolve(Manager, Healed);
    TestEqual(TEXT("Наутро -- точно: +0.03"), Manager->Molva, 0.03f, 1e-4f);
    TestEqual(TEXT("Заказ закрыт"), Manager->GetActiveOrders().Num(), 0);

    // Промахнулся (травник мог видеть иначе -- сверка по тому, что отдано).
    const int32 Missed = Manager->IssueOrder(FName(TEXT("VIL_HEAL")));
    Manager->DeliverOrder(Missed, MakeOrderTestPotion(MakeOrderTestState(0.1f, 0.7f, 0.1f, 0.1f, 0.9f, 0.05f)));
    JumpToResolve(Manager, Missed);
    TestEqual(TEXT("Не помогло: -0.02"), Manager->Molva, 0.01f, 1e-4f);

    DestroyOrderNotes(World);
    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistOrders_DarkOrderLowersMolvaAndPoisonsTheVillage,
    "Herbalist.Orders.DarkOrderLowersMolvaAndPoisonsTheVillage",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistOrders_DarkOrderLowersMolvaAndPoisonsTheVillage::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;
    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;
    Manager->Molva = 0.0f;
    const FIntPoint Home(5, 5);
    Manager->SetZaryanaCellIfUnset(Home);
    const FIntPoint Probe(7, 5);   // край радиуса отравы
    const FGridCell* ProbeCell = Manager->GetCellConst(Probe.X, Probe.Y);
    if (!TestNotNull(TEXT("Probe cell"), ProbeCell)) { Manager->Destroy(); return false; }
    const float Before = ProbeCell->State.Meta.Corruption;

    const int32 Number = Manager->IssueOrder(FName(TEXT("OUT_POISON")));
    Manager->DeliverOrder(Number, MakeOrderTestPotion(MakeOrderTestState(0.7f, 0.1f, 0.1f, 0.1f, 0.1f, 0.8f)));
    JumpToResolve(Manager, Number);
    TestEqual(TEXT("Отрава продана: -0.04"), Manager->Molva, -0.04f, 1e-4f);
    if (TestEqual(TEXT("Последствие ждёт своего часа"), Manager->GetPendingOrderConsequences().Num(), 1))
    {
        TestEqual(TEXT("Клетка пока цела"), ProbeCell->State.Meta.Corruption, Before);
        Manager->SetGameClockSeconds(Manager->GetPendingOrderConsequences()[0].FireClock);
        Manager->ResolveDueOrders();
        TestEqual(TEXT("Сработало -- порча у деревни"), ProbeCell->State.Meta.Corruption, FMath::Min(1.0f, Before + 0.3f), 1e-4f);
        TestEqual(TEXT("Последствие снято"), Manager->GetPendingOrderConsequences().Num(), 0);
    }

    DestroyOrderNotes(World);
    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistOrders_DeceivingAnOutlawRisksRevengeNotRepute,
    "Herbalist.Orders.DeceivingAnOutlawRisksRevengeNotRepute",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistOrders_DeceivingAnOutlawRisksRevengeNotRepute::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;
    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;
    Manager->Molva = 0.0f;
    const FIntPoint Home(5, 5);
    Manager->SetZaryanaCellIfUnset(Home);
    const FGridCell* HomeCell = Manager->GetCellConst(Home.X, Home.Y);
    if (!TestNotNull(TEXT("Home cell"), HomeCell)) { Manager->Destroy(); return false; }
    // Порог чистый -- чтобы месть было видно (засеянный круг держит порчу у самого дома).
    Manager->ForEachCell([Home](FGridCell& C)
    {
        if (FMath::Abs(C.X - Home.X) <= 1 && FMath::Abs(C.Y - Home.Y) <= 1)
        {
            C.State.Meta.Corruption = 0.0f;
        }
    });

    // Вместо отравы -- чистый отвар Разума: так далеко от заказа, что вскроется наверняка.
    const int32 Number = Manager->IssueOrder(FName(TEXT("OUT_POISON")));
    Manager->DeliverOrder(Number, MakeOrderTestPotion(MakeOrderTestState(0.1f, 0.7f, 0.1f, 0.1f, 0.9f, 0.0f)));
    JumpToResolve(Manager, Number);
    TestEqual(TEXT("Люди не знают -- Молва та же"), Manager->Molva, 0.0f);
    TestEqual(TEXT("Отравы не было -- последствий нет"), Manager->GetPendingOrderConsequences().Num(), 0);
    TestEqual(TEXT("Вскрылось -- порча на пороге (хранилища нет)"), HomeCell->State.Meta.Corruption, 0.2f, 1e-4f);

    DestroyOrderNotes(World);
    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistOrders_ExpiryAndRefusal,
    "Herbalist.Orders.ExpiryAndRefusal",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistOrders_ExpiryAndRefusal::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;
    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;
    Manager->Molva = 0.0f;

    const int32 Refused = Manager->IssueOrder(FName(TEXT("OUT_SLEEP")));
    TestTrue(TEXT("Отказ принят"), Manager->RefuseOrder(Refused));
    TestFalse(TEXT("Повторный отказ -- заказа уже нет"), Manager->RefuseOrder(Refused));
    TestEqual(TEXT("Отказ ничего не стоит"), Manager->Molva, 0.0f);

    Manager->IssueOrder(FName(TEXT("VIL_CATTLE")));
    Manager->IssueOrder(FName(TEXT("OUT_CURSE")));
    Manager->SetGameClockSeconds(Manager->GetGameClockSeconds() + 10.0 * Manager->GetOrderDayLengthSeconds());
    Manager->ResolveDueOrders();
    TestEqual(TEXT("Оба просрочены"), Manager->GetActiveOrders().Num(), 0);
    TestEqual(TEXT("Не дождался добрый -- -0.02, лихой -- ничего"), Manager->Molva, -0.02f, 1e-4f);

    DestroyOrderNotes(World);
    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistOrders_DailyNotesFollowMolva,
    "Herbalist.Orders.DailyNotesFollowMolva",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistOrders_DailyNotesFollowMolva::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;
    AGridWorldManager* Manager = SpawnAndBeginPlay(World, {}, 1234);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;
    const double Day = Manager->GetOrderDayLengthSeconds();

    auto RunDays = [Manager, Day](float Molva, int32 Days, int32 CircleCounts[3], int32& MaxOpen)
    {
        Manager->Molva = Molva;
        for (int32 D = 0; D < Days; ++D)
        {
            Manager->SetGameClockSeconds(Manager->GetGameClockSeconds() + Day);
            Manager->UpdateOrders();
            MaxOpen = FMath::Max(MaxOpen, Manager->GetActiveOrders().Num());
            TArray<int32> Numbers;
            for (const FActiveOrder& Order : Manager->GetActiveOrders())
            {
                if (const FOrderDefinition* Def = HerbalistOrders::FindOrderDefinition(Order.DefinitionID))
                {
                    ++CircleCounts[static_cast<int32>(Def->Circle)];
                }
                Numbers.Add(Order.Number);
            }
            for (const int32 Number : Numbers)
            {
                Manager->RefuseOrder(Number);   // освободить место на завтра
            }
        }
    };

    int32 Good[3] = { 0, 0, 0 };
    int32 MaxOpen = 0;
    RunDays(1.0f, 30, Good, MaxOpen);
    TestTrue(TEXT("Добрая слава: записки приходят"), Good[0] + Good[1] > 0);
    TestEqual(TEXT("Добрая слава: лихих нет"), Good[2], 0);
    TestTrue(TEXT("Не больше двух записок в сутки"), MaxOpen <= 2);

    int32 Bad[3] = { 0, 0, 0 };
    RunDays(-1.0f, 30, Bad, MaxOpen);
    TestTrue(TEXT("Дурная слава: лихие приходят"), Bad[2] > 0);
    TestEqual(TEXT("Дурная слава: только лихие"), Bad[0] + Bad[1], 0);

    // Без отказов -- не больше трёх открытых разом.
    Manager->Molva = 0.0f;
    for (int32 D = 0; D < 10; ++D)
    {
        Manager->SetGameClockSeconds(Manager->GetGameClockSeconds() + Day);
        Manager->UpdateOrders();
        TestTrue(TEXT("Открытых не больше трёх"), Manager->GetActiveOrders().Num() <= 3);
    }

    // Перемотка назад новых записок не даёт.
    for (const FActiveOrder& Order : TArray<FActiveOrder>(Manager->GetActiveOrders()))
    {
        Manager->RefuseOrder(Order.Number);
    }
    Manager->SetGameClockSeconds(Manager->GetGameClockSeconds() - 3.0 * Day);
    Manager->UpdateOrders();
    TestEqual(TEXT("Назад -- записок нет"), Manager->GetActiveOrders().Num(), 0);

    // Сейв: заказы, счётчики -- как были.
    const int32 Kept = Manager->IssueOrder(FName(TEXT("WAR_HUNT")));
    const TArray<FActiveOrder> Orders = Manager->GetActiveOrders();
    const int32 NextNumber = Manager->GetNextOrderNumber();
    const int32 LastDay = Manager->GetLastOrderDay();
    Manager->SetOrdersState({}, {}, 1, -1);
    TestEqual(TEXT("Сброшено"), Manager->GetActiveOrders().Num(), 0);
    Manager->SetOrdersState(Orders, {}, NextNumber, LastDay);
    TestEqual(TEXT("Заказ восстановлен"), Manager->GetActiveOrders().Num(), 1);
    TestEqual(TEXT("Тот же номер"), Manager->GetActiveOrders()[0].Number, Kept);
    TestEqual(TEXT("Следующий номер -- как был"), Manager->IssueOrder(FName(TEXT("WAR_HUNT"))), NextNumber);

    DestroyOrderNotes(World);
    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistOrders_ControllerDeliversOnlyPotions,
    "Herbalist.Orders.ControllerDeliversOnlyPotions",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistOrders_ControllerDeliversOnlyPotions::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;
    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;
    AHerbalistPlayerController* PC = SpawnControllerAndBeginPlay(World, Manager);
    if (!TestNotNull(TEXT("Controller spawned"), PC) || !TestNotNull(TEXT("Inventory"), PC->InventoryComponent))
    {
        Manager->Destroy();
        return false;
    }

    // Трава и зелье; ячейки ищем по ID -- котомка сама решает порядок.
    FInventoryItem Herb;
    Herb.IngredientID = FName(TEXT("broad_04"));
    PC->InventoryComponent->AddItem(Herb, 1);
    PC->InventoryComponent->AddItem(MakeOrderTestPotion(MakeOrderTestState(0.7f, 0.1f, 0.1f, 0.1f, 0.9f, 0.05f)), 1);
    auto SlotOf = [PC](FName ID)
    {
        return PC->InventoryComponent->GetItems().IndexOfByPredicate([ID](const FInventoryItem& Item) { return Item.IngredientID == ID; });
    };
    const int32 Number = Manager->IssueOrder(FName(TEXT("VIL_HEAL")));

    PC->DeliverOrder(Number, SlotOf(FName(TEXT("broad_04"))));
    TestEqual(TEXT("Траву не отдать -- заказ открыт"), Manager->GetActiveOrders()[0].State, EOrderState::Open);
    TestEqual(TEXT("Трава осталась в котомке"), CountItemsWithID(PC->InventoryComponent, FName(TEXT("broad_04"))), 1);

    if (!TestTrue(TEXT("Зелье в котомке"), SlotOf(FName(TEXT("Potion"))) != INDEX_NONE))
    {
        PC->Destroy();
        Manager->Destroy();
        return false;
    }
    PC->DeliverOrder(Number, SlotOf(FName(TEXT("Potion"))));
    TestEqual(TEXT("Зелье отдано"), Manager->GetActiveOrders()[0].State, EOrderState::Delivered);
    TestEqual(TEXT("Зелья в котомке больше нет"), CountItemsWithID(PC->InventoryComponent, FName(TEXT("Potion"))), 0);

    const int32 Refused = Manager->IssueOrder(FName(TEXT("OUT_SLEEP")));
    PC->RefuseOrder(Refused);
    TestEqual(TEXT("Отказ через команду"), Manager->GetActiveOrders().Num(), 1);

    DestroyOrderNotes(World);
    PC->Destroy();
    Manager->Destroy();
    return true;
}

#endif // WITH_AUTOMATION_TESTS && WITH_EDITOR
