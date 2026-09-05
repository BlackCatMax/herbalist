// Source/ProjectHerbalistTests/Private/Tests/PotionProcessingTest.cpp
//
// Многоступенчатые зелья (2026-09-05, прямой запрос пользователя): "1.
// Готовим обычное зелье... 2. Отстой — процесс усиления доминирующей оси...
// 3. Варка с этим зельем в качестве основы... 4. Фильтрация. 5.
// Выпаривание... не все эти действия вместе, а комбинации... игрок должен
// понимать, ДЛЯ ЧЕГО оно — что-то усиливается и что-то теряется". Шаг 3
// (варка на основе зелья) уже работал без единой правки кода (зелья
// хранятся под общим IngredientID="Potion", вся алхимия считается по
// FInventoryItem::State, не по ID — AlchemySlotWidget.cpp::CanAcceptItem уже
// пускает Potion в обычный слот ингредиента) -- см. CHANGELOG.md, здесь не
// тестируется отдельно (нечего тестировать, поведение не менялось).
//
// Отстой и Выпаривание — растянутые во времени станции-процессы (тот же
// класс механики, что уже Сушка, EProcessingStationType, см. довод в
// HerbalistInventoryComponent.h), подтверждено явным выбором пользователя.
// Фильтрация — мгновенное действие без станции (UHerbalistInventoryComponent::
// TryFilterPotion).
//
// Та же трёхуровневая граница проверки, что уже DryingTest.cpp:
// 1. Tick*Item/Apply*Effect -- чистые функции состояния, без реестра.
// 2. Полный путь через TickComponent (StationType==SettlingStand/
//    EvaporationStill) -- таймер взводится/считает/завершается, эффект
//    применяется РОВНО ОДИН РАЗ (терминальность).
// 3. Регрессия -- предмет, не бывший в подходящей станции (или не Potion),
//    не меняется случайно на обычном TickComponent.

#include "Core/Inventory/HerbalistInventoryComponent.h"
#include "Core/Types/HerbalistCoreTypes.h"
#include "Misc/AutomationTest.h"
#include "Editor.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

#if WITH_AUTOMATION_TESTS && WITH_EDITOR

// ---------------------------------------------------------------------------
// TickSettlingItem -- чистая функция таймера, тот же приём, что TickDryingItem.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistPotionProcessing_TickSettlingItemStartsCountsDownCompletes,
    "Herbalist.PotionProcessing.TickSettlingItemStartsCountsDownCompletes",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistPotionProcessing_TickSettlingItemStartsCountsDownCompletes::RunTest(const FString& Parameters)
{
    FInventoryItem Item;
    Item.IngredientID = FName(TEXT("Potion"));
    Item.Count = 1;
    TestEqual(TEXT("Fresh item starts with the -1 sentinel"), Item.SettlingTimeRemainingSeconds, -1.0f);
    TestFalse(TEXT("Fresh item has not settled"), Item.bHasSettled);

    const bool bCompletedOnStart = UHerbalistInventoryComponent::TickSettlingItem(Item, 1.0f, 10.0f);
    TestFalse(TEXT("First call only arms the timer"), bCompletedOnStart);
    TestEqual(TEXT("Timer armed to the full duration"), Item.SettlingTimeRemainingSeconds, 10.0f);

    const bool bCompletedMidway = UHerbalistInventoryComponent::TickSettlingItem(Item, 4.0f, 10.0f);
    TestFalse(TEXT("Midway through, still not complete"), bCompletedMidway);
    TestEqual(TEXT("Remaining time decremented"), Item.SettlingTimeRemainingSeconds, 6.0f);

    const bool bCompletedAtEnd = UHerbalistInventoryComponent::TickSettlingItem(Item, 7.0f, 10.0f);
    TestTrue(TEXT("Crossing zero reports completion"), bCompletedAtEnd);
    TestEqual(TEXT("Remaining time clamps at exactly 0"), Item.SettlingTimeRemainingSeconds, 0.0f);
    TestTrue(TEXT("Item is now marked settled"), Item.bHasSettled);

    return true;
}

