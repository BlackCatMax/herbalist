// Source/ProjectHerbalistTests/Private/Tests/AlchemyUIBugfixesTest.cpp
//
// Аудит проекта (2026-09-05), кластер "UI/инвентарь/котёл" -- первые
// автотесты на виджеты в этом проекте (UAlchemySlotWidget/UMemoryRevealWidget
// не требуют ни одного BindWidget с реальным Blueprint-ассетом, в отличие от
// UAlchemyTransferWidget/UInventoryWidget -- те целиком собраны в .uasset и
// без него падают на первом же обращении к WaterSlot/SlotContainer,
// поэтому не тестируются здесь тем же способом).

#include "UI/AlchemySlotWidget.h"
#include "UI/MemoryRevealWidget.h"
#include "UI/InventorySlotWidget.h"
#include "Core/Types/HerbalistCoreMath.h"
#include "Core/World/GridWorldManager.h"
#include "Player/HerbalistPlayerController.h"
#include "Core/Inventory/HerbalistInventoryComponent.h"
#include "Misc/AutomationTest.h"
#include "Editor.h"
#include "Engine/World.h"

#if WITH_AUTOMATION_TESTS && WITH_EDITOR

#include "TestWorldHelpers.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistCoreMath_BlendRealStatesForStackWeightsByCount,
    "Herbalist.CoreMath.BlendRealStatesForStackWeightsByCount",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistCoreMath_BlendRealStatesForStackWeightsByCount::RunTest(const FString& Parameters)
{
    FRealState Target;
    Target.Magnitude = 0.2f;

    FRealState Source;
    Source.Magnitude = 0.8f;

    // 3 уже лежащих единицы + 1 новая -- вес 3:1 в пользу Target.
    HerbalistCore::Math::BlendRealStatesForStack(Target, Source, 3, 1);

    TestEqual(TEXT("Magnitude -- взвешенное среднее по количеству (0.2*0.75+0.8*0.25=0.35), не Source и не голое 50/50"),
        Target.Magnitude, 0.35f, 0.001f);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistAlchemySlot_OverflowRefusesInsteadOfSilentlyTruncating,
    "Herbalist.UI.AlchemySlot.OverflowRefusesInsteadOfSilentlyTruncating",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistAlchemySlot_OverflowRefusesInsteadOfSilentlyTruncating::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    UAlchemySlotWidget* Slot = CreateWidget<UAlchemySlotWidget>(World, UAlchemySlotWidget::StaticClass());
    if (!TestNotNull(TEXT("Slot widget created"), Slot)) return false;
    Slot->InitializeSlot(EAlchemySlotType::Ingredient, 9);

    FInventoryItem Item;
    Item.IngredientID = FName(TEXT("Ромашка"));
    Item.Count = 1;

    // Аудит 2026-09-05: раньше Amount молча клэмпился до MaxCount, остаток
    // "исчезал", AddItem всё равно возвращал true. Теперь -- честный отказ.
    TestFalse(TEXT("Amount за пределами MaxCount отклоняется целиком, не усекается молча"),
        Slot->AddItem(Item, 15));
    TestNull(TEXT("Слот остался пустым -- ничего не было тихо принято"), Slot->GetItem());

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistAlchemySlot_SecondUnitBlendsStateNotDiscarded,
    "Herbalist.UI.AlchemySlot.SecondUnitBlendsStateNotDiscarded",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistAlchemySlot_SecondUnitBlendsStateNotDiscarded::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    UAlchemySlotWidget* Slot = CreateWidget<UAlchemySlotWidget>(World, UAlchemySlotWidget::StaticClass());
    if (!TestNotNull(TEXT("Slot widget created"), Slot)) return false;
    Slot->InitializeSlot(EAlchemySlotType::Ingredient, 9);

    FInventoryItem ItemA;
    ItemA.IngredientID = FName(TEXT("Ромашка"));
    ItemA.State.Meta.Distortion = 0.2f;
    ItemA.Count = 1;

    FInventoryItem ItemB = ItemA;
    ItemB.State.Meta.Distortion = 0.8f;

    TestTrue(TEXT("Первая единица принята"), Slot->AddItem(ItemA, 1));
    TestTrue(TEXT("Вторая единица (с другим State) принята в тот же слот"), Slot->AddItem(ItemB, 1));

    const FInventoryItem* Stored = Slot->GetItem();
    if (TestNotNull(TEXT("В слоте что-то лежит"), Stored))
    {
        TestEqual(TEXT("Count учитывает обе единицы"), Slot->GetCount(), 2);
        // Аудит 2026-09-05: раньше вторая единица полностью теряла свой
        // State -- Pipeline варил бы Count копий ПЕРВОЙ единицы (0.2), не
        // честную смесь. Теперь Distortion должен сдвинуться к 0.8.
        TestNotEqual(TEXT("Distortion -- это смесь, не повтор первой единицы (0.2)"),
            Stored->State.Meta.Distortion, 0.2f);
    }

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistAlchemySlot_ResultSlotDoubleClickDoesNotDuplicate,
    "Herbalist.UI.AlchemySlot.ResultSlotDoubleClickDoesNotDuplicate",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistAlchemySlot_ResultSlotDoubleClickDoesNotDuplicate::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;
    AHerbalistPlayerController* PC = SpawnControllerAndBeginPlay(World, Manager);
    if (!TestNotNull(TEXT("PlayerController spawned"), PC)) { Manager->Destroy(); return false; }

    // ResultSlot ничего не изымает из инвентаря при пополнении -- Pipeline
    // (CheckForNewPotion в реальном UI) уже положил зелье в НАСТОЯЩИЙ
    // инвентарь напрямую; здесь имитируем ровно это состояние: предмет уже
    // есть в PC->InventoryComponent, ResultSlot лишь зеркалит его на витрину.
    // Считаем именно "Potion"-стопки, не Items.Num() целиком -- BeginPlay
    // контроллера уже кладёт стартовую Корзину (см. AHerbalistPlayerController::
    // BeginPlay), общий счётчик слотов был бы 2 ещё до этого теста.
    auto CountPotionStacks = [PC]() {
        return PC->InventoryComponent->GetItems().FilterByPredicate([](const FInventoryItem& I) {
            return I.IngredientID == FName(TEXT("Potion"));
        }).Num();
    };

    FInventoryItem Potion;
    Potion.IngredientID = FName(TEXT("Potion"));
    Potion.Count = 1;
    PC->InventoryComponent->AddItem(Potion, 1);
    TestEqual(TEXT("Зелье лежит в инвентаре один раз (до двойного клика)"), CountPotionStacks(), 1);

    // CreateWidget(World, ...), не CreateWidget(PC, ...) -- голый
    // World->SpawnActor<AHerbalistPlayerController> в тестовом харнессе не
    // локальный (нет ULocalPlayer), UMG отказывается назначать такой
    // контроллер владельцем виджета. Для проверки самого фикса это не
    // мешает: ветка SlotType==Result вообще не обращается к
    // GetOwningPlayer()/HPC (в этом и была суть бага и его починки).
    UAlchemySlotWidget* ResultSlotW = CreateWidget<UAlchemySlotWidget>(World, UAlchemySlotWidget::StaticClass());
    if (!TestNotNull(TEXT("ResultSlot widget created"), ResultSlotW)) { PC->Destroy(); Manager->Destroy(); return false; }
    ResultSlotW->InitializeSlot(EAlchemySlotType::Result, 1);
    ResultSlotW->AddItem(Potion, 1);   // та самая "витрина" -- не изымает ничего из инвентаря

    ResultSlotW->NativeOnMouseButtonDoubleClick(FGeometry(), FPointerEvent());

    // Аудит 2026-09-05: раньше двойной клик по ResultSlot ЕЩЁ РАЗ звал
    // InventoryComponent->AddItem -- зелье оказывалось в инвентаре дважды.
    TestEqual(TEXT("Зелье в инвентаре ровно одно и после двойного клика -- не задублировалось"),
        CountPotionStacks(), 1);
    TestNull(TEXT("Витрина очистилась после клика"), ResultSlotW->GetItem());

    PC->Destroy();
    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistMemoryRevealWidget_ShowBuildsLayoutEvenWithoutViewport,
    "Herbalist.UI.MemoryRevealWidget.ShowBuildsLayoutEvenWithoutViewport",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistMemoryRevealWidget_ShowBuildsLayoutEvenWithoutViewport::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    // Аудит 2026-09-05: раньше Show() проверял BodyText ДО AddToViewport() и
    // тихо выходил на первом же вызове (BodyText ещё nullptr, дерево строит
    // только NativeConstruct, который запускает именно AddToViewport). В
    // редакторском мире (не игровом, IsGameWorld()==false) AddToViewport и
    // тут не сработает -- ровно та обстановка, где старый код молчал
    // навсегда. Show() теперь строит дерево явно и не зависит от того,
    // получится ли реально попасть на экран.
    UMemoryRevealWidget* Widget = CreateWidget<UMemoryRevealWidget>(World, UMemoryRevealWidget::StaticClass());
    if (!TestNotNull(TEXT("Widget created"), Widget)) return false;

    TestTrue(TEXT("BodyText ещё пуст до первого Show()"), Widget->GetBodyTextForTest().IsEmpty());

    Widget->Show(FText::FromString(TEXT("Испытание памяти")), 1.0f);

    TestEqual(TEXT("Текст действительно установлен -- дерево виджета построилось на первом же Show()"),
        Widget->GetBodyTextForTest().ToString(), FString(TEXT("Испытание памяти")));

    // Повторный вызов не должен ломаться от идемпотентной защиты BuildLayout.
    Widget->Show(FText::FromString(TEXT("Второе испытание")), 1.0f);
    TestEqual(TEXT("Повторный Show() тоже обновляет текст"),
        Widget->GetBodyTextForTest().ToString(), FString(TEXT("Второе испытание")));

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistInventorySlot_FindRealIndexSurvivesStateDrift,
    "Herbalist.UI.InventorySlot.FindRealIndexSurvivesStateDrift",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistInventorySlot_FindRealIndexSurvivesStateDrift::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;
    AHerbalistPlayerController* PC = SpawnControllerAndBeginPlay(World, Manager);
    if (!TestNotNull(TEXT("PlayerController spawned"), PC)) { Manager->Destroy(); return false; }

    FInventoryItem ItemA;
    ItemA.IngredientID = FName(TEXT("Ромашка"));
    ItemA.CreationTime = 111.0f;
    ItemA.State.Meta.Distortion = 0.1f;
    ItemA.Count = 1;
    PC->InventoryComponent->AddItem(ItemA, 1);

    FInventoryItem ItemB;
    ItemB.IngredientID = FName(TEXT("Ромашка"));
    ItemB.CreationTime = 222.0f;
    ItemB.State.Meta.Distortion = 0.9f;
    ItemB.Count = 1;
    PC->InventoryComponent->AddItem(ItemB, 1);   // State слишком разный для AreStatesSimilar -- отдельная стопка

    const TArray<FInventoryItem>& Items = PC->InventoryComponent->GetItems();
    // Считаем именно стопки "Ромашка" -- BeginPlay контроллера уже кладёт
    // стартовую Корзину (см. AHerbalistPlayerController::BeginPlay), общий
    // Items.Num() был бы на единицу больше независимо от этого теста.
    const int32 ChamomileStackCount = Items.FilterByPredicate([](const FInventoryItem& I) {
        return I.IngredientID == FName(TEXT("Ромашка"));
    }).Num();
    if (!TestEqual(TEXT("Две отдельные стопки Ромашки (State слишком разный, чтобы слиться)"), ChamomileStackCount, 2))
    {
        PC->Destroy(); Manager->Destroy(); return false;
    }

    const int32 RealIndexB = Items.IndexOfByPredicate([](const FInventoryItem& I) { return I.CreationTime > 200.0f; });
    if (!TestTrue(TEXT("ItemB найден в инвентаре"), RealIndexB != INDEX_NONE))
    {
        PC->Destroy(); Manager->Destroy(); return false;
    }

    UInventorySlotWidget* SlotW = CreateWidget<UInventorySlotWidget>(World, UInventorySlotWidget::StaticClass());
    if (!TestNotNull(TEXT("Slot widget created"), SlotW)) { PC->Destroy(); Manager->Destroy(); return false; }
    SlotW->InitializeSlot(RealIndexB, ItemB, PC->InventoryComponent);   // кэш = снимок ItemB на момент инициализации

    // Порча непрерывно двигает настоящий State (HerbalistInventoryComponent::
    // TickComponent), никогда не оповещая UI (аудит 2026-09-05) -- имитируем
    // это напрямую, не тикая весь компонент: реальный Distortion уходит
    // далеко от закэшированных 0.9, CreationTime порча не трогает.
    FInventoryItem& RealItemB = const_cast<FInventoryItem&>(Items[RealIndexB]);
    RealItemB.State.Meta.Distortion = 0.05f;

    TestEqual(TEXT("FindRealIndex всё ещё находит ItemB по CreationTime, несмотря на разъехавшийся State"),
        SlotW->FindRealIndexForTest(), RealIndexB);

    PC->Destroy();
    Manager->Destroy();
    return true;
}

#endif // WITH_AUTOMATION_TESTS && WITH_EDITOR
