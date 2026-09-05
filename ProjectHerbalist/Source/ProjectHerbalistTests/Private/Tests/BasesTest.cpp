// Source/ProjectHerbalistTests/Private/Tests/BasesTest.cpp
//
// Базы/лагеря (21_Journey_And_Artifacts.md §21.2, 2026-09-01). Тот же
// DispatchBeginPlay-паттерн, что уже обкатан в ShrineTest.cpp/ZaryanaTest.cpp.

#include "Core/World/GridWorldManager.h"
#include "Misc/AutomationTest.h"
#include "Editor.h"
#include "Engine/World.h"

#if WITH_AUTOMATION_TESTS && WITH_EDITOR

#include "TestWorldHelpers.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistBases_RegisterBaseAddsOnceAndRejectsWater,
    "Herbalist.Bases.RegisterBaseAddsOnceAndRejectsWater",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistBases_RegisterBaseAddsOnceAndRejectsWater::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;

    TestEqual(TEXT("No bases at start"), Manager->GetBases().Num(), 0);

    // Клетка на воде — не регистрируется.
    if (FGridCell* WaterCell = Manager->GetCell(1, 1))
    {
        WaterCell->bIsWater = true;
    }
    Manager->RegisterBase(FIntPoint(1, 1));
    TestEqual(TEXT("Water cell rejected"), Manager->GetBases().Num(), 0);

    // Обычная клетка — регистрируется, Biome берётся с клетки.
    if (FGridCell* Cell = Manager->GetCell(2, 2))
    {
        Cell->bIsWater = false;
    }
    Manager->RegisterBase(FIntPoint(2, 2));
    TestEqual(TEXT("Valid cell registered"), Manager->GetBases().Num(), 1);
    if (Manager->GetBases().Num() == 1)
    {
        TestTrue(TEXT("Registered base sits on the requested cell"), Manager->GetBases()[0].Cell == FIntPoint(2, 2));
        if (const FGridCell* Cell = Manager->GetCellConst(2, 2))
        {
            TestEqual(TEXT("Registered base biome matches the cell"), (uint8)Manager->GetBases()[0].Biome, (uint8)Cell->Biome);
        }
    }

    // Повторная регистрация той же клетки — не создаёт дубликат.
    Manager->RegisterBase(FIntPoint(2, 2));
    TestEqual(TEXT("Re-registering the same cell does not duplicate"), Manager->GetBases().Num(), 1);

    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistBases_BrewingValidAtShrinesAndBasesOnly,
    "Herbalist.Bases.BrewingValidAtShrinesAndBasesOnly",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistBases_BrewingValidAtShrinesAndBasesOnly::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;

    TestFalse(TEXT("An arbitrary cell is not a valid brewing location"), Manager->IsValidBrewingLocation(FIntPoint(5, 5)));

    // Капище (тот же вызов, что AShrineActor::BeginPlay делает в реальной игре).
    Manager->RegisterShrine(FIntPoint(0, 0), EShrineType::Ancestral);
    TestTrue(TEXT("A shrine cell is a valid brewing location"), Manager->IsValidBrewingLocation(FIntPoint(0, 0)));

    // База — второй, отдельный путь к тому же разрешению.
    if (FGridCell* Cell = Manager->GetCell(3, 3))
    {
        Cell->bIsWater = false;
    }
    Manager->RegisterBase(FIntPoint(3, 3));
    TestTrue(TEXT("A base cell is a valid brewing location"), Manager->IsValidBrewingLocation(FIntPoint(3, 3)));

    Manager->Destroy();
    return true;
}

#endif // WITH_AUTOMATION_TESTS && WITH_EDITOR