// ---------------------------------------------------------------------------
// ApplySettlingEffect -- доминирующая ось растёт, остальные три проседают
// пропорционально (сумма всё ещё 1.0 после NormalizeSum), Magnitude падает
// на ожидаемый множитель.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistPotionProcessing_ApplySettlingEffectBoostsDominantAxis,
    "Herbalist.PotionProcessing.ApplySettlingEffectBoostsDominantAxis",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistPotionProcessing_ApplySettlingEffectBoostsDominantAxis::RunTest(const FString& Parameters)
{
    FRealState State;
    State.Magnitude = 0.8f;
    State.Direction.Body = 0.1f;
    State.Direction.Mind = 0.5f;   // доминирующая ось
    State.Direction.Spirit = 0.2f;
    State.Direction.Nature = 0.2f;

    const float PreBoostBody = State.Direction.Body;
    const float PreBoostSpirit = State.Direction.Spirit;
    const float PreBoostNature = State.Direction.Nature;
    const float PreBoostMind = State.Direction.Mind;

    UHerbalistInventoryComponent::ApplySettlingEffect(State, 0.15f, 0.9f);

    const float Sum = State.Direction.Body + State.Direction.Mind + State.Direction.Spirit + State.Direction.Nature;
    TestTrue(TEXT("Direction still sums to 1.0 after NormalizeSum"), FMath::IsNearlyEqual(Sum, 1.0f, KINDA_SMALL_NUMBER));

    TestTrue(TEXT("Dominant axis (Mind) grew relative to the others"),
        State.Direction.Mind > State.Direction.Body &&
        State.Direction.Mind > State.Direction.Spirit &&
        State.Direction.Mind > State.Direction.Nature);

    // Body/Spirit/Nature не получили буст напрямую -- NormalizeSum делит их
    // тем же общим множителем (1 / новую сумму), поэтому их ВЗАИМНОЕ
    // соотношение не меняется, хотя абсолютное значение каждого падает
    // ("просаживаются пропорционально").
    const float PreRatioSpiritToBody = PreBoostSpirit / PreBoostBody;
    const float PreRatioNatureToBody = PreBoostNature / PreBoostBody;
    const float PostRatioSpiritToBody = State.Direction.Spirit / State.Direction.Body;
    const float PostRatioNatureToBody = State.Direction.Nature / State.Direction.Body;
    TestTrue(TEXT("Non-dominant axes keep their relative ratio (proportional drop, not arbitrary)"),
        FMath::IsNearlyEqual(PreRatioSpiritToBody, PostRatioSpiritToBody, KINDA_SMALL_NUMBER) &&
        FMath::IsNearlyEqual(PreRatioNatureToBody, PostRatioNatureToBody, KINDA_SMALL_NUMBER));
    TestTrue(TEXT("Non-dominant axes fell in absolute terms"),
        State.Direction.Body < PreBoostBody && State.Direction.Spirit < PreBoostSpirit && State.Direction.Nature < PreBoostNature);

    const float ExpectedMagnitude = 0.8f * 0.9f;
    TestEqual(TEXT("Magnitude falls by the loss factor -- the price of settling"), State.Magnitude, ExpectedMagnitude);

    return true;
}

// ---------------------------------------------------------------------------
// TickEvaporationItem -- та же граница таймера, независимая пара полей.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistPotionProcessing_TickEvaporationItemStartsCountsDownCompletes,
    "Herbalist.PotionProcessing.TickEvaporationItemStartsCountsDownCompletes",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistPotionProcessing_TickEvaporationItemStartsCountsDownCompletes::RunTest(const FString& Parameters)
{
    FInventoryItem Item;
    Item.IngredientID = FName(TEXT("Potion"));
    Item.Count = 1;
    TestEqual(TEXT("Fresh item starts with the -1 sentinel"), Item.EvaporationTimeRemainingSeconds, -1.0f);

    const bool bCompletedOnStart = UHerbalistInventoryComponent::TickEvaporationItem(Item, 1.0f, 10.0f);
    TestFalse(TEXT("First call only arms the timer"), bCompletedOnStart);
    TestEqual(TEXT("Timer armed to the full duration"), Item.EvaporationTimeRemainingSeconds, 10.0f);

    const bool bCompletedAtEnd = UHerbalistInventoryComponent::TickEvaporationItem(Item, 20.0f, 10.0f);
    TestTrue(TEXT("Overshoot still reports completion"), bCompletedAtEnd);
    TestEqual(TEXT("Remaining time clamps at exactly 0, does not go negative"), Item.EvaporationTimeRemainingSeconds, 0.0f);
    TestTrue(TEXT("Item is now marked evaporated"), Item.bHasEvaporated);

    return true;
}

