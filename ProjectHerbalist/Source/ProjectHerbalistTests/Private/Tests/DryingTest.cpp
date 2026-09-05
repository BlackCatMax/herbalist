// Source/ProjectHerbalistTests/Private/Tests/DryingTest.cpp
//
// Сушка (DESIGN_Community_And_Homestead.md §2.2, "Хранилища" пункт 3,
// 2026-09-04, прямой запрос пользователя: "сушка — не контейнер, а
// ПРОЦЕСС... хочу честно с изменением свойств"). Три уровня проверки, тем
// же принципом границы, что уже PeregnoyTest.cpp:
//
// 1. TickDryingItem/ApplyDriedStateDelta -- чистые функции состояния, БЕЗ
//    обращения к реестрам, тестируются напрямую (тот же приём, что
//    ShouldConvertToPeregnoy).
// 2. Полный путь через TickComponent (StationType==DryingRack) -- таймер
//    взводится/считает/завершается, decay резко падает после bIsDried.
//    DriedStateDelta через реестр здесь НЕ проверяется (IngredientRegistrySubsystem
//    недоступен в Editor-мире автотестов, тот же класс пробела, что уже у
//    TradeWithCommunity/PlantSeed/BuildHomeStorage) -- см. пункт 1 вместо
//    этого для честных ботанических дельт конкретных карточек.
// 3. AreItemsStackable -- сушёный и свежий предмет одного вида, а также два
//    предмета в процессе сушки, не стекуются молча.
// 4. ResolveDryingDurationSeconds (2026-09-05, "процесс сушки у разных
//    растений разный (длительность)") -- чистая функция резолва приоритета
//    карточка-vs-глобальный-фолбэк, тем же приёмом, что и пункт 1: рукописная
//    FIngredientTableRow вместо реестра.

#include "Core/Inventory/HerbalistInventoryComponent.h"
#include "Core/Types/HerbalistCoreTypes.h"
#include "Core/Data/IngredientTableRow.h"
#include "Misc/AutomationTest.h"
#include "Editor.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

#if WITH_AUTOMATION_TESTS && WITH_EDITOR

// ---------------------------------------------------------------------------
// TickDryingItem -- чистая функция таймера, без реестра/GameInstance.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistDrying_TickDryingItemStartsCountsDownCompletes,
    "Herbalist.Drying.TickDryingItemStartsCountsDownCompletes",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistDrying_TickDryingItemStartsCountsDownCompletes::RunTest(const FString& Parameters)
{
    FInventoryItem Item;
    Item.IngredientID = FName(TEXT("TestHerb"));
    Item.Count = 1;
    TestEqual(TEXT("Fresh item starts with the -1 sentinel (never dried, never started)"), Item.DryingTimeRemainingSeconds, -1.0f);
    TestFalse(TEXT("Fresh item is not marked dried"), Item.bIsDried);

    // Первый вызов -- только взводит таймер, ничего не завершает этим же тиком.
    const bool bCompletedOnStart = UHerbalistInventoryComponent::TickDryingItem(Item, 1.0f, 10.0f);
    TestFalse(TEXT("First call only arms the timer, does not complete"), bCompletedOnStart);
    TestEqual(TEXT("Timer armed to the full duration"), Item.DryingTimeRemainingSeconds, 10.0f);
    TestFalse(TEXT("Still not dried right after arming"), Item.bIsDried);

    // Считаем вниз, не долетая до нуля.
    const bool bCompletedMidway = UHerbalistInventoryComponent::TickDryingItem(Item, 4.0f, 10.0f);
    TestFalse(TEXT("Midway through, still not complete"), bCompletedMidway);
    TestEqual(TEXT("Remaining time decremented"), Item.DryingTimeRemainingSeconds, 6.0f);
    TestFalse(TEXT("Still not dried midway"), Item.bIsDried);

    // Досчитываем ровно до конца (и немного за него -- проверка клампа на 0).
    const bool bCompletedAtEnd = UHerbalistInventoryComponent::TickDryingItem(Item, 7.0f, 10.0f);
    TestTrue(TEXT("Crossing zero reports completion"), bCompletedAtEnd);
    TestEqual(TEXT("Remaining time clamps at exactly 0, does not go negative"), Item.DryingTimeRemainingSeconds, 0.0f);
    TestTrue(TEXT("Item is now marked dried"), Item.bIsDried);

    // Дальнейшие вызовы на уже высохшем предмете -- не откатываются назад
    // (вызывающая сторона в TickComponent и так не зовёт эту функцию
    // повторно на bIsDried==true, но сама функция не должна ломаться, если
    // всё же вызвана: 0 - DeltaTime уходит в минус, клампится обратно в 0).
    const bool bCompletedAgain = UHerbalistInventoryComponent::TickDryingItem(Item, 1.0f, 10.0f);
    TestTrue(TEXT("Calling again on an already-finished item still reports completion (already at/below zero)"), bCompletedAgain);
    TestEqual(TEXT("Remaining time stays clamped at 0"), Item.DryingTimeRemainingSeconds, 0.0f);

    return true;
}

