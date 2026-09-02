// Source/ProjectHerbalistTests/Private/Tests/ZaryanaTest.cpp
//
// Заряна: фрагменты памяти и Буян (обсуждение в сессии 2026-08-24). Тот же
// DispatchBeginPlay-паттерн, что уже обкатан в SaveSystemTest.cpp/ShrineTest.cpp/
// BistabilityTest.cpp.

#include "Core/World/GridWorldManager.h"
#include "Core/Zaryana/MemoryFragmentDefinitions.h"
#include "Misc/AutomationTest.h"
#include "Editor.h"
#include "Engine/World.h"

#if WITH_AUTOMATION_TESTS && WITH_EDITOR

#include "TestWorldHelpers.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistZaryana_TrueFragmentRaisesClarityAndMarksCollected,
    "Herbalist.Zaryana.TrueFragmentRaisesClarityAndMarksCollected",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistZaryana_TrueFragmentRaisesClarityAndMarksCollected::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;

    const FName ID(TEXT("TIKHOE_MESTO"));
    TestEqual(TEXT("Clarity starts at zero"), Manager->GetGlobalPerceptionClarity(), 0.0f);

    Manager->CollectMemoryFragment(ID, /*bIsFalse=*/false, nullptr);

    TestTrue(TEXT("Clarity rose after a true fragment"), Manager->GetGlobalPerceptionClarity() > 0.0f);
    TestTrue(TEXT("Fragment ID marked collected"), Manager->GetCollectedFragmentIDs().Contains(ID));

    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistZaryana_FalseFragmentDoesNotLowerAnchorAndDoesNotMarkCollected,
    "Herbalist.Zaryana.FalseFragmentDoesNotLowerAnchorAndDoesNotMarkCollected",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistZaryana_FalseFragmentDoesNotLowerAnchorAndDoesNotMarkCollected::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;

    const FName ID(TEXT("PERVAYA_VARKA"));
    Manager->SetClarityAnchor(0.3f);
    Manager->SetGlobalPerceptionClarity(0.3f);

    Manager->CollectMemoryFragment(ID, /*bIsFalse=*/true, nullptr);

    // 2026-09-01, §20.3 "якорь + отклик": прямой штраф Clarity за ложный
    // фрагмент убран сознательно (якорь монотонен по спецификации, отклик
    // и так честно отражает мир) — Anchor и GlobalPerceptionClarity (никем
    // не пересчитанный на этом пути) остаются на месте.
    TestEqual(TEXT("Anchor unaffected by a false fragment"), Manager->GetClarityAnchor(), 0.3f);
    TestEqual(TEXT("Clarity unaffected by a false fragment"), Manager->GetGlobalPerceptionClarity(), 0.3f);
    TestFalse(TEXT("False collection does not mark the ID collected — a true one can still spawn later"),
        Manager->GetCollectedFragmentIDs().Contains(ID));

    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistZaryana_ClarityAnchorNeverDecreasesAcrossFragments,
    "Herbalist.Zaryana.ClarityAnchorNeverDecreasesAcrossFragments",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistZaryana_ClarityAnchorNeverDecreasesAcrossFragments::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;

    TestEqual(TEXT("Anchor starts at zero"), Manager->GetClarityAnchor(), 0.0f);

    Manager->CollectMemoryFragment(FName(TEXT("TIKHOE_MESTO")), /*bIsFalse=*/false, nullptr);
    const float AnchorAfterFirst = Manager->GetClarityAnchor();
    TestTrue(TEXT("Anchor rose after first true fragment"), AnchorAfterFirst > 0.0f);

    // Ложный фрагмент между двумя подлинными — не должен откатить якорь.
    Manager->CollectMemoryFragment(FName(TEXT("PODNOSHENIE")), /*bIsFalse=*/true, nullptr);
    TestEqual(TEXT("Anchor unchanged by an intervening false fragment"), Manager->GetClarityAnchor(), AnchorAfterFirst);

    Manager->CollectMemoryFragment(FName(TEXT("PODNOSHENIE")), /*bIsFalse=*/false, nullptr);
    TestTrue(TEXT("Anchor rose again after second true fragment"), Manager->GetClarityAnchor() > AnchorAfterFirst);

    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistZaryana_ClarityResponseClampedAndFlooredByAnchor,
    "Herbalist.Zaryana.ClarityResponseClampedAndFlooredByAnchor",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistZaryana_ClarityResponseClampedAndFlooredByAnchor::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;

    Manager->SetClarityAnchor(0.4f);
    // Изолируем от сеянных тестовых "хозяев" (SeedTestLandmarks -- их
    // Respect=0 по умолчанию тоже входит в среднее AvgRestorationRespect,
    // иначе один капище на максимуме не поднимет среднее к 1.0 среди
    // дюжины нулевых хозяев) -- то же изолирующее решение, что уже не раз
    // применялось в этом файле для State-триггеров фрагментов.
    Manager->SetEntityLandmarks({});
    Manager->RegisterShrine(FIntPoint(1, 1), EShrineType::Ancestral);

    // Капище на нуле — отклик уходит в минус, но пол (§20.3) не даёт
    // Clarity упасть ниже уже заработанного якоря. Гарантия структурная
    // (Max()), верна даже до всякого сглаживания — один вызов достаточен.
    if (FShrine* Shrine = Manager->FindShrineAt(FIntPoint(1, 1))) { Shrine->Restoration = 0.0f; }
    Manager->RecomputeGlobalPerceptionClarity();
    TestTrue(TEXT("Clarity never drops below the anchor even with a bad response"),
        Manager->GetGlobalPerceptionClarity() >= Manager->GetClarityAnchor() - KINDA_SMALL_NUMBER);

    // Капище на максимуме — отклик положителен, но теперь СГЛАЖЕН
    // (§20.3, 2026-09-02): один опрос не должен заметно сдвинуть Clarity.
    if (FShrine* Shrine = Manager->FindShrineAt(FIntPoint(1, 1))) { Shrine->Restoration = 1.0f; }
    Manager->RecomputeGlobalPerceptionClarity();
    TestTrue(TEXT("A single recompute after a swing barely moves Clarity (smoothed, not instant)"),
        Manager->GetGlobalPerceptionClarity() < Manager->GetClarityAnchor() + 0.01f);
    TestTrue(TEXT("...but it does move, even if slightly"),
        Manager->GetGlobalPerceptionClarity() > Manager->GetClarityAnchor());

    // Много опросов подряд (тот же неизменный хороший мир) -- отклик
    // сходится к полному Anchor + ClarityResponseRange (дефолт 0.2), не
    // превышая его.
    for (int32 i = 0; i < 5000; ++i)
    {
        Manager->RecomputeGlobalPerceptionClarity();
    }
    TestTrue(TEXT("Clarity converges close to Anchor + ClarityResponseRange after many recomputes"),
        Manager->GetGlobalPerceptionClarity() > Manager->GetClarityAnchor() + 0.19f);
    TestTrue(TEXT("Clarity does not exceed Anchor + ClarityResponseRange"),
        Manager->GetGlobalPerceptionClarity() <= Manager->GetClarityAnchor() + 0.2f + KINDA_SMALL_NUMBER);

    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistZaryana_BuyanRequiresBothWorldStateAndShrines,
    "Herbalist.Zaryana.BuyanRequiresBothWorldStateAndShrines",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistZaryana_BuyanRequiresBothWorldStateAndShrines::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;

    // Капище с низкой Restoration — мир не готов, даже если State идеален.
    Manager->RegisterShrine(FIntPoint(0, 0), EShrineType::Ancestral);
    if (FShrine* Shrine = Manager->FindShrineAt(FIntPoint(0, 0)))
    {
        Shrine->Restoration = 0.1f;
    }

    // Одна идеальная клетка на всю сетку почти не двигает среднее —
    // остальные клетки на дефолтных (не-S0) значениях.
    if (FGridCell* Cell = Manager->GetCell(0, 0))
    {
        Cell->State = FAlatyr::S0;
    }

    Manager->CheckBuyanCondition();
    TestFalse(TEXT("Buyan not reached with a low shrine even if one cell is ideal"), Manager->IsBuyanReached());

    if (FShrine* Shrine = Manager->FindShrineAt(FIntPoint(0, 0)))
    {
        Shrine->Restoration = 0.9f;
    }
    // Всё ещё не должно сработать — большинство клеток далеки от S0 (дефолт).
    Manager->CheckBuyanCondition();
    TestFalse(TEXT("Buyan still not reached — most cells are not near S0"), Manager->IsBuyanReached());

    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistZaryana_RosaFallsBackToS0WhenCellUnset,
    "Herbalist.Zaryana.RosaFallsBackToS0WhenCellUnset",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistZaryana_RosaFallsBackToS0WhenCellUnset::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;

    // ZaryanaCell по умолчанию (-1,-1) — не размещена ни левел-дизайнером,
    // ни AlchemyTableActor::BeginPlay (в тестовом мире стола нет).
    FRandomStream Rng(1);
    const FRealState Perceived = Manager->GetZaryanaPerceivedState(Rng);
    TestEqual(TEXT("Falls back to S0 without a placed ZaryanaCell"), Perceived.Meta.Purity, FAlatyr::S0.Meta.Purity);

    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistZaryana_RosaLayer1MatchesCellStateAtFullClarity,
    "Herbalist.Zaryana.RosaLayer1MatchesCellStateAtFullClarity",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistZaryana_RosaLayer1MatchesCellStateAtFullClarity::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;

    Manager->SetZaryanaCellIfUnset(FIntPoint(0, 0));
    if (FGridCell* Cell = Manager->GetCell(0, 0))
    {
        Cell->State.Meta.Purity = 0.8f;
        Cell->State.Meta.Corruption = 0.1f;
    }
    // Clarity=1 гасит шум формулы PerceiveRealState (NoiseScale = Distortion
    // * (1-Clarity) = 0) — та же гарантия, что уже проверяет PerceptionServiceTest.cpp.
    // Никаких капищ/хозяев рядом не зарегистрировано — Слой 3 не добавляет
    // отдалённого влияния, чтение обязано точно совпасть со Слоем 1.
    Manager->SetGlobalPerceptionClarity(1.0f);

    FRandomStream Rng(1);
    const FRealState Perceived = Manager->GetZaryanaPerceivedState(Rng);
    TestEqual(TEXT("Layer 1 Purity passes through exactly at full Clarity, no nearby shrines"), Perceived.Meta.Purity, 0.8f);
    TestEqual(TEXT("Layer 1 Corruption passes through exactly at full Clarity, no nearby shrines"), Perceived.Meta.Corruption, 0.1f);

    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistZaryana_RosaSensingRadiusGrowsWithClarity,
    "Herbalist.Zaryana.RosaSensingRadiusGrowsWithClarity",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistZaryana_RosaSensingRadiusGrowsWithClarity::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;

    Manager->SetZaryanaCellIfUnset(FIntPoint(0, 0));

    // Капище далеко за пределами базового радиуса (дефолт 3), но внутри
    // радиуса при высокой Clarity (дефолт 3 + 1.0*15 = 18) — §19.2 Слой 3.
    Manager->RegisterShrine(FIntPoint(10, 0), EShrineType::Ancestral);
    if (FShrine* Shrine = Manager->FindShrineAt(FIntPoint(10, 0)))
    {
        Shrine->Restoration = 1.0f;
    }

    Manager->SetGlobalPerceptionClarity(0.0f);
    FRandomStream RngLow(1);
    const FRealState PerceivedLowClarity = Manager->GetZaryanaPerceivedState(RngLow);

    Manager->SetGlobalPerceptionClarity(1.0f);
    FRandomStream RngHigh(1);
    const FRealState PerceivedHighClarity = Manager->GetZaryanaPerceivedState(RngHigh);

    TestTrue(TEXT("A distant restored shrine only lightens the reading once Clarity widens the radius"),
        PerceivedHighClarity.Meta.Purity > PerceivedLowClarity.Meta.Purity);

    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistZaryana_RosaFirstUntouchedDriftIsFlaggedOnceAsCoincidence,
    "Herbalist.Zaryana.RosaFirstUntouchedDriftIsFlaggedOnceAsCoincidence",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistZaryana_RosaFirstUntouchedDriftIsFlaggedOnceAsCoincidence::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;

    Manager->SetZaryanaCellIfUnset(FIntPoint(0, 0));

    // Первый опрос — только фиксирует точку отсчёта, флаг ещё не может сработать.
    Manager->UpdateRosaSignal();
    TestFalse(TEXT("First poll only captures the baseline"), Manager->IsRosaFirstFalseSignalShown());

    // Мир вокруг Заряны меняется сам (тот же эффект, что релаксация/биом-граф
    // дают непрерывно) — без прямого применения зелья на её клетку.
    if (FGridCell* Cell = Manager->GetCell(0, 0))
    {
        Cell->State.Meta.Purity = FMath::Clamp(Cell->State.Meta.Purity + 0.5f, 0.0f, 1.0f);
    }

    Manager->UpdateRosaSignal();
    TestTrue(TEXT("Untouched drift flags the one-time false signal"), Manager->IsRosaFirstFalseSignalShown());

    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistZaryana_KhlebSolSpawnsOnHighMolvaThreshold,
    "Herbalist.Zaryana.KhlebSolSpawnsOnHighMolvaThreshold",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistZaryana_KhlebSolSpawnsOnHighMolvaThreshold::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;

    Manager->SetZaryanaCellIfUnset(FIntPoint(0, 0));
    // Изолируем ветку ХЛЕБ-СОЛЬ от всех остальных State-триггеров — иначе
    // дефолтное состояние тестовой сетки могло бы спавнить не тот фрагмент
    // первым же вызовом (найдено 2026-09-01: клетки Тайги по умолчанию
    // держат Distortion=0, ниже порога TISHINA_LESA/TIKHOE_MESTO тривиально
    // с самого начала).
    Manager->SetCollectedFragmentIDs({FName(TEXT("TIKHOE_MESTO")), FName(TEXT("PODNOSHENIE")),
        FName(TEXT("TISHINA_LESA")), FName(TEXT("OJIDANIE_BURI")), FName(TEXT("NE_POKHVALILA")),
        FName(TEXT("NEUDOBNAYA_PRAVDA"))});

    Manager->Molva = 0.3f;
    Manager->TrySpawnStateBasedFragment();
    TestEqual(TEXT("No fragment below the Molva threshold"), Manager->GetActiveFragmentDefinitionID(), FName(NAME_None));

    // "Устойчиво высокая Молва" (§17.6, 2026-09-02) -- теперь выдержано, не
    // мгновенный порог: KhlebSolSustainedSeconds (дефолт 30с) / CheckInterval
    // (дефолт 5с) = 6 опросов подряд, не один.
    Manager->Molva = 0.8f;
    for (int32 i = 0; i < 5; ++i)
    {
        Manager->TrySpawnStateBasedFragment();
        TestEqual(TEXT("Not sustained long enough yet -- no spawn"), Manager->GetActiveFragmentDefinitionID(), FName(NAME_None));
    }
    Manager->TrySpawnStateBasedFragment();
    TestEqual(TEXT("KHLEB_SOL spawns once Molva has stayed high for the full sustained window"), Manager->GetActiveFragmentDefinitionID(), FName(TEXT("KHLEB_SOL")));

    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistZaryana_KhlebSolResetsSustainedAccumulatorOnDrop,
    "Herbalist.Zaryana.KhlebSolResetsSustainedAccumulatorOnDrop",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistZaryana_KhlebSolResetsSustainedAccumulatorOnDrop::RunTest(const FString& Parameters)
{
    // Один провал условия между опросами обязан сбросить накопление целиком
    // (§17.6 "устойчиво" -- не "почти всё время"), не просто затормозить его.
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;

    Manager->SetZaryanaCellIfUnset(FIntPoint(0, 0));
    Manager->SetCollectedFragmentIDs({FName(TEXT("TIKHOE_MESTO")), FName(TEXT("PODNOSHENIE")),
        FName(TEXT("TISHINA_LESA")), FName(TEXT("OJIDANIE_BURI")), FName(TEXT("NE_POKHVALILA")),
        FName(TEXT("NEUDOBNAYA_PRAVDA"))});

    Manager->Molva = 0.8f;
    for (int32 i = 0; i < 5; ++i)
    {
        Manager->TrySpawnStateBasedFragment();   // 5/6 opросов -- почти выдержано
    }
    TestEqual(TEXT("Not spawned yet after 5 of 6 sustained polls"), Manager->GetActiveFragmentDefinitionID(), FName(NAME_None));

    Manager->Molva = 0.1f;   // условие сорвалось на один опрос
    Manager->TrySpawnStateBasedFragment();
    Manager->Molva = 0.8f;   // условие снова держится
    for (int32 i = 0; i < 5; ++i)
    {
        Manager->TrySpawnStateBasedFragment();
        TestEqual(TEXT("Accumulator restarted from zero after the drop -- still not enough"), Manager->GetActiveFragmentDefinitionID(), FName(NAME_None));
    }
    Manager->TrySpawnStateBasedFragment();
    TestEqual(TEXT("Spawns only after a full fresh sustained window since the drop"), Manager->GetActiveFragmentDefinitionID(), FName(TEXT("KHLEB_SOL")));

    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistZaryana_BuyanPathRequiresBuyanReachedAndOnlyChoosesOnce,
    "Herbalist.Zaryana.BuyanPathRequiresBuyanReachedAndOnlyChoosesOnce",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistZaryana_BuyanPathRequiresBuyanReachedAndOnlyChoosesOnce::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;

    // Буян ещё не достигнут — путь 3 (без порога Clarity/Молвы) всё равно недоступен.
    TestFalse(TEXT("No path before Buyan is reached"), Manager->TryChooseBuyanPath(EBuyanPath::AcceptReality));
    TestEqual(TEXT("ChosenBuyanPath stays None"), (uint8)Manager->GetChosenBuyanPath(), (uint8)EBuyanPath::None);

    Manager->SetBuyanReached(true);
    TestTrue(TEXT("Path 3 available once Buyan is reached, no threshold"), Manager->TryChooseBuyanPath(EBuyanPath::AcceptReality));
    TestEqual(TEXT("ChosenBuyanPath recorded"), (uint8)Manager->GetChosenBuyanPath(), (uint8)EBuyanPath::AcceptReality);
    TestTrue(TEXT("A successful choice delivers the matching guaranteed ending fragment"),
        Manager->GetCollectedFragmentIDs().Contains(FName(TEXT("BUYAN_ACCEPT_REALITY"))));

    // Не переигрывается — второй выбор (даже другого пути) не проходит.
    TestFalse(TEXT("Cannot choose a second path once one is chosen"), Manager->TryChooseBuyanPath(EBuyanPath::TradePlaces));
    TestEqual(TEXT("ChosenBuyanPath unchanged"), (uint8)Manager->GetChosenBuyanPath(), (uint8)EBuyanPath::AcceptReality);

    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistZaryana_BuyanGuardianPathGatedByClarityAndMolva,
    "Herbalist.Zaryana.BuyanGuardianPathGatedByClarityAndMolva",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistZaryana_BuyanGuardianPathGatedByClarityAndMolva::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;

    Manager->SetBuyanReached(true);

    // Ни Clarity, ни Молва не заданы (дефолт 0) — путь 1 недоступен.
    TestFalse(TEXT("Guardian path unavailable with low Clarity/Molva"), Manager->TryChooseBuyanPath(EBuyanPath::Guardian));
    TestEqual(TEXT("ChosenBuyanPath still None"), (uint8)Manager->GetChosenBuyanPath(), (uint8)EBuyanPath::None);

    // Высокая Clarity, но Молва по-прежнему низкая (дефолт 0) — всё ещё недоступен.
    Manager->SetGlobalPerceptionClarity(1.0f);
    TestFalse(TEXT("Guardian path unavailable with high Clarity alone"), Manager->TryChooseBuyanPath(EBuyanPath::Guardian));

    // Оба порога пройдены — путь 1 доступен.
    Manager->Molva = 1.0f;
    TestTrue(TEXT("Guardian path available with both thresholds met"), Manager->TryChooseBuyanPath(EBuyanPath::Guardian));
    TestEqual(TEXT("ChosenBuyanPath recorded as Guardian"), (uint8)Manager->GetChosenBuyanPath(), (uint8)EBuyanPath::Guardian);

    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistZaryana_TishinaLesaSpawnsOnlyInTaiga,
    "Herbalist.Zaryana.TishinaLesaSpawnsOnlyInTaiga",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistZaryana_TishinaLesaSpawnsOnlyInTaiga::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;

    // Якорь Индрик-зверя гарантированно стоит на клетке биома Тайга (тот же
    // приём, что уже LegendaryEntityTest.cpp/ArtifactTest.cpp).
    const FIntPoint* TaigaAnchor = Manager->GetLegendaryAnchors().Find(FName(TEXT("Индрик-зверь")));
    if (!TestNotNull(TEXT("Taiga cell available via Индрик-зверь anchor"), TaigaAnchor))
    {
        Manager->Destroy();
        return false;
    }

    // Изолируем от всех остальных State-триггеров -- тот же класс
    // контаминации, что уже пойман у KhlebSolSpawnsOnHighMolvaThreshold.
    Manager->SetCollectedFragmentIDs({FName(TEXT("TIKHOE_MESTO")), FName(TEXT("PODNOSHENIE")),
        FName(TEXT("KHLEB_SOL")), FName(TEXT("OJIDANIE_BURI")), FName(TEXT("NE_POKHVALILA")),
        FName(TEXT("NEUDOBNAYA_PRAVDA"))});

    // Все прочие клетки Тайги по умолчанию тоже держат Distortion=0
    // (найдено ранее в этом файле, "KhlebSolSpawnsOnHighMolvaThreshold") --
    // без этого их аккумуляторы копились бы параллельно с якорной клеткой
    // и случайный выбор мог унести спавн на любую из них. Форсируем ИХ
    // высокий Distortion, чтобы кандидатом на TISHINA_LESA была только
    // клетка-якорь.
    Manager->ForEachCell([TaigaAnchor](FGridCell& C)
    {
        if (C.Biome == EBiomeType::Taiga && FIntPoint(C.X, C.Y) != *TaigaAnchor)
        {
            C.State.Meta.Distortion = 0.9f;
        }
    });

    if (FGridCell* Cell = Manager->GetCell(TaigaAnchor->X, TaigaAnchor->Y))
    {
        Cell->bIsWater = false;
        Cell->State.Meta.Distortion = 0.0f;
    }

    // "Длительная низкая Distortion" (§17.7, 2026-09-02) -- выдержано, не
    // мгновенно: TishinaLesaSustainedSeconds (дефолт 60с) / CheckInterval
    // (дефолт 5с) = 12 опросов подряд на ЭТОЙ ЖЕ клетке.
    for (int32 i = 0; i < 11; ++i)
    {
        Manager->TrySpawnStateBasedFragment();
        TestEqual(TEXT("Not sustained long enough yet -- no spawn"), Manager->GetActiveFragmentDefinitionID(), FName(NAME_None));
    }
    Manager->TrySpawnStateBasedFragment();
    TestEqual(TEXT("TISHINA_LESA spawns once the Taiga cell has held low Distortion for the full window"),
        Manager->GetActiveFragmentDefinitionID(), FName(TEXT("TISHINA_LESA")));

    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistZaryana_TishinaLesaResetsPerCellAccumulatorOnDrop,
    "Herbalist.Zaryana.TishinaLesaResetsPerCellAccumulatorOnDrop",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistZaryana_TishinaLesaResetsPerCellAccumulatorOnDrop::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;

    const FIntPoint* TaigaAnchor = Manager->GetLegendaryAnchors().Find(FName(TEXT("Индрик-зверь")));
    if (!TestNotNull(TEXT("Taiga cell available via Индрик-зверь anchor"), TaigaAnchor))
    {
        Manager->Destroy();
        return false;
    }
    Manager->SetCollectedFragmentIDs({FName(TEXT("TIKHOE_MESTO")), FName(TEXT("PODNOSHENIE")),
        FName(TEXT("KHLEB_SOL")), FName(TEXT("OJIDANIE_BURI")), FName(TEXT("NE_POKHVALILA")),
        FName(TEXT("NEUDOBNAYA_PRAVDA"))});

    // Изолируем от прочих клеток Тайги (по умолчанию тоже Distortion=0) --
    // тот же приём, что уже в TishinaLesaSpawnsOnlyInTaiga выше, иначе их
    // независимо накапливающиеся аккумуляторы маскируют сброс именно на
    // клетке-якоре случайным выбором среди нескольких готовых клеток.
    Manager->ForEachCell([TaigaAnchor](FGridCell& C)
    {
        if (C.Biome == EBiomeType::Taiga && FIntPoint(C.X, C.Y) != *TaigaAnchor)
        {
            C.State.Meta.Distortion = 0.9f;
        }
    });

    FGridCell* Cell = Manager->GetCell(TaigaAnchor->X, TaigaAnchor->Y);
    if (!TestNotNull(TEXT("Cell exists"), Cell)) { Manager->Destroy(); return false; }
    Cell->bIsWater = false;
    Cell->State.Meta.Distortion = 0.0f;

    for (int32 i = 0; i < 11; ++i)
    {
        Manager->TrySpawnStateBasedFragment();   // 11/12 -- почти выдержано
    }
    TestEqual(TEXT("Not spawned yet after 11 of 12 sustained polls"), Manager->GetActiveFragmentDefinitionID(), FName(NAME_None));

    Cell->State.Meta.Distortion = 0.9f;   // условие сорвалось на этой клетке
    Manager->TrySpawnStateBasedFragment();
    Cell->State.Meta.Distortion = 0.0f;   // условие снова держится

    for (int32 i = 0; i < 11; ++i)
    {
        Manager->TrySpawnStateBasedFragment();
        TestEqual(TEXT("Per-cell accumulator restarted from zero after the drop"), Manager->GetActiveFragmentDefinitionID(), FName(NAME_None));
    }
    Manager->TrySpawnStateBasedFragment();
    TestEqual(TEXT("Spawns only after a full fresh sustained window on this cell since the drop"),
        Manager->GetActiveFragmentDefinitionID(), FName(TEXT("TISHINA_LESA")));

    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistZaryana_OjidanieBuriNeedsHighStabilityInTundra,
    "Herbalist.Zaryana.OjidanieBuriNeedsHighStabilityInTundra",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistZaryana_OjidanieBuriNeedsHighStabilityInTundra::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;

    const FIntPoint* TundraAnchor = Manager->GetLegendaryAnchors().Find(FName(TEXT("Волот")));
    if (!TestNotNull(TEXT("Tundra cell available via Волот anchor"), TundraAnchor))
    {
        Manager->Destroy();
        return false;
    }

    // Изолируем от всех остальных State-триггеров, не только TIKHOE_MESTO --
    // клетки Тайги по умолчанию держат Distortion=0, ниже порога
    // TISHINA_LESA тривиально с самого начала (найдено 2026-09-01).
    Manager->SetCollectedFragmentIDs({FName(TEXT("TIKHOE_MESTO")), FName(TEXT("PODNOSHENIE")),
        FName(TEXT("KHLEB_SOL")), FName(TEXT("TISHINA_LESA")), FName(TEXT("NE_POKHVALILA")),
        FName(TEXT("NEUDOBNAYA_PRAVDA"))});

    if (FGridCell* Cell = Manager->GetCell(TundraAnchor->X, TundraAnchor->Y))
    {
        Cell->State.Meta.Stability = 0.3f;
    }
    Manager->TrySpawnStateBasedFragment();
    TestNotEqual(TEXT("Low Stability -- does not spawn OJIDANIE_BURI"),
        Manager->GetActiveFragmentDefinitionID(), FName(TEXT("OJIDANIE_BURI")));

    // "Удержанной долго" (§17.7, 2026-09-02) -- выдержано, не мгновенно:
    // OjidanieBuriSustainedSeconds (дефолт 120с) / CheckInterval (дефолт 5с)
    // = 24 опроса подряд на этой же клетке.
    if (FGridCell* Cell = Manager->GetCell(TundraAnchor->X, TundraAnchor->Y))
    {
        Cell->State.Meta.Stability = 0.9f;
    }
    for (int32 i = 0; i < 23; ++i)
    {
        Manager->TrySpawnStateBasedFragment();
        TestNotEqual(TEXT("Not sustained long enough yet -- no OJIDANIE_BURI"), Manager->GetActiveFragmentDefinitionID(), FName(TEXT("OJIDANIE_BURI")));
    }
    Manager->TrySpawnStateBasedFragment();
    TestEqual(TEXT("High Stability held for the full sustained window -- spawns OJIDANIE_BURI"),
        Manager->GetActiveFragmentDefinitionID(), FName(TEXT("OJIDANIE_BURI")));

    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistZaryana_OjidanieBuriResetsPerCellAccumulatorOnDrop,
    "Herbalist.Zaryana.OjidanieBuriResetsPerCellAccumulatorOnDrop",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistZaryana_OjidanieBuriResetsPerCellAccumulatorOnDrop::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;

    const FIntPoint* TundraAnchor = Manager->GetLegendaryAnchors().Find(FName(TEXT("Волот")));
    if (!TestNotNull(TEXT("Tundra cell available via Волот anchor"), TundraAnchor))
    {
        Manager->Destroy();
        return false;
    }
    Manager->SetCollectedFragmentIDs({FName(TEXT("TIKHOE_MESTO")), FName(TEXT("PODNOSHENIE")),
        FName(TEXT("KHLEB_SOL")), FName(TEXT("TISHINA_LESA")), FName(TEXT("NE_POKHVALILA")),
        FName(TEXT("NEUDOBNAYA_PRAVDA"))});

    // Тот же изолирующий приём, что уже TishinaLesaResetsPerCellAccumulatorOnDrop --
    // защитно, даже если default-Stability Тундры сейчас ниже порога и без
    // этого (эмпирически подтверждено в OjidanieBuriNeedsHighStabilityInTundra
    // выше), не полагаемся на это молча.
    Manager->ForEachCell([TundraAnchor](FGridCell& C)
    {
        if (C.Biome == EBiomeType::Tundra && FIntPoint(C.X, C.Y) != *TundraAnchor)
        {
            C.State.Meta.Stability = 0.1f;
        }
    });

    FGridCell* Cell = Manager->GetCell(TundraAnchor->X, TundraAnchor->Y);
    if (!TestNotNull(TEXT("Cell exists"), Cell)) { Manager->Destroy(); return false; }
    Cell->State.Meta.Stability = 0.9f;

    for (int32 i = 0; i < 23; ++i)
    {
        Manager->TrySpawnStateBasedFragment();   // 23/24 -- почти выдержано
    }
    TestNotEqual(TEXT("Not spawned yet after 23 of 24 sustained polls"), Manager->GetActiveFragmentDefinitionID(), FName(TEXT("OJIDANIE_BURI")));

    Cell->State.Meta.Stability = 0.1f;   // условие сорвалось на этой клетке
    Manager->TrySpawnStateBasedFragment();
    Cell->State.Meta.Stability = 0.9f;   // условие снова держится

    for (int32 i = 0; i < 23; ++i)
    {
        Manager->TrySpawnStateBasedFragment();
        TestNotEqual(TEXT("Per-cell accumulator restarted from zero after the drop"), Manager->GetActiveFragmentDefinitionID(), FName(TEXT("OJIDANIE_BURI")));
    }
    Manager->TrySpawnStateBasedFragment();
    TestEqual(TEXT("Spawns only after a full fresh sustained window on this cell since the drop"),
        Manager->GetActiveFragmentDefinitionID(), FName(TEXT("OJIDANIE_BURI")));

    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistZaryana_NePokhvalilaNeedsBabaYagaManifested,
    "Herbalist.Zaryana.NePokhvalilaNeedsBabaYagaManifested",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistZaryana_NePokhvalilaNeedsBabaYagaManifested::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;

    const FIntPoint* Anchor = Manager->GetLegendaryAnchors().Find(FName(TEXT("Баба-Яга")));
    if (!TestNotNull(TEXT("Баба-Яга has a seeded anchor cell"), Anchor))
    {
        Manager->Destroy();
        return false;
    }

    // Изолируем от всех остальных State-триггеров -- та же контаминация,
    // что уже пойман у KhlebSolSpawnsOnHighMolvaThreshold.
    Manager->SetCollectedFragmentIDs({FName(TEXT("TIKHOE_MESTO")), FName(TEXT("PODNOSHENIE")),
        FName(TEXT("KHLEB_SOL")), FName(TEXT("TISHINA_LESA")), FName(TEXT("OJIDANIE_BURI")),
        FName(TEXT("NEUDOBNAYA_PRAVDA"))});

    Manager->TrySpawnStateBasedFragment();
    TestNotEqual(TEXT("Not manifested -- does not spawn NE_POKHVALILA"),
        Manager->GetActiveFragmentDefinitionID(), FName(TEXT("NE_POKHVALILA")));

    if (FGridCell* Cell = Manager->GetCell(Anchor->X, Anchor->Y))
    {
        Cell->ManifestedEntityID = FName(TEXT("Баба-Яга"));
    }
    Manager->TrySpawnStateBasedFragment();
    TestEqual(TEXT("Баба-Яга manifested -- spawns NE_POKHVALILA"),
        Manager->GetActiveFragmentDefinitionID(), FName(TEXT("NE_POKHVALILA")));

    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistZaryana_NeudobnayaPravdaNeedsShrineInBroadleafForest,
    "Herbalist.Zaryana.NeudobnayaPravdaNeedsShrineInBroadleafForest",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistZaryana_NeudobnayaPravdaNeedsShrineInBroadleafForest::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;

    // Якорь Дуб-старца гарантированно на клетке биома Широколиственный лес.
    const FIntPoint* Anchor = Manager->GetLegendaryAnchors().Find(FName(TEXT("Дуб-старец")));
    if (!TestNotNull(TEXT("BroadleafForest cell available via Дуб-старец anchor"), Anchor))
    {
        Manager->Destroy();
        return false;
    }

    // Изолируем от ПОДНОШЕНИЕ (то же капище иначе отдаст его первым) и от
    // остальных State-триггеров -- та же контаминация, что уже пойман у
    // KhlebSolSpawnsOnHighMolvaThreshold.
    Manager->SetCollectedFragmentIDs({FName(TEXT("PODNOSHENIE")), FName(TEXT("TIKHOE_MESTO")),
        FName(TEXT("KHLEB_SOL")), FName(TEXT("TISHINA_LESA")), FName(TEXT("OJIDANIE_BURI")),
        FName(TEXT("NE_POKHVALILA"))});

    Manager->RegisterShrine(*Anchor, EShrineType::Ancestral);
    if (FShrine* Shrine = Manager->FindShrineAt(*Anchor))
    {
        Shrine->Restoration = 0.9f;
    }

    Manager->TrySpawnStateBasedFragment();
    TestEqual(TEXT("Restored shrine in BroadleafForest -- spawns NEUDOBNAYA_PRAVDA"),
        Manager->GetActiveFragmentDefinitionID(), FName(TEXT("NEUDOBNAYA_PRAVDA")));

    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistZaryana_NitMateriDeliveredOnYarnBallAcquisition,
    "Herbalist.Zaryana.NitMateriDeliveredOnYarnBallAcquisition",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistZaryana_NitMateriDeliveredOnYarnBallAcquisition::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;

    const FName LegendaryID(TEXT("Мать-Сыра-Земля"));
    const FIntPoint* Anchor = Manager->GetLegendaryAnchors().Find(LegendaryID);
    if (!TestNotNull(TEXT("Мать-Сыра-Земля has a seeded anchor cell"), Anchor))
    {
        Manager->Destroy();
        return false;
    }
    if (FGridCell* Cell = Manager->GetCell(Anchor->X, Anchor->Y))
    {
        Cell->ManifestedEntityID = LegendaryID;
    }

    TestFalse(TEXT("Not collected before Клубочек is acquired"),
        Manager->GetCollectedFragmentIDs().Contains(FName(TEXT("NIT_MATERI"))));

    FInventoryItem Offering;
    Offering.IngredientID = FName(TEXT("TestOffering"));
    Offering.State.Meta.Purity = 0.9f;
    Offering.Count = 1;
    bool bViaDeception = true;
    const bool bAcquired = Manager->TryAcquireArtifact(FName(TEXT("Клубочек")), { Offering }, bViaDeception);
    TestTrue(TEXT("Клубочек acquired"), bAcquired);
    TestTrue(TEXT("NIT_MATERI delivered directly on acquisition"),
        Manager->GetCollectedFragmentIDs().Contains(FName(TEXT("NIT_MATERI"))));

    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistZaryana_FirstPlacementSeedsCorruptedCircleFallingOffWithDistance,
    "Herbalist.Zaryana.FirstPlacementSeedsCorruptedCircleFallingOffWithDistance",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistZaryana_FirstPlacementSeedsCorruptedCircleFallingOffWithDistance::RunTest(const FString& Parameters)
{
    // §19.4a: "трава полегла... кругом... облако Морока" -- первое
    // размещение ZaryanaCell должно оставить видимый, спадающий к краям
    // шрам (Distortion/Corruption), не мгновенно чистую клетку.
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;

    const FIntPoint Center(10, 10);
    const float CenterDistortionBefore = Manager->GetCellConst(Center.X, Center.Y)->State.Meta.Distortion;
    const float EdgeDistortionBefore = Manager->GetCellConst(Center.X + 3, Center.Y)->State.Meta.Distortion;
    const float FarDistortionBefore = Manager->GetCellConst(Center.X + 4, Center.Y)->State.Meta.Distortion;

    Manager->SetZaryanaCellIfUnset(Center);

    const float CenterDistortionAfter = Manager->GetCellConst(Center.X, Center.Y)->State.Meta.Distortion;
    const float EdgeDistortionAfter = Manager->GetCellConst(Center.X + 3, Center.Y)->State.Meta.Distortion;
    const float FarDistortionAfter = Manager->GetCellConst(Center.X + 4, Center.Y)->State.Meta.Distortion;

    TestTrue(TEXT("Center cell gains a large Distortion bump (default radius 3, peak 0.5)"),
        CenterDistortionAfter - CenterDistortionBefore > 0.4f);
    TestTrue(TEXT("Edge of the circle (radius 3) gains a much smaller bump than the center"),
        (EdgeDistortionAfter - EdgeDistortionBefore) < (CenterDistortionAfter - CenterDistortionBefore));
    TestEqual(TEXT("Cell just outside the radius is untouched"), FarDistortionAfter, FarDistortionBefore);

    // Idempotent -- вторая расстановка (клетка уже занята первым вызовом)
    // не переигрывает круг заново в другом месте.
    const FIntPoint Other(0, 0);
    const float OtherDistortionBefore = Manager->GetCellConst(Other.X, Other.Y)->State.Meta.Distortion;
    Manager->SetZaryanaCellIfUnset(Other);
    const float OtherDistortionAfter = Manager->GetCellConst(Other.X, Other.Y)->State.Meta.Distortion;
    TestEqual(TEXT("Second SetZaryanaCellIfUnset call does not re-seed a circle elsewhere"),
        OtherDistortionAfter, OtherDistortionBefore);

    Manager->Destroy();
    return true;
}

#endif // WITH_AUTOMATION_TESTS && WITH_EDITOR