// ---------------------------------------------------------------------------
// ApplyEvaporationEffect -- Magnitude/Potency растут (усиление), Distortion
// И Corruption растут вместе (концентрация не разбирает, что усиливать).
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistPotionProcessing_ApplyEvaporationEffectBoostsAndConcentratesRisk,
    "Herbalist.PotionProcessing.ApplyEvaporationEffectBoostsAndConcentratesRisk",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistPotionProcessing_ApplyEvaporationEffectBoostsAndConcentratesRisk::RunTest(const FString& Parameters)
{
    {
        FRealState State;
        State.Magnitude = 0.5f;
        State.Meta.Potency = 0.5f;
        State.Meta.Distortion = 0.4f;
        State.Meta.Corruption = 0.3f;

        UHerbalistInventoryComponent::ApplyEvaporationEffect(State, 1.2f, 0.15f, 1.2f);

        TestEqual(TEXT("Magnitude grows by the boost multiplier"), State.Magnitude, 0.5f * 1.2f);
        TestEqual(TEXT("Potency grows by the flat boost"), State.Meta.Potency, 0.65f);
        TestEqual(TEXT("Distortion is concentrated by the same risk multiplier"), State.Meta.Distortion, FMath::Clamp(0.4f * 1.2f, 0.0f, 1.0f));
        TestEqual(TEXT("Corruption is concentrated by the same risk multiplier"), State.Meta.Corruption, FMath::Clamp(0.3f * 1.2f, 0.0f, 1.0f));
    }

    // Клампы на потолке -- concentration не должна утащить оси за пределы [0,1].
    {
        FRealState AtCeiling;
        AtCeiling.Magnitude = 0.9f;
        AtCeiling.Meta.Potency = 0.95f;
        AtCeiling.Meta.Distortion = 0.9f;
        AtCeiling.Meta.Corruption = 0.9f;

        UHerbalistInventoryComponent::ApplyEvaporationEffect(AtCeiling, 1.2f, 0.15f, 1.2f);

        TestEqual(TEXT("Magnitude clamps at 1.0 ceiling"), AtCeiling.Magnitude, 1.0f);
        TestEqual(TEXT("Potency clamps at 1.0 ceiling"), AtCeiling.Meta.Potency, 1.0f);
        TestEqual(TEXT("Distortion clamps at 1.0 ceiling"), AtCeiling.Meta.Distortion, 1.0f);
        TestEqual(TEXT("Corruption clamps at 1.0 ceiling"), AtCeiling.Meta.Corruption, 1.0f);
    }

    return true;
}

