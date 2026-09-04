// Source/ProjectHerbalistTests/Private/Tests/WardTest.cpp
//
// Обереги (кристаллы Пещеры, DESIGN_Community_And_Homestead.md §2.4,
// 2026-09-04) — "работа оберегов слабая, но постоянная, есть временные
// лимиты, не постоянное действие". Тот же DispatchBeginPlay-паттерн и тот
// же вертикальный срез, что уже ArtifactEffectsTest.cpp применяет к
// Шапке-невидимке/Камню-обереgу: проверяет состояние/геометрию/таймер на
// GridWorldManager напрямую, не гоняет полный UpdateEntityManifestations
// (та интеграция уже покрыта LegendaryEntityTest.cpp/SystemInteractionTest.cpp
// для того же класса гейта — IsInvisibilityCapActive рядом с
// IsWardConcealmentActive в одном && цепочки). Резолв кристалла из
// инвентаря по имени (AHerbalistPlayerController::ActivateWard) намеренно
// НЕ тестируется здесь — тот же класс пробела, что уже у TradeWithCommunity
// (IngredientRegistrySubsystem недоступен через GameInstance в Editor-мире
// автотестов, см. ROADMAP.md); эти тесты бьют напрямую в уже резолвленный
// AGridWorldManager::ActivateWardBrewBoost/ActivateWardConcealment.

#include "Core/World/GridWorldManager.h"
#include "Misc/AutomationTest.h"
#include "Editor.h"
#include "Engine/World.h"

#if WITH_AUTOMATION_TESTS && WITH_EDITOR

#include "TestWorldHelpers.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistWard_BrewBoostActivatesAndExpires,
    "Herbalist.Ward.BrewBoostActivatesAndExpires",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistWard_BrewBoostActivatesAndExpires::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;

    TestFalse(TEXT("Not active before activation"), Manager->IsWardBrewBoostActive());

    Manager->SetGameClockSeconds(1000.0f);
    TestTrue(TEXT("Activates"), Manager->ActivateWardBrewBoost());
    TestTrue(TEXT("Active immediately after activation"), Manager->IsWardBrewBoostActive());

    // "Есть временные лимиты, не постоянное действие" -- продвигаем часы за
    // пределы WardDurationSeconds (черновое число 600, см. HerbalistSettings.h)
    // и проверяем, что эффект действительно гаснет, не висит вечно.
    Manager->SetGameClockSeconds(1000.0f + 601.0f);
    TestFalse(TEXT("Expires after WardDurationSeconds"), Manager->IsWardBrewBoostActive());

    // Не разовый расходуемый предмет -- реактивация после истечения снова
    // включает эффект (тот же принцип, что уже Шапка-невидимка).
    TestTrue(TEXT("Re-activates after expiry"), Manager->ActivateWardBrewBoost());
    TestTrue(TEXT("Active again"), Manager->IsWardBrewBoostActive());

    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistWard_ConcealmentOnlyProtectsASmallRadiusAroundCenter,
    "Herbalist.Ward.ConcealmentOnlyProtectsASmallRadiusAroundCenter",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistWard_ConcealmentOnlyProtectsASmallRadiusAroundCenter::RunTest(const FString& Parameters)
{
    // "Слабая" версия Шапки-невидимки -- та же геометрия (Chebyshev-радиус
    // вокруг клетки активации), но WardConcealmentRadius (черновое число 1)
    // заметно меньше InvisibilityCapRadius (3, см. HerbalistSettings.h).
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;

    TestFalse(TEXT("A cell far from any center is never protected before activation"),
        Manager->IsWardConcealmentActive(FIntPoint(10, 10)));

    Manager->ActivateWardConcealment(FIntPoint(5, 5));   // радиус по умолчанию 1

    TestTrue(TEXT("The activation cell itself is protected"), Manager->IsWardConcealmentActive(FIntPoint(5, 5)));
    TestTrue(TEXT("An adjacent cell (within radius 1) is protected"), Manager->IsWardConcealmentActive(FIntPoint(6, 6)));
    TestFalse(TEXT("A cell two steps away is NOT protected -- weaker than the full Шапка"),
        Manager->IsWardConcealmentActive(FIntPoint(7, 5)));
    TestTrue(TEXT("The general (location-less) check reports active"), Manager->IsWardConcealmentActive());

    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistWard_ConcealmentExpiresLikeBrewBoost,
    "Herbalist.Ward.ConcealmentExpiresLikeBrewBoost",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistWard_ConcealmentExpiresLikeBrewBoost::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;

    Manager->SetGameClockSeconds(2000.0f);
    Manager->ActivateWardConcealment(FIntPoint(2, 2));
    TestTrue(TEXT("Active right after activation"), Manager->IsWardConcealmentActive(FIntPoint(2, 2)));

    Manager->SetGameClockSeconds(2000.0f + 601.0f);
    TestFalse(TEXT("Expires just like BrewBoost, same timer"), Manager->IsWardConcealmentActive(FIntPoint(2, 2)));
    TestFalse(TEXT("General check also reports inactive"), Manager->IsWardConcealmentActive());

    Manager->Destroy();
    return true;
}

