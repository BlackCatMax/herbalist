// Source/ProjectHerbalistTests/Private/Tests/StationProcessStatusTest.cpp
//
// Станции в подсказке (2026-09-14, вопрос пользователя «как произвести другие
// действия: сушка и т.п.»): ход и итог сушки, отстоя и выпаривания игрок
// раньше не видел нигде. Попутно и по ревью:
// - слот любого инвентаря брал искажённый предмет из сумки игрока по номеру
//   строки, а при дубле CreationTime -- чужой слот;
// - готовое зелье (bSubjectToDecay=false) на станциях не обрабатывалось;
// - накопитель обновлений терял остаток интервала;
// - Перегной сохранял таймеры процессов.

#include "Core/Types/HerbalistNameUtils.h"
#include "Core/Config/HerbalistSettings.h"
#include "Core/Inventory/HerbalistInventoryComponent.h"
#include "Core/Simulation/Public/PerceivedTypes.h"
#include "Core/Simulation/Public/SnapshotTypes.h"
#include "PerceptionService.h"
#include "Core/Storage/DryingRackActor.h"
#include "Core/Storage/StorageContainer.h"
#include "UI/InventorySlotWidget.h"
#include "UI/ItemTooltipWidget.h"
#include "Blueprint/UserWidget.h"
#include "Misc/AutomationTest.h"
#include "Editor.h"
#include "Engine/World.h"