// ---------------------------------------------------------------------------
// ApplyDriedStateDelta -- чистая функция клампа, без реестра/GameInstance.
// Используем реальные числа двух ботанических карточек компендиума (Мухомор
// -- рост Potency, Смородина чёрная -- падение Potency) как честные образцы,
// не выдуманные для симметрии теста числа -- ровно то, что записано в
// ingredient_drying_state_patch.json.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistDrying_ApplyDriedStateDeltaAddsAndClamps,
    "Herbalist.Drying.ApplyDriedStateDeltaAddsAndClamps",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistDrying_ApplyDriedStateDeltaAddsAndClamps::RunTest(const FString& Parameters)
{
    // Мухомор (mix_09): Distortion +0.05, Potency +0.10, Corruption -0.15 --
    // реальный пример роста Potency при сушке (частичная декарбоксилизация
    // иботеновой кислоты в мусцимол, см. компендиумную карточку).
    {
        FMeta Meta;
        Meta.Distortion = 0.85f;
        Meta.Potency = 0.75f;
        Meta.Corruption = 0.80f;
        Meta.Purity = 0.30f;

        FMeta MukhomorDelta;
        MukhomorDelta.Distortion = 0.05f;
        MukhomorDelta.Potency = 0.10f;
        MukhomorDelta.Corruption = -0.15f;

        UHerbalistInventoryComponent::ApplyDriedStateDelta(Meta, MukhomorDelta);

        TestEqual(TEXT("Mukhomor: Potency rises on drying (partial ibotenic acid -> muscimol conversion)"), Meta.Potency, 0.85f);
        TestEqual(TEXT("Mukhomor: Distortion rises slightly (character shifts toward the vision, not the sickness)"), Meta.Distortion, 0.90f);
        TestEqual(TEXT("Mukhomor: Corruption falls (less raw toxic nausea)"), Meta.Corruption, 0.65f);
        TestEqual(TEXT("Untouched axis (Purity) stays exactly as it was"), Meta.Purity, 0.30f);
    }

    // Смородина чёрная (riv_09): Potency -0.12 -- реальный пример падения
    // Potency при сушке (витамин C деградирует от тепла/окисления).
    {
        FMeta Meta;
        Meta.Potency = 0.55f;

        FMeta BlackcurrantDelta;
        BlackcurrantDelta.Potency = -0.12f;

        UHerbalistInventoryComponent::ApplyDriedStateDelta(Meta, BlackcurrantDelta);

        TestEqual(TEXT("Blackcurrant: Potency falls on drying (vitamin C degrades)"), Meta.Potency, 0.43f);
    }

    // Клампы на обеих границах -- дельта не должна утащить ось за [0,1].
    {
        FMeta AtCeiling;
        AtCeiling.Potency = 0.95f;
        FMeta BigPositive;
        BigPositive.Potency = 0.5f;
        UHerbalistInventoryComponent::ApplyDriedStateDelta(AtCeiling, BigPositive);
        TestEqual(TEXT("Delta clamps at the 1.0 ceiling, does not overflow"), AtCeiling.Potency, 1.0f);

        FMeta AtFloor;
        AtFloor.Corruption = 0.05f;
        FMeta BigNegative;
        BigNegative.Corruption = -0.5f;
        UHerbalistInventoryComponent::ApplyDriedStateDelta(AtFloor, BigNegative);
        TestEqual(TEXT("Delta clamps at the 0.0 floor, does not go negative"), AtFloor.Corruption, 0.0f);
    }

    return true;
}