// ---------------------------------------------------------------------------
// ApplyFilterEffect -- мгновенно (без тиков): Purity растёт, Distortion/
// Corruption падают, Potency падает как цена. Повторное применение
// РАЗРЕШЕНО и складывается (прямое архитектурное решение, см. довод у
// TryFilterPotion в HerbalistInventoryComponent.h) -- но самоограничено
// клампами [0,1], проверяем оба конца: складывается на нескольких
// применениях, и не переливается за границу при многократном повторе.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistPotionProcessing_ApplyFilterEffectInstantAndRepeatable,
    "Herbalist.PotionProcessing.ApplyFilterEffectInstantAndRepeatable",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistPotionProcessing_ApplyFilterEffectInstantAndRepeatable::RunTest(const FString& Parameters)
{
    // Одно применение -- мгновенный эффект, без единого тика/таймера.
    {
        FMeta Meta;
        Meta.Purity = 0.4f;
        Meta.Distortion = 0.5f;
        Meta.Corruption = 0.5f;
        Meta.Potency = 0.6f;

        UHerbalistInventoryComponent::ApplyFilterEffect(Meta, 0.15f, 0.1f, 0.1f, 0.1f);

        TestEqual(TEXT("Purity rises"), Meta.Purity, 0.55f);
        TestEqual(TEXT("Distortion falls"), Meta.Distortion, 0.4f);
        TestEqual(TEXT("Corruption falls"), Meta.Corruption, 0.4f);
        TestEqual(TEXT("Potency falls -- the price of a cleaner potion"), Meta.Potency, 0.5f);

        // Второе применение на том же зелье -- эффект СКЛАДЫВАЕТСЯ (решение:
        // повторная фильтрация разрешена, не блокируется терминальным флагом).
        UHerbalistInventoryComponent::ApplyFilterEffect(Meta, 0.15f, 0.1f, 0.1f, 0.1f);
        TestEqual(TEXT("Second filtering compounds Purity further"), Meta.Purity, 0.70f);
        TestEqual(TEXT("Second filtering compounds Distortion reduction further"), Meta.Distortion, 0.3f);
        TestEqual(TEXT("Second filtering compounds Potency loss further"), Meta.Potency, 0.4f);
    }

    // Много применений подряд -- самоограничено клампами, не бесконечный
    // рост/провал за пределы [0,1] (иначе повтор был бы эксплойтом).
    {
        FMeta Meta;
        Meta.Purity = 0.4f;
        Meta.Potency = 0.25f;

        for (int32 i = 0; i < 10; ++i)
        {
            UHerbalistInventoryComponent::ApplyFilterEffect(Meta, 0.15f, 0.1f, 0.1f, 0.1f);
        }

        TestEqual(TEXT("Purity saturates at the 1.0 ceiling after enough repeats, does not overflow"), Meta.Purity, 1.0f);
        TestEqual(TEXT("Potency floors at 0.0 after enough repeats, does not go negative"), Meta.Potency, 0.0f);
    }

    return true;
}

// ---------------------------------------------------------------------------
// Полный путь через TickComponent: отстойник (StationType==SettlingStand)
// доводит зелье до bHasSettled=true за SettlingDurationSeconds, дальнейшие
// тики не удваивают эффект (терминальность).
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistPotionProcessing_SettlingStandAppliesEffectOnceThenStops,
    "Herbalist.PotionProcessing.SettlingStandAppliesEffectOnceThenStops",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistPotionProcessing_SettlingStandAppliesEffectOnceThenStops::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AActor* Owner = World->SpawnActor<AActor>();
    UHerbalistInventoryComponent* Stand = NewObject<UHerbalistInventoryComponent>(Owner);
    Stand->RegisterComponent();
    Stand->StationType = EProcessingStationType::SettlingStand;

    FInventoryItem Item;
    Item.IngredientID = FName(TEXT("Potion"));
    Item.Count = 1;
    Item.bSubjectToDecay = true;
    Item.State.Meta.Stability = 1.0f;   // нулевая Instability -- decay не смазывает наблюдаемый эффект отстоя
    Item.State.Magnitude = 0.8f;
    Item.State.Direction.Mind = 0.5f;
    Item.State.Direction.Body = 0.2f;
    Item.State.Direction.Spirit = 0.2f;
    Item.State.Direction.Nature = 0.1f;
    Stand->AddItem(Item, 1);

    FActorComponentTickFunction DummyTick;

    // Один тик -- только взводит таймер (см. довод у аналогичного теста
    // DryingRackDriesItemAndSlowsDecay, тот же троттлинг DecayUpdateInterval).
    Stand->TickComponent(1.0f, ELevelTick::LEVELTICK_All, &DummyTick);
    const FInventoryItem* SlotAfterFirstTick = Stand->GetSlot(0);
    if (!TestNotNull(TEXT("Slot present after first tick"), SlotAfterFirstTick)) { Owner->Destroy(); return false; }
    TestFalse(TEXT("Not settled after a single 1s tick"), SlotAfterFirstTick->bHasSettled);
    TestTrue(TEXT("Timer armed"), SlotAfterFirstTick->SettlingTimeRemainingSeconds >= 0.0f);

    bool bBecameSettled = false;
    for (int32 i = 0; i < 1000 && !bBecameSettled; ++i)
    {
        Stand->TickComponent(1.0f, ELevelTick::LEVELTICK_All, &DummyTick);
        const FInventoryItem* Slot = Stand->GetSlot(0);
        if (!TestNotNull(TEXT("Slot still present during repeated ticking"), Slot)) { Owner->Destroy(); return false; }
        bBecameSettled = Slot->bHasSettled;
    }

    const FInventoryItem* SlotAfterCompletion = Stand->GetSlot(0);
    if (!TestNotNull(TEXT("Slot present after completion"), SlotAfterCompletion)) { Owner->Destroy(); return false; }
    TestTrue(TEXT("Item settled after enough time on the stand"), SlotAfterCompletion->bHasSettled);
    TestTrue(TEXT("Dominant axis (Mind) grew past its original 0.5 share"), SlotAfterCompletion->State.Direction.Mind > 0.5f);
    TestTrue(TEXT("Magnitude fell (price of settling)"), SlotAfterCompletion->State.Magnitude < 0.8f);

    const float DirectionAfterCompletion = SlotAfterCompletion->State.Direction.Mind;
    const float MagnitudeAfterCompletion = SlotAfterCompletion->State.Magnitude;

    // Дальнейшие тики -- эффект уже применён (bHasSettled=true), TickComponent
    // не должен вызвать ApplySettlingEffect повторно (терминальность).
    for (int32 i = 0; i < 5; ++i)
    {
        Stand->TickComponent(1.0f, ELevelTick::LEVELTICK_All, &DummyTick);
    }
    const FInventoryItem* SlotAfterExtraTicks = Stand->GetSlot(0);
    if (!TestNotNull(TEXT("Slot present after extra ticks"), SlotAfterExtraTicks)) { Owner->Destroy(); return false; }
    TestEqual(TEXT("Dominant axis unchanged by extra ticks -- effect not doubled"), SlotAfterExtraTicks->State.Direction.Mind, DirectionAfterCompletion);
    TestEqual(TEXT("Magnitude unchanged by extra ticks -- effect not doubled"), SlotAfterExtraTicks->State.Magnitude, MagnitudeAfterCompletion);

    Owner->Destroy();
    return true;
}