#if WITH_AUTOMATION_TESTS && WITH_EDITOR

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistProcessStatus_DescribesRunningFinishedAndPausedProcesses,
    "Herbalist.UI.ProcessStatus.DescribesRunningFinishedAndPausedProcesses",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistProcessStatus_DescribesRunningFinishedAndPausedProcesses::RunTest(const FString& Parameters)
{
    FInventoryItem Fresh;
    Fresh.IngredientID = FName(TEXT("Ромашка"));
    TestEqual(TEXT("Процессов не было -- строки нет"),
        GetItemProcessStatus(Fresh, EProcessingStationType::DryingRack), FString());

    FInventoryItem Drying = Fresh;
    Drying.DryingTimeRemainingSeconds = 1500.0f;
    TestEqual(TEXT("В сушилке -- идёт, остаток в минутах"),
        GetItemProcessStatus(Drying, EProcessingStationType::DryingRack), FString(TEXT("сохнет, осталось 25 мин")));
    TestEqual(TEXT("В сумке -- таймер стоит, куда вернуть"),
        GetItemProcessStatus(Drying, EProcessingStationType::None), FString(TEXT("сушка не закончена, осталось 25 мин — в сушилку")));

    Drying.DryingTimeRemainingSeconds = 60.5f;
    TestEqual(TEXT("Минуты вверх: 61 с -- 2 мин"),
        GetItemProcessStatus(Drying, EProcessingStationType::DryingRack), FString(TEXT("сохнет, осталось 2 мин")));
    Drying.DryingTimeRemainingSeconds = 0.2f;
    TestEqual(TEXT("Последние доли секунды -- не 0"),
        GetItemProcessStatus(Drying, EProcessingStationType::DryingRack), FString(TEXT("сохнет, осталось 1 с")));

    FInventoryItem Dried = Fresh;
    Dried.bIsDried = true;
    Dried.DryingTimeRemainingSeconds = 0.0f;
    TestEqual(TEXT("Итог сушки где угодно"),
        GetItemProcessStatus(Dried, EProcessingStationType::None), FString(TEXT("высушено")));

    FInventoryItem Potion;
    Potion.IngredientID = FName(TEXT("Potion"));
    Potion.bHasSettled = true;
    Potion.SettlingTimeRemainingSeconds = 0.0f;
    Potion.EvaporationTimeRemainingSeconds = 45.0f;
    TestEqual(TEXT("Два процесса на зелье -- оба, в порядке станций"),
        GetItemProcessStatus(Potion, EProcessingStationType::EvaporationStill), FString(TEXT("отстоялось · выпаривается, осталось 45 с")));
    TestEqual(TEXT("Выпаривание, вынутое в отстойник, стоит"),
        GetItemProcessStatus(Potion, EProcessingStationType::SettlingStand),
        FString(TEXT("отстоялось · выпаривание не закончено, осталось 45 с — в выпарной куб")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistInventorySlot_PerceivedItemComesFromItsOwnInventory,
    "Herbalist.UI.InventorySlot.PerceivedItemComesFromItsOwnInventory",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistInventorySlot_PerceivedItemComesFromItsOwnInventory::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    ADryingRackActor* Rack = World->SpawnActor<ADryingRackActor>();
    AStorageContainer* Bag = World->SpawnActor<AStorageContainer>();   // в роли сумки игрока
    if (!TestNotNull(TEXT("Rack spawned"), Rack) || !TestNotNull(TEXT("Bag spawned"), Bag))
    {
        if (Rack) Rack->Destroy();
        if (Bag) Bag->Destroy();
        return false;
    }

    // Distortion > 0 и ясность < 1 -- шум восприятия не нулевой, иначе
    // искажённая копия совпала бы с настоящим предметом и проверять было бы
    // нечего.
    const float Clarity = 0.25f;

    FInventoryItem Drying;
    Drying.IngredientID = FName(TEXT("Ромашка"));
    Drying.Count = 1;
    Drying.CreationTime = 111.0f;
    Drying.State.Magnitude = 0.8f;
    Drying.State.Meta.Distortion = 0.6f;
    Drying.DryingTimeRemainingSeconds = 300.0f;
    Rack->InventoryComponent->AddItem(Drying, 1);

    FInventoryItem BagItem;
    BagItem.IngredientID = FName(TEXT("Зверобой"));
    BagItem.Count = 1;
    BagItem.CreationTime = 222.0f;
    BagItem.State.Magnitude = 0.2f;
    BagItem.State.Meta.Distortion = 0.6f;
    Bag->InventoryComponent->AddItem(BagItem, 1);

    // Кэш восприятия сумки: под номером 0 -- её предмет, с узнаваемой силой.
    FInventoryItem CachedBagItem = BagItem;
    CachedBagItem.State.Magnitude = 0.123f;
    FPerceivedInventory BagPerceived;
    BagPerceived.ContainerContents.Add(0, TArray<FInventoryItem>{ CachedBagItem });

    // То, что дало бы восприятие менеджера, считай оно станцию и сумку.
    const FPerceivedInventory RackFull = Simulation::FPerceptionService::ComputePerceivedInventory(Rack->InventoryComponent->CaptureState(), Clarity);
    const FPerceivedInventory BagFull = Simulation::FPerceptionService::ComputePerceivedInventory(Bag->InventoryComponent->CaptureState(), Clarity);
    const FInventoryItem& RackExpected = RackFull.ContainerContents.FindChecked(0)[0];
    const FInventoryItem& BagExpected = BagFull.ContainerContents.FindChecked(0)[0];

    FInventoryItem Out;
    if (TestTrue(TEXT("Предмет станции искажён"),
        UInventorySlotWidget::ResolvePerceivedItem(Rack->InventoryComponent, 0, Bag->InventoryComponent, &BagPerceived, Clarity, Out)))
    {
        TestEqual(TEXT("Станция -- свой предмет, не предмет сумки под тем же номером"), Out.IngredientID, Drying.IngredientID);
        TestEqual(TEXT("Таймер процесса перенесён в искажённую копию"), Out.DryingTimeRemainingSeconds, 300.0f);
        TestEqual(TEXT("Сила -- как у восприятия всего инвентаря"), Out.State.Magnitude, RackExpected.State.Magnitude);
        TestEqual(TEXT("Искажение -- как у восприятия всего инвентаря"), Out.State.Meta.Distortion, RackExpected.State.Meta.Distortion);
    }

    if (TestTrue(TEXT("Предмет сумки искажён"),
        UInventorySlotWidget::ResolvePerceivedItem(Bag->InventoryComponent, 0, Bag->InventoryComponent, &BagPerceived, Clarity, Out)))
    {
        TestEqual(TEXT("Сумка -- кэш восприятия, тот же предмет"), Out.State.Magnitude, 0.123f);
    }

    FInventoryItem SameIdOtherTime = CachedBagItem;
    SameIdOtherTime.CreationTime = 999.0f;
    FPerceivedInventory StaleSameId;
    StaleSameId.ContainerContents.Add(0, TArray<FInventoryItem>{ SameIdOtherTime });
    if (TestTrue(TEXT("Кэш с тем же ID, но другим предметом не мешает"),
        UInventorySlotWidget::ResolvePerceivedItem(Bag->InventoryComponent, 0, Bag->InventoryComponent, &StaleSameId, Clarity, Out)))
    {
        TestEqual(TEXT("Другой CreationTime -- искажается настоящий предмет"), Out.State.Magnitude, BagExpected.State.Magnitude);
    }

    FPerceivedInventory StaleOtherId;
    StaleOtherId.ContainerContents.Add(0, TArray<FInventoryItem>{ Drying });
    if (TestTrue(TEXT("Кэш с чужим ID не мешает"),
        UInventorySlotWidget::ResolvePerceivedItem(Bag->InventoryComponent, 0, Bag->InventoryComponent, &StaleOtherId, Clarity, Out)))
    {
        TestEqual(TEXT("Под номером в кэше другой предмет -- искажается настоящий"), Out.IngredientID, BagItem.IngredientID);
    }

    TestFalse(TEXT("Номера нет в инвентаре -- false"),
        UInventorySlotWidget::ResolvePerceivedItem(Rack->InventoryComponent, 5, nullptr, nullptr, Clarity, Out));

    // Слот окна станции -- тот же путь через TryGetPerceivedItem (без
    // контроллера ясность 0).
    UInventorySlotWidget* SlotW = CreateWidget<UInventorySlotWidget>(World, UInventorySlotWidget::StaticClass());
    if (TestNotNull(TEXT("Slot widget created"), SlotW))
    {
        SlotW->InitializeSlot(0, Drying, Rack->InventoryComponent);
        if (TestTrue(TEXT("Слот станции находит искажённый предмет"), SlotW->TryGetPerceivedItemForTest(Out)))
        {
            TestEqual(TEXT("Слот станции -- предмет станции"), Out.IngredientID, Drying.IngredientID);
        }
        TestEqual(TEXT("Строка процесса слота -- с предмета станции"),
            SlotW->GetProcessStatusForTest(), FString(TEXT("сохнет, осталось 5 мин")));
    }

    Rack->Destroy();
    Bag->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistInventorySlot_SlotFindsItsOwnRowAmongSameCreationTime,
    "Herbalist.UI.InventorySlot.SlotFindsItsOwnRowAmongSameCreationTime",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistInventorySlot_SlotFindsItsOwnRowAmongSameCreationTime::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AActor* Owner = World->SpawnActor<AActor>();
    UHerbalistInventoryComponent* Bag = NewObject<UHerbalistInventoryComponent>(Owner);
    Bag->RegisterComponent();

    // Одна стопка, разошедшаяся по станциям: одна ромашка вернулась из
    // сушилки недосушенной -- тот же CreationTime, отдельный слот (таймер не
    // даёт сложиться).
    FInventoryItem Stack;
    Stack.IngredientID = FName(TEXT("Ромашка"));
    Stack.Count = 1;
    Stack.CreationTime = 111.0f;
    Bag->AddItem(Stack, 4);

    FInventoryItem Unfinished = Stack;
    Unfinished.DryingTimeRemainingSeconds = 300.0f;
    Bag->AddItem(Unfinished, 1);

    if (!TestEqual(TEXT("Sanity: два слота с одним CreationTime"), Bag->GetNumSlots(), 2)) { Owner->Destroy(); return false; }

    UInventorySlotWidget* SlotW = CreateWidget<UInventorySlotWidget>(World, UInventorySlotWidget::StaticClass());
    if (TestNotNull(TEXT("Slot widget created"), SlotW))
    {
        SlotW->InitializeSlot(1, *Bag->GetSlot(1), Bag);
        TestEqual(TEXT("Слот находит свою строку, не первую с тем же CreationTime"), SlotW->FindRealIndexForTest(), 1);
        TestEqual(TEXT("Строка процесса -- недосушенной ромашки"),
            SlotW->GetProcessStatusForTest(), FString(TEXT("сушка не закончена, осталось 5 мин — в сушилку")));
    }

    Owner->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistStations_BrewedPotionIsProcessedDespiteNoDecay,
    "Herbalist.Stations.BrewedPotionIsProcessedDespiteNoDecay",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistStations_BrewedPotionIsProcessedDespiteNoDecay::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AActor* Owner = World->SpawnActor<AActor>();
    UHerbalistInventoryComponent* Stand = NewObject<UHerbalistInventoryComponent>(Owner);
    Stand->RegisterComponent();
    Stand->StationType = EProcessingStationType::SettlingStand;
    UHerbalistInventoryComponent* Rack = NewObject<UHerbalistInventoryComponent>(Owner);
    Rack->RegisterComponent();
    Rack->StationType = EProcessingStationType::DryingRack;

    // Как из котла: ProcessApplyCommand ставит зелью bSubjectToDecay=false.
    FInventoryItem Potion;
    Potion.IngredientID = FName(TEXT("Potion"));
    Potion.Count = 1;
    Potion.bSubjectToDecay = false;
    Potion.State.Magnitude = 0.8f;
    Potion.State.Direction.Mind = 0.5f;
    Potion.State.Direction.Body = 0.2f;
    Potion.State.Direction.Spirit = 0.2f;
    Potion.State.Direction.Nature = 0.1f;
    Stand->AddItem(Potion, 1);
    Rack->AddItem(Potion, 1);

    FActorComponentTickFunction DummyTick;
    Stand->TickComponent(1.0f, ELevelTick::LEVELTICK_All, &DummyTick);
    Rack->TickComponent(1.0f, ELevelTick::LEVELTICK_All, &DummyTick);

    const FInventoryItem* OnStand = Stand->GetSlot(0);
    const FInventoryItem* OnRack = Rack->GetSlot(0);
    if (!TestNotNull(TEXT("Potion on stand"), OnStand) || !TestNotNull(TEXT("Potion on rack"), OnRack)) { Owner->Destroy(); return false; }
    TestTrue(TEXT("Отстойник взвёл таймер зелья из котла"), OnStand->SettlingTimeRemainingSeconds >= 0.0f);
    TestTrue(TEXT("Сушилка не трогает то, что не портится"), OnRack->DryingTimeRemainingSeconds < 0.0f);

    bool bSettled = false;
    for (int32 i = 0; i < 1000 && !bSettled; ++i)
    {
        Stand->TickComponent(1.0f, ELevelTick::LEVELTICK_All, &DummyTick);
        const FInventoryItem* Slot = Stand->GetSlot(0);
        bSettled = Slot && Slot->bHasSettled;
    }
    TestTrue(TEXT("Зелье из котла отстоялось"), bSettled);

    Owner->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistStations_UpdateAccumulatorKeepsRemainder,
    "Herbalist.Stations.UpdateAccumulatorKeepsRemainder",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistStations_UpdateAccumulatorKeepsRemainder::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AActor* Owner = World->SpawnActor<AActor>();
    UHerbalistInventoryComponent* Rack = NewObject<UHerbalistInventoryComponent>(Owner);
    Rack->RegisterComponent();
    Rack->StationType = EProcessingStationType::DryingRack;

    FInventoryItem Herb;
    Herb.IngredientID = FName(TEXT("TestHerb"));
    Herb.Count = 1;
    Herb.bSubjectToDecay = true;
    Herb.State.Meta.Stability = 1.0f;   // порча не двигает State
    Rack->AddItem(Herb, 1);

    // 0.75 с -- точно в двоичной записи. Четыре тика -- 3 с: обновления на 1.5,
    // 2.25 (остаток 0.5 + 0.75) и 3.0 (0.25 + 0.75). Первое взводит таймер,
    // два следующих вычитают по секунде. С обнулением накопителя обновлений
    // было бы два -- таймер на секунду больше.
    FActorComponentTickFunction DummyTick;
    for (int32 i = 0; i < 4; ++i)
    {
        Rack->TickComponent(0.75f, ELevelTick::LEVELTICK_All, &DummyTick);
    }

    const UHerbalistSettings* Settings = GetDefault<UHerbalistSettings>();
    const float Duration = Settings ? Settings->DryingDurationSeconds : 1920.0f;   // реестра карточек в автотесте нет -- общий срок
    const FInventoryItem* Slot = Rack->GetSlot(0);
    if (TestNotNull(TEXT("Herb present"), Slot))
    {
        TestEqual(TEXT("За 3 с -- взвод и две секунды сушки"), Slot->DryingTimeRemainingSeconds, Duration - 2.0f);
    }

    Owner->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistStations_RottingDropsProcessState,
    "Herbalist.Stations.RottingDropsProcessState",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistStations_RottingDropsProcessState::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AActor* Owner = World->SpawnActor<AActor>();
    UHerbalistInventoryComponent* Rack = NewObject<UHerbalistInventoryComponent>(Owner);
    Rack->RegisterComponent();
    Rack->StationType = EProcessingStationType::DryingRack;

    // Трава сгнила, не досохнув: те же пороги, что FullyRottenItemConvertsOnTick.
    FInventoryItem Herb;
    Herb.IngredientID = FName(TEXT("TestHerb"));
    Herb.Count = 1;
    Herb.bSubjectToDecay = true;
    Herb.State.Meta.Purity = 0.02f;
    Herb.State.Meta.Distortion = 0.98f;
    Herb.State.Meta.Stability = 0.0f;
    Herb.DryingTimeRemainingSeconds = 500.0f;
    Rack->AddItem(Herb, 1);

    FActorComponentTickFunction DummyTick;
    Rack->TickComponent(1.0f, ELevelTick::LEVELTICK_All, &DummyTick);

    const FInventoryItem* Slot = Rack->GetSlot(0);
    if (TestNotNull(TEXT("Slot present"), Slot))
    {
        TestEqual(TEXT("Sanity: стала Перегноем"), Slot->IngredientID, UHerbalistInventoryComponent::PeregnoyIngredientID);
        TestEqual(TEXT("Таймер сушки сброшен"), Slot->DryingTimeRemainingSeconds, -1.0f);
        TestEqual(TEXT("Подсказка не пишет про сушку сгнившего"),
            GetItemProcessStatus(*Slot, EProcessingStationType::DryingRack), FString());
    }

    Owner->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistItemTooltip_AppendsProcessStatusToTypeLine,
    "Herbalist.UI.ItemTooltip.AppendsProcessStatusToTypeLine",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistItemTooltip_AppendsProcessStatusToTypeLine::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    // Настоящий макет: строка процесса идёт в его TypeText.
    UClass* TooltipClass = LoadClass<UItemTooltipWidget>(nullptr, TEXT("/Game/Widgets/UI/WBP_ItemTooltip.WBP_ItemTooltip_C"));
    if (!TestNotNull(TEXT("WBP_ItemTooltip loaded"), TooltipClass)) return false;
    UItemTooltipWidget* Tooltip = CreateWidget<UItemTooltipWidget>(World, TooltipClass);
    if (!TestNotNull(TEXT("Tooltip created"), Tooltip)) return false;

    FInventoryItem Item;
    Item.IngredientID = FName(TEXT("Ромашка"));
    Item.Count = 1;

    Tooltip->SetItem(Item);
    TestEqual(TEXT("Без процесса -- только тип"), Tooltip->GetTypeLineForTest().ToString(), FString(TEXT("Ингредиент")));

    Tooltip->SetItem(Item, TEXT("сохнет, осталось 25 мин"));
    TestEqual(TEXT("С процессом -- через точку"), Tooltip->GetTypeLineForTest().ToString(), FString(TEXT("Ингредиент · сохнет, осталось 25 мин")));
    return true;
}

#endif // WITH_AUTOMATION_TESTS && WITH_EDITOR