// ---------------------------------------------------------------------------
// Полный путь через TickComponent: инвентарь-сушилка (StationType==DryingRack)
// доводит свежий предмет до bIsDried=true за DryingDurationSeconds, и после
// этого он портится заметно медленнее (DriedItemDecayMultiplier). Реестр
// недоступен в Editor-мире автотестов -- DriedStateDelta здесь честно не
// проверяется (см. довод в шапке файла), только таймер/decay-множитель.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistDrying_DryingRackDriesItemAndSlowsDecay,
    "Herbalist.Drying.DryingRackDriesItemAndSlowsDecay",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistDrying_DryingRackDriesItemAndSlowsDecay::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AActor* Owner = World->SpawnActor<AActor>();
    UHerbalistInventoryComponent* Rack = NewObject<UHerbalistInventoryComponent>(Owner);
    Rack->RegisterComponent();
    Rack->StationType = EProcessingStationType::DryingRack;

    FInventoryItem Item;
    Item.IngredientID = FName(TEXT("TestHerb"));
    Item.Count = 1;
    Item.bSubjectToDecay = true;
    Item.State.Meta.Stability = 1.0f;   // нулевая Instability -- decay почти не двигается сам по себе, эффект drying-множителя виден чище
    Rack->AddItem(Item, 1);

    FActorComponentTickFunction DummyTick;

    // Один тик (DecayUpdateInterval=1с) только взводит таймер -- ещё не сухо.
    Rack->TickComponent(1.0f, ELevelTick::LEVELTICK_All, &DummyTick);
    const FInventoryItem* SlotAfterFirstTick = Rack->GetSlot(0);
    if (!TestNotNull(TEXT("Slot present after first tick"), SlotAfterFirstTick)) { Owner->Destroy(); return false; }
    TestFalse(TEXT("Not dried after a single 1s tick (DryingDurationSeconds is far larger)"), SlotAfterFirstTick->bIsDried);
    TestTrue(TEXT("Timer is now armed (no longer the -1 sentinel)"), SlotAfterFirstTick->DryingTimeRemainingSeconds >= 0.0f);

    // TickComponent троттлит себя (TimeSinceLastDecayUpdate/DecayUpdateInterval,
    // см. HerbalistInventoryComponent.cpp) -- КАЖДЫЙ вызов, даже с огромным
    // DeltaTime, продвигает таймер сушки ровно на DecayUpdateInterval=1с
    // (ровно тот же приём троттлинга, что уже у decay). Один вызов с
    // DeltaTime=1000 НЕ пересекает 1920с сушки (глобальный фолбэк
    // UHerbalistSettings::DryingDurationSeconds, 2026-09-05 -- см. довод там)
    // за раз -- нужно 1920 отдельных срабатываний, IngredientRegistrySubsystem
    // недоступен в Editor-мире автотестов (см. шапку файла), поэтому
    // "TestHerb" не резолвится карточкой и всегда падает на глобальный
    // фолбэк. Гоняем DeltaTime=1с построчно (запас с лишним) до завершения,
    // тем же способом, каким реально тикает игра много кадров подряд.
    bool bBecameDried = false;
    for (int32 i = 0; i < 2000 && !bBecameDried; ++i)
    {
        Rack->TickComponent(1.0f, ELevelTick::LEVELTICK_All, &DummyTick);
        const FInventoryItem* Slot = Rack->GetSlot(0);
        if (!TestNotNull(TEXT("Slot still present during repeated ticking"), Slot)) { Owner->Destroy(); return false; }
        bBecameDried = Slot->bIsDried;
    }
    const FInventoryItem* SlotAfterLongTick = Rack->GetSlot(0);
    if (!TestNotNull(TEXT("Slot still present after repeated ticking"), SlotAfterLongTick)) { Owner->Destroy(); return false; }
    TestTrue(TEXT("Item is dried after enough time in the rack"), SlotAfterLongTick->bIsDried);
    TestEqual(TEXT("Timer settles at exactly 0 once finished"), SlotAfterLongTick->DryingTimeRemainingSeconds, 0.0f);

    Owner->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistDrying_DriedItemDecaysSlowerThanFreshItem,
    "Herbalist.Drying.DriedItemDecaysSlowerThanFreshItem",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistDrying_DriedItemDecaysSlowerThanFreshItem::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    // Свежий предмет, обычный инвентарь (не сушилка) -- decay без drying-множителя.
    AActor* FreshOwner = World->SpawnActor<AActor>();
    UHerbalistInventoryComponent* FreshInventory = NewObject<UHerbalistInventoryComponent>(FreshOwner);
    FreshInventory->RegisterComponent();

    FInventoryItem FreshItem;
    FreshItem.IngredientID = FName(TEXT("TestHerb"));
    FreshItem.Count = 1;
    FreshItem.bSubjectToDecay = true;
    FreshItem.State.Meta.Stability = 0.0f;   // максимальная Instability, как в StorageContainerTest.cpp
    FreshInventory->AddItem(FreshItem, 1);

    FActorComponentTickFunction DummyTick;
    FreshInventory->TickComponent(1.0f, ELevelTick::LEVELTICK_All, &DummyTick);
    const FInventoryItem* FreshSlot = FreshInventory->GetSlot(0);
    if (!TestNotNull(TEXT("Fresh slot present"), FreshSlot)) { FreshOwner->Destroy(); return false; }
    const float FreshDistortion = FreshSlot->State.Meta.Distortion;

    // Тот же предмет, но уже помеченный bIsDried=true до тика (симулирует
    // "уже высох ранее") -- в обычном (не-сушилка) инвентаре, тот же decay
    // switch-case, что и у свежего, разница -- ТОЛЬКО DriedItemDecayMultiplier.
    AActor* DriedOwner = World->SpawnActor<AActor>();
    UHerbalistInventoryComponent* DriedInventory = NewObject<UHerbalistInventoryComponent>(DriedOwner);
    DriedInventory->RegisterComponent();

    FInventoryItem DriedItem;
    DriedItem.IngredientID = FName(TEXT("TestHerb"));
    DriedItem.Count = 1;
    DriedItem.bSubjectToDecay = true;
    DriedItem.bIsDried = true;
    DriedItem.State.Meta.Stability = 0.0f;
    DriedInventory->AddItem(DriedItem, 1);

    DriedInventory->TickComponent(1.0f, ELevelTick::LEVELTICK_All, &DummyTick);
    const FInventoryItem* DriedSlot = DriedInventory->GetSlot(0);
    if (!TestNotNull(TEXT("Dried slot present"), DriedSlot)) { FreshOwner->Destroy(); DriedOwner->Destroy(); return false; }
    const float DriedDistortion = DriedSlot->State.Meta.Distortion;

    TestTrue(TEXT("Dried item decays (Distortion rises) noticeably slower than a fresh item over the same tick"),
        DriedDistortion < FreshDistortion);

    FreshOwner->Destroy();
    DriedOwner->Destroy();
    return true;
}