// ---------------------------------------------------------------------------
// Тот же полный путь для выпарного куба.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistPotionProcessing_EvaporationStillAppliesEffectOnceThenStops,
    "Herbalist.PotionProcessing.EvaporationStillAppliesEffectOnceThenStops",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistPotionProcessing_EvaporationStillAppliesEffectOnceThenStops::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AActor* Owner = World->SpawnActor<AActor>();
    UHerbalistInventoryComponent* Still = NewObject<UHerbalistInventoryComponent>(Owner);
    Still->RegisterComponent();
    Still->StationType = EProcessingStationType::EvaporationStill;

    FInventoryItem Item;
    Item.IngredientID = FName(TEXT("Potion"));
    Item.Count = 1;
    Item.bSubjectToDecay = true;
    Item.State.Meta.Stability = 1.0f;
    Item.State.Magnitude = 0.5f;
    Item.State.Meta.Potency = 0.5f;
    Item.State.Meta.Distortion = 0.3f;
    Item.State.Meta.Corruption = 0.2f;
    Still->AddItem(Item, 1);

    FActorComponentTickFunction DummyTick;
    Still->TickComponent(1.0f, ELevelTick::LEVELTICK_All, &DummyTick);
    const FInventoryItem* SlotAfterFirstTick = Still->GetSlot(0);
    if (!TestNotNull(TEXT("Slot present after first tick"), SlotAfterFirstTick)) { Owner->Destroy(); return false; }
    TestFalse(TEXT("Not evaporated after a single 1s tick"), SlotAfterFirstTick->bHasEvaporated);

    bool bBecameEvaporated = false;
    for (int32 i = 0; i < 2000 && !bBecameEvaporated; ++i)
    {
        Still->TickComponent(1.0f, ELevelTick::LEVELTICK_All, &DummyTick);
        const FInventoryItem* Slot = Still->GetSlot(0);
        if (!TestNotNull(TEXT("Slot still present during repeated ticking"), Slot)) { Owner->Destroy(); return false; }
        bBecameEvaporated = Slot->bHasEvaporated;
    }

    const FInventoryItem* SlotAfterCompletion = Still->GetSlot(0);
    if (!TestNotNull(TEXT("Slot present after completion"), SlotAfterCompletion)) { Owner->Destroy(); return false; }
    TestTrue(TEXT("Item evaporated after enough time in the still"), SlotAfterCompletion->bHasEvaporated);
    TestTrue(TEXT("Magnitude rose (concentration)"), SlotAfterCompletion->State.Magnitude > 0.5f);
    TestTrue(TEXT("Potency rose (concentration)"), SlotAfterCompletion->State.Meta.Potency > 0.5f);
    TestTrue(TEXT("Distortion rose (risk concentrates too)"), SlotAfterCompletion->State.Meta.Distortion > 0.3f);
    TestTrue(TEXT("Corruption rose (risk concentrates too)"), SlotAfterCompletion->State.Meta.Corruption > 0.2f);

    const float MagnitudeAfterCompletion = SlotAfterCompletion->State.Magnitude;
    const float DistortionAfterCompletion = SlotAfterCompletion->State.Meta.Distortion;

    for (int32 i = 0; i < 5; ++i)
    {
        Still->TickComponent(1.0f, ELevelTick::LEVELTICK_All, &DummyTick);
    }
    const FInventoryItem* SlotAfterExtraTicks = Still->GetSlot(0);
    if (!TestNotNull(TEXT("Slot present after extra ticks"), SlotAfterExtraTicks)) { Owner->Destroy(); return false; }
    TestEqual(TEXT("Magnitude unchanged by extra ticks -- effect not doubled"), SlotAfterExtraTicks->State.Magnitude, MagnitudeAfterCompletion);
    TestEqual(TEXT("Distortion unchanged by extra ticks -- effect not doubled"), SlotAfterExtraTicks->State.Meta.Distortion, DistortionAfterCompletion);

    Owner->Destroy();
    return true;
}