// MorokReduction (Куриный бог, второй заход механики оберегов, 2026-09-04)
// — тот же таймер/геометрия, что уже проверены выше у EntityConceal
// (ActivateWardMorokReduction/IsWardMorokReductionActive используют
// идентичный Center+Radius код), поэтому здесь не повторяем таймер/радиус
// с нуля отдельным тестом — проверяем именно то, что отличает этот
// оберег: реальное влияние на ComputePerceptionDistortion (единая точка
// входа восприятия, GridWorldManagerEntities.cpp) и ТОЛЬКО ночью, тот же
// день/ночь переключатель, что уже DayCycleTest.cpp использует для
// IsDawn/IsDusk/IsNight (минута 10 = День, минута 29 = Ночь, 32-минутные
// сутки).
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistWard_MorokReductionOnlyAppliesAtNightNearActivation,
    "Herbalist.Ward.MorokReductionOnlyAppliesAtNightNearActivation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistWard_MorokReductionOnlyAppliesAtNightNearActivation::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;

    // Дневное значение ДО активации оберега вообще -- эталон "как было бы
    // без него", снятый один раз в начале, до всякой активации.
    Manager->SetGameClockSeconds(10.0f * 60.0f);   // День (тот же тайминг, что DayCycleTest.cpp)
    if (!TestFalse(TEXT("Sanity: this instant is not Night"), Manager->IsNight())) { Manager->Destroy(); return false; }
    const float BaselineDayBeforeActivation = Manager->ComputePerceptionDistortion(5, 5);

    // Ночь (минута 29 из 32-минутных суток, тот же тайминг, что уже
    // DayCycleTest.cpp использует для "внутри Ночи").
    Manager->SetGameClockSeconds(29.0f * 60.0f);
    if (!TestTrue(TEXT("Sanity: this instant really is Night"), Manager->IsNight())) { Manager->Destroy(); return false; }

    const float BaselineNight = Manager->ComputePerceptionDistortion(5, 5);
    TestTrue(TEXT("Not active before activation"), !Manager->IsWardMorokReductionActive());

    Manager->ActivateWardMorokReduction(FIntPoint(5, 5));   // радиус по умолчанию 1
    TestTrue(TEXT("Active immediately after activation"), Manager->IsWardMorokReductionActive());

    const float ReducedNight = Manager->ComputePerceptionDistortion(5, 5);
    TestTrue(TEXT("Distortion at the activation cell drops once active, at night"), ReducedNight < BaselineNight);

    const float ReducedNightAdjacent = Manager->ComputePerceptionDistortion(6, 6);
    TestTrue(TEXT("An adjacent cell (within radius 1) is also protected"), ReducedNightAdjacent < BaselineNight);

    const float UnaffectedFarCell = Manager->ComputePerceptionDistortion(7, 5);
    TestEqual(TEXT("A cell two steps away is NOT protected -- same weak radius as EntityConceal"),
        UnaffectedFarCell, BaselineNight);

    // "Защищает конкретно сон" -- днём этот же активный оберег не должен
    // менять восприятие вовсе, в отличие от EntityConceal/BrewBoost,
    // которые не завязаны на время суток. Часы двигаем ЗАДОМ (29 мин ->
    // 10 мин) намеренно -- тест только переключает фазу суток напрямую,
    // не гоняет полный игровой цикл; таймер оберега (WardDurationSeconds
    // от момента активации) от этого не путается, читает GameClockSeconds
    // как есть.
    Manager->SetGameClockSeconds(10.0f * 60.0f);
    if (!TestFalse(TEXT("Sanity: this instant is not Night"), Manager->IsNight())) { Manager->Destroy(); return false; }
    TestTrue(TEXT("Ward is still active by timer (only the day/night gate changed)"), Manager->IsWardMorokReductionActive());

    const float DayWhileActive = Manager->ComputePerceptionDistortion(5, 5);
    TestEqual(TEXT("No reduction during the day, even while the ward timer is active"),
        DayWhileActive, BaselineDayBeforeActivation);

    Manager->Destroy();
    return true;
}

// ---------------------------------------------------------------------------
// Тиражные обереги (награда ритуалов перехода ярусов биомов, 2026-09-04,
// GridWorldManagerWards.cpp::ActivateTieredWard) -- "как активировал/надел
// оберег, так он и работает" (прямой запрос): НЕТ GameClockSeconds-экспаери,
// в отличие от трёх оберегов выше. Продвигаем часы на 100х WardDurationSeconds
// (черновое число 600) и убеждаемся, что тиражный оберег всё ещё активен --
// та самая проверка "не постоянное действие" НЕ применяется к этой тройке.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistWard_TieredWardHasNoExpiry,
    "Herbalist.Ward.TieredWardHasNoExpiry",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistWard_TieredWardHasNoExpiry::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;

    Manager->GetCell(5, 5)->Biome = EBiomeType::Taiga;

    Manager->SetGameClockSeconds(1000.0f);
    Manager->ActivateTieredWard(EWardEffectType::EntityConceal, { EBiomeType::Taiga, EBiomeType::ForestSteppe });
    TestTrue(TEXT("Active immediately after activation (home biome)"), Manager->IsTieredConcealmentActive(FIntPoint(5, 5)));

    // Часы двигаются на 100х WardDurationSeconds (600) -- если бы тиражный
    // оберег по ошибке разделял таймер со старой тройкой, это давно истекло бы.
    Manager->SetGameClockSeconds(1000.0f + 100.0f * 600.0f);
    TestTrue(TEXT("Still active after 100x WardDurationSeconds -- no timer at all"), Manager->IsTieredConcealmentActive(FIntPoint(5, 5)));

    Manager->Destroy();
    return true;
}