// ---------------------------------------------------------------------------
// Регрессия: предмет БЕЗ дельты сушки (обычный путь в реальной таблице, см.
// довод у FIngredientTableRow::DriedStateDelta -- дефолт "все нули")
// сохраняет свои оси State как были после высыхания -- меняются только
// bIsDried/decay, не State.Meta/Direction. Проверяем это напрямую через
// ApplyDriedStateDelta с нулевой дельтой (ровно то поведение, что получает
// любая карточка без ботанического обоснования в проходе 2026-09-04).
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistDrying_ZeroDeltaLeavesAxesUnchanged,
    "Herbalist.Drying.ZeroDeltaLeavesAxesUnchanged",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistDrying_ZeroDeltaLeavesAxesUnchanged::RunTest(const FString& Parameters)
{
    FMeta Meta;
    Meta.Distortion = 0.42f;
    Meta.Stability = 0.33f;
    Meta.Purity = 0.61f;
    Meta.Potency = 0.55f;
    Meta.Resonance = 0.28f;
    Meta.Corruption = 0.19f;

    const FMeta Snapshot = Meta;
    const FMeta ZeroDelta;   // все поля 0.0f по умолчанию -- дефолт FIngredientTableRow::DriedStateDelta

    UHerbalistInventoryComponent::ApplyDriedStateDelta(Meta, ZeroDelta);

    TestEqual(TEXT("Distortion unchanged"), Meta.Distortion, Snapshot.Distortion);
    TestEqual(TEXT("Stability unchanged"), Meta.Stability, Snapshot.Stability);
    TestEqual(TEXT("Purity unchanged"), Meta.Purity, Snapshot.Purity);
    TestEqual(TEXT("Potency unchanged"), Meta.Potency, Snapshot.Potency);
    TestEqual(TEXT("Resonance unchanged"), Meta.Resonance, Snapshot.Resonance);
    TestEqual(TEXT("Corruption unchanged"), Meta.Corruption, Snapshot.Corruption);

    return true;
}