// ---------------------------------------------------------------------------
// TryFilterPotion -- находит первый Potion в реальном инвентаре и применяет
// эффект МГНОВЕННО, без единого TickComponent.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistPotionProcessing_TryFilterPotionAppliesInstantlyWithoutTicks,
    "Herbalist.PotionProcessing.TryFilterPotionAppliesInstantlyWithoutTicks",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistPotionProcessing_TryFilterPotionAppliesInstantlyWithoutTicks::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AActor* Owner = World->SpawnActor<AActor>();
    UHerbalistInventoryComponent* Inventory = NewObject<UHerbalistInventoryComponent>(Owner);
    Inventory->RegisterComponent();

    FInventoryItem NotAPotion;
    NotAPotion.IngredientID = FName(TEXT("TestHerb"));
    NotAPotion.Count = 1;
    Inventory->AddItem(NotAPotion, 1);

    // Без зелья в инвентаре -- отказ, ничего не падает.
    TestFalse(TEXT("No potion in inventory -- TryFilterPotion fails cleanly"), Inventory->TryFilterPotion());

    FInventoryItem Potion;
    Potion.IngredientID = FName(TEXT("Potion"));
    Potion.Count = 1;
    Potion.State.Meta.Purity = 0.3f;
    Potion.State.Meta.Distortion = 0.6f;
    Potion.State.Meta.Corruption = 0.5f;
    Potion.State.Meta.Potency = 0.7f;
    Inventory->AddItem(Potion, 1);

    // Не вызываем TickComponent ни разу -- эффект должен быть виден сразу
    // после единственного вызова TryFilterPotion (мгновенное действие,
    // в отличие от Отстоя/Выпаривания).
    TestTrue(TEXT("TryFilterPotion finds the potion and applies the effect"), Inventory->TryFilterPotion());

    const int32 PotionIndex = Inventory->GetItems().IndexOfByPredicate([](const FInventoryItem& Item)
    {
        return Item.IngredientID == FName(TEXT("Potion"));
    });
    if (!TestTrue(TEXT("Potion slot found after filtering"), PotionIndex != INDEX_NONE)) { Owner->Destroy(); return false; }

    const FInventoryItem* FilteredSlot = Inventory->GetSlot(PotionIndex);
    if (!TestNotNull(TEXT("Filtered slot present"), FilteredSlot)) { Owner->Destroy(); return false; }

    TestTrue(TEXT("Purity rose instantly, no ticks needed"), FilteredSlot->State.Meta.Purity > 0.3f);
    TestTrue(TEXT("Distortion fell instantly"), FilteredSlot->State.Meta.Distortion < 0.6f);
    TestTrue(TEXT("Corruption fell instantly"), FilteredSlot->State.Meta.Corruption < 0.5f);
    TestTrue(TEXT("Potency fell instantly -- the price"), FilteredSlot->State.Meta.Potency < 0.7f);

    Owner->Destroy();
    return true;
}

