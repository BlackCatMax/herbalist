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

namespace
{
    AGridWorldManager* SpawnAndBeginPlay(UWorld* World)
    {
        if (!World) return nullptr;
        AGridWorldManager* Manager = World->SpawnActor<AGridWorldManager>();
        if (Manager)
        {
            Manager->DispatchBeginPlay();
        }
        return Manager;
    }
}

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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistZaryana_FalseFragmentLowersClarityAndDoesNotMarkCollected,
    "Herbalist.Zaryana.FalseFragmentLowersClarityAndDoesNotMarkCollected",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistZaryana_FalseFragmentLowersClarityAndDoesNotMarkCollected::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;

    const FName ID(TEXT("PERVAYA_VARKA"));
    Manager->SetGlobalPerceptionClarity(0.3f);

    Manager->CollectMemoryFragment(ID, /*bIsFalse=*/true, nullptr);

    TestTrue(TEXT("Clarity dropped after a false fragment"), Manager->GetGlobalPerceptionClarity() < 0.3f);
    TestFalse(TEXT("False collection does not mark the ID collected — a true one can still spawn later"),
        Manager->GetCollectedFragmentIDs().Contains(ID));

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