// ---------------------------------------------------------------------------
// AreItemsStackable (через AddItem, публичный API -- сам метод protected):
// сушёный и свежий предмет одного вида не сливаются молча, и два предмета
// в процессе сушки (таймер взведён, ещё не досчитал) тоже не стекуются.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistDrying_DriedAndFreshDoNotStack,
    "Herbalist.Drying.DriedAndFreshDoNotStack",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistDrying_DriedAndFreshDoNotStack::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AActor* Owner = World->SpawnActor<AActor>();
    UHerbalistInventoryComponent* Inventory = NewObject<UHerbalistInventoryComponent>(Owner);
    Inventory->RegisterComponent();

    FInventoryItem Fresh;
    Fresh.IngredientID = FName(TEXT("TestHerb"));
    Fresh.Count = 1;
    Fresh.bIsDried = false;
    Inventory->AddItem(Fresh, 1);

    FInventoryItem Dried;
    Dried.IngredientID = FName(TEXT("TestHerb"));   // same species
    Dried.Count = 1;
    Dried.bIsDried = true;
    Inventory->AddItem(Dried, 1);

    TestEqual(TEXT("Fresh and dried items of the same species occupy two distinct slots, not merged"), Inventory->GetItems().Num(), 2);

    Owner->Destroy();
    return true;
}

// ---------------------------------------------------------------------------
// ResolveDryingDurationSeconds -- чистая функция резолва приоритета
// "карточка vs глобальный фолбэк" (2026-09-05, "процесс сушки у разных
// растений разный (длительность)", прямой запрос пользователя). Рукописная
// FIngredientTableRow вместо реестра, тем же приёмом, что и остальные тесты
// этого файла (см. шапку).
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistDrying_ResolveDryingDurationSecondsPrefersCardOverGlobal,
    "Herbalist.Drying.ResolveDryingDurationSecondsPrefersCardOverGlobal",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistDrying_ResolveDryingDurationSecondsPrefersCardOverGlobal::RunTest(const FString& Parameters)
{
    const float GlobalFallback = 1920.0f;   // текущий UHerbalistSettings::DryingDurationSeconds (1 игровые сутки)

    // (a) Карточка не резолвится вовсе (Row == nullptr -- ингредиент не
    // найден в реестре, тот же случай, что реестр недоступен) -- падаем на
    // глобальный фолбэк.
    {
        const float Result = UHerbalistInventoryComponent::ResolveDryingDurationSeconds(nullptr, GlobalFallback);
        TestEqual(TEXT("No row at all -- falls back to the global setting"), Result, GlobalFallback);
    }

    // (a) Карточка резолвится, но НЕ несёт явного значения -- сентинел -1,
    // ДЕФОЛТ FIngredientTableRow::DryingDurationSeconds ровно для карточек,
    // для которых заход забудет проставить число -- тоже падаем на глобальный
    // фолбэк, не на 0/мгновенную сушку.
    {
        FIngredientTableRow RowWithoutExplicitValue;
        TestEqual(TEXT("Default DryingDurationSeconds is the -1 sentinel"), RowWithoutExplicitValue.DryingDurationSeconds, -1.0f);

        const float Result = UHerbalistInventoryComponent::ResolveDryingDurationSeconds(&RowWithoutExplicitValue, GlobalFallback);
        TestEqual(TEXT("Row with the -1 sentinel -- falls back to the global setting"), Result, GlobalFallback);
    }

    // (b) Карточка несёт ЯВНОЕ значение, короткое, отличное от глобального --
    // используем ЕГО, не глобальное (реальный случай: тонкая трава сохнет
    // куда быстрее плотного корня/коры, см. довод у
    // FIngredientTableRow::DryingDurationSeconds).
    {
        FIngredientTableRow RowWithExplicitValue;
        RowWithExplicitValue.DryingDurationSeconds = 300.0f;

        const float Result = UHerbalistInventoryComponent::ResolveDryingDurationSeconds(&RowWithExplicitValue, GlobalFallback);
        TestEqual(TEXT("Row with an explicit value -- uses the card's own number"), Result, 300.0f);
        TestTrue(TEXT("Card value differs from the global fallback in this test on purpose"), Result != GlobalFallback);
    }

    return true;
}