// ---------------------------------------------------------------------------
// Регрессия: обычный инвентарь (StationType::None) с зельем внутри не
// запускает Отстой/Выпаривание случайно на обычном TickComponent -- тот же
// класс проверки, что уже FHerbalistDrying_* тесты делают для сушки.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistPotionProcessing_OrdinaryInventoryDoesNotProcessPotionOnTick,
    "Herbalist.PotionProcessing.OrdinaryInventoryDoesNotProcessPotionOnTick",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistPotionProcessing_OrdinaryInventoryDoesNotProcessPotionOnTick::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AActor* Owner = World->SpawnActor<AActor>();
    UHerbalistInventoryComponent* Inventory = NewObject<UHerbalistInventoryComponent>(Owner);
    Inventory->RegisterComponent();
    // StationType stays None -- обычный инвентарь/тара.

    FInventoryItem Potion;
    Potion.IngredientID = FName(TEXT("Potion"));
    Potion.Count = 1;
    Potion.bSubjectToDecay = true;
    Potion.State.Meta.Stability = 1.0f;
    Inventory->AddItem(Potion, 1);

    FActorComponentTickFunction DummyTick;
    for (int32 i = 0; i < 20; ++i)
    {
        Inventory->TickComponent(1.0f, ELevelTick::LEVELTICK_All, &DummyTick);
    }

    const FInventoryItem* Slot = Inventory->GetSlot(0);
    if (!TestNotNull(TEXT("Slot present after ticking"), Slot)) { Owner->Destroy(); return false; }
    TestFalse(TEXT("Potion in an ordinary inventory never settles"), Slot->bHasSettled);
    TestEqual(TEXT("SettlingTimeRemainingSeconds stays at the -1 sentinel -- never armed"), Slot->SettlingTimeRemainingSeconds, -1.0f);
    TestFalse(TEXT("Potion in an ordinary inventory never evaporates"), Slot->bHasEvaporated);
    TestEqual(TEXT("EvaporationTimeRemainingSeconds stays at the -1 sentinel -- never armed"), Slot->EvaporationTimeRemainingSeconds, -1.0f);

    Owner->Destroy();
    return true;
}

// ---------------------------------------------------------------------------
// Регрессия: сырая трава (не Potion) на отстойнике НЕ отстаивается -- эффект
// намеренно ограничен готовыми зельями (см. довод у PotionIngredientID,
// HerbalistInventoryComponent.cpp).
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistPotionProcessing_RawHerbOnSettlingStandDoesNotSettle,
    "Herbalist.PotionProcessing.RawHerbOnSettlingStandDoesNotSettle",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistPotionProcessing_RawHerbOnSettlingStandDoesNotSettle::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AActor* Owner = World->SpawnActor<AActor>();
    UHerbalistInventoryComponent* Stand = NewObject<UHerbalistInventoryComponent>(Owner);
    Stand->RegisterComponent();
    Stand->StationType = EProcessingStationType::SettlingStand;

    FInventoryItem Herb;
    Herb.IngredientID = FName(TEXT("TestHerb"));   // не "Potion"
    Herb.Count = 1;
    Herb.bSubjectToDecay = true;
    Herb.State.Meta.Stability = 1.0f;
    Stand->AddItem(Herb, 1);

    FActorComponentTickFunction DummyTick;
    for (int32 i = 0; i < 20; ++i)
    {
        Stand->TickComponent(1.0f, ELevelTick::LEVELTICK_All, &DummyTick);
    }

    const FInventoryItem* Slot = Stand->GetSlot(0);
    if (!TestNotNull(TEXT("Slot present after ticking"), Slot)) { Owner->Destroy(); return false; }
    TestFalse(TEXT("Raw herb on a settling stand never settles -- gated to Potion only"), Slot->bHasSettled);
    TestEqual(TEXT("Timer stays at the -1 sentinel"), Slot->SettlingTimeRemainingSeconds, -1.0f);

    Owner->Destroy();
    return true;
}

#endif // WITH_AUTOMATION_TESTS && WITH_EDITOR