// ---------------------------------------------------------------------------
// EntityConceal (тираж) -- полная защита в домашнем биоме клетки, никакой
// защиты вне них (см. довод у AGridWorldManager::IsTieredConcealmentActive:
// геометрия по биому клетки, не Center+Radius от места активации).
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistWard_TieredConcealmentOnlyProtectsHomeBiome,
    "Herbalist.Ward.TieredConcealmentOnlyProtectsHomeBiome",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistWard_TieredConcealmentOnlyProtectsHomeBiome::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;

    Manager->GetCell(5, 5)->Biome = EBiomeType::Taiga;         // домашний биом (Зорин-камень: Taiga+ForestSteppe)
    Manager->GetCell(6, 6)->Biome = EBiomeType::Steppe;        // НЕ домашний

    TestFalse(TEXT("Not active before activation"), Manager->IsTieredConcealmentActive(FIntPoint(5, 5)));

    Manager->ActivateTieredWard(EWardEffectType::EntityConceal, { EBiomeType::Taiga, EBiomeType::ForestSteppe });

    TestTrue(TEXT("Full protection at a cell in a home biome"), Manager->IsTieredConcealmentActive(FIntPoint(5, 5)));
    TestFalse(TEXT("No protection at a cell outside the home biomes"), Manager->IsTieredConcealmentActive(FIntPoint(6, 6)));

    Manager->Destroy();
    return true;
}

// ---------------------------------------------------------------------------
// MorokReduction (тираж) -- полная сила (WardMorokReductionAmount) в домашнем
// биоме клетки, ослабленная (x TieredWardOutOfBiomeStrength) вне них, ночью
// только (тот же гейт, что и у Куриного бога).
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistWard_TieredMorokReductionWeakerOutsideHomeBiome,
    "Herbalist.Ward.TieredMorokReductionWeakerOutsideHomeBiome",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistWard_TieredMorokReductionWeakerOutsideHomeBiome::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;

    // Синь-камень: домашние биомы MixedForest+BroadleafForest.
    Manager->GetCell(5, 5)->Biome = EBiomeType::MixedForest;   // домашний
    Manager->GetCell(6, 6)->Biome = EBiomeType::Tundra;        // НЕ домашний

    TestEqual(TEXT("0.0f before activation (home cell)"), Manager->GetTieredMorokReductionAmount(FIntPoint(5, 5)), 0.0f);

    // Эталон "как было бы без тиражного оберега" -- снят ДО активации, ночью,
    // на обеих клетках (тот же приём, что и MorokReductionOnlyAppliesAtNightNearActivation
    // выше в этом файле).
    Manager->SetGameClockSeconds(29.0f * 60.0f);   // Ночь
    if (!TestTrue(TEXT("Sanity: really Night"), Manager->IsNight())) { Manager->Destroy(); return false; }
    const float BaselineNightHome = Manager->ComputePerceptionDistortion(5, 5);
    const float BaselineNightAway = Manager->ComputePerceptionDistortion(6, 6);

    Manager->ActivateTieredWard(EWardEffectType::MorokReduction, { EBiomeType::MixedForest, EBiomeType::BroadleafForest });

    const float HomeAmount = Manager->GetTieredMorokReductionAmount(FIntPoint(5, 5));
    const float AwayAmount = Manager->GetTieredMorokReductionAmount(FIntPoint(6, 6));

    TestTrue(TEXT("Home biome gives a positive reduction"), HomeAmount > 0.0f);
    TestTrue(TEXT("Away biome gives a smaller, still positive reduction (weakened, not zero)"),
        AwayAmount > 0.0f && AwayAmount < HomeAmount - KINDA_SMALL_NUMBER);

    const float NightDistortionHome = Manager->ComputePerceptionDistortion(5, 5);
    const float NightDistortionAway = Manager->ComputePerceptionDistortion(6, 6);

    TestTrue(TEXT("Home cell: distortion drops once the tiered ward is active, at night"),
        NightDistortionHome < BaselineNightHome - KINDA_SMALL_NUMBER);
    TestTrue(TEXT("Away cell: distortion also drops, but by less than the home cell did"),
        (BaselineNightAway - NightDistortionAway) < (BaselineNightHome - NightDistortionHome) - KINDA_SMALL_NUMBER);

    Manager->Destroy();
    return true;
}

#endif // WITH_AUTOMATION_TESTS && WITH_EDITOR