// Комбинация ResolveDryingDurationSeconds + TickDryingItem (обе чистые,
// проверенные по отдельности выше/в TickDryingItemStartsCountsDownCompletes)
// на короткой дистанции: карточка с явным коротким числом досыхает за
// считанные тики, тот же ингредиент БЕЗ карточки (падает на глобальный
// 1920с фолбэк) за те же несколько тиков ещё даже не близко -- разница
// видна за 6 тиков теста, не нужно крутить полные 1920 итераций.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistDrying_CardDurationDriesFasterThanGlobalFallback,
    "Herbalist.Drying.CardDurationDriesFasterThanGlobalFallback",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistDrying_CardDurationDriesFasterThanGlobalFallback::RunTest(const FString& Parameters)
{
    const float GlobalFallback = 1920.0f;

    FIngredientTableRow ShortDryingCard;
    ShortDryingCard.DryingDurationSeconds = 5.0f;   // тонкий лист, короче фолбэка на три порядка

    FInventoryItem CardItem;
    CardItem.IngredientID = FName(TEXT("TestShortDryHerb"));

    FInventoryItem FallbackItem;
    FallbackItem.IngredientID = FName(TEXT("TestNoCardHerb"));

    for (int32 i = 0; i < 6; ++i)
    {
        const float CardDuration = UHerbalistInventoryComponent::ResolveDryingDurationSeconds(&ShortDryingCard, GlobalFallback);
        UHerbalistInventoryComponent::TickDryingItem(CardItem, 1.0f, CardDuration);

        const float FallbackDuration = UHerbalistInventoryComponent::ResolveDryingDurationSeconds(nullptr, GlobalFallback);
        UHerbalistInventoryComponent::TickDryingItem(FallbackItem, 1.0f, FallbackDuration);
    }

    TestTrue(TEXT("Item whose card names a short 5s duration is fully dried after 6s of ticking"), CardItem.bIsDried);
    TestFalse(TEXT("Item with no card (1920s global fallback) is nowhere near done after the same 6s"), FallbackItem.bIsDried);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistDrying_InProgressItemsDoNotStack,
    "Herbalist.Drying.InProgressItemsDoNotStack",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistDrying_InProgressItemsDoNotStack::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AActor* Owner = World->SpawnActor<AActor>();
    UHerbalistInventoryComponent* Inventory = NewObject<UHerbalistInventoryComponent>(Owner);
    Inventory->RegisterComponent();

    FInventoryItem FirstBatch;
    FirstBatch.IngredientID = FName(TEXT("TestHerb"));
    FirstBatch.Count = 1;
    FirstBatch.DryingTimeRemainingSeconds = 300.0f;   // already mid-drying
    Inventory->AddItem(FirstBatch, 1);

    FInventoryItem SecondBatch;
    SecondBatch.IngredientID = FName(TEXT("TestHerb"));   // same species
    SecondBatch.Count = 1;
    SecondBatch.DryingTimeRemainingSeconds = 150.0f;   // a different, independent timer
    Inventory->AddItem(SecondBatch, 1);

    TestEqual(TEXT("Two independently-timed drying batches of the same species stay in separate slots"), Inventory->GetItems().Num(), 2);

    Owner->Destroy();
    return true;
}

#endif // WITH_AUTOMATION_TESTS && WITH_EDITOR
