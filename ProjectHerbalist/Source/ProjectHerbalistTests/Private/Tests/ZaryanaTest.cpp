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
    Manager->RegisterShrine(FIntPoint(1, 1), EShrineType::Ancestral);

    // Капище на нуле — отклик уходит в минус, но пол (§20.3) не даёт
    // Clarity упасть ниже уже заработанного якоря.
    if (FShrine* Shrine = Manager->FindShrineAt(FIntPoint(1, 1))) { Shrine->Restoration = 0.0f; }
    Manager->RecomputeGlobalPerceptionClarity();
    TestTrue(TEXT("Clarity never drops below the anchor even with a bad response"),
        Manager->GetGlobalPerceptionClarity() >= Manager->GetClarityAnchor() - KINDA_SMALL_NUMBER);

    // Капище на максимуме — отклик положителен и не может увести Clarity
    // выше Anchor + ClarityResponseRange (дефолт 0.2).
    if (FShrine* Shrine = Manager->FindShrineAt(FIntPoint(1, 1))) { Shrine->Restoration = 1.0f; }
    Manager->RecomputeGlobalPerceptionClarity();
    TestTrue(TEXT("Clarity rises above the anchor with a good response"),
        Manager->GetGlobalPerceptionClarity() > Manager->GetClarityAnchor());
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

#endif // WITH_AUTOMATION_TESTS && WITH_EDITOR
