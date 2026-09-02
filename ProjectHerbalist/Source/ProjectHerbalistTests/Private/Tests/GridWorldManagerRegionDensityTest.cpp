// Source/ProjectHerbalistTests/Private/Tests/GridWorldManagerRegionDensityTest.cpp
//
// Пер-региональная плотность контента на ABiomeRegionVolume (2026-09-02,
// прямой запрос пользователя: "настройки под все дела в этих волюмах").
// MinResourcesPerCell/MaxResourcesPerCell/WaterDensity -- раньше были одним
// числом на весь мир (AGridWorldManager), теперь настройка самого региона.
// ResourceRegrowthTimeSeconds не тестируется отдельно -- тот же класс
// "таймер, не проверяемый в автотестах без реального Tick", что и у
// ResourceRegrowthTime до этой правки (ни одного существующего теста на
// него не было).

#include "Core/World/GridWorldManager.h"
#include "Core/World/BiomeRegionVolume.h"
#include "Core/Types/BiomeTypes.h"
#include "Misc/AutomationTest.h"
#include "Editor.h"
#include "Engine/World.h"

#if WITH_AUTOMATION_TESTS && WITH_EDITOR

#include "TestWorldHelpers.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistRegionDensity_ZeroMaxResourcesPerCellSpawnsNothing,
    "Herbalist.RegionDensity.ZeroMaxResourcesPerCellSpawnsNothing",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistRegionDensity_ZeroMaxResourcesPerCellSpawnsNothing::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    ABiomeRegionVolume* Region = SpawnRegionCoveringWorldRect(World, EBiomeType::Bog, -50.f, -50.f, 1950.f, 1950.f);
    if (!TestNotNull(TEXT("Region spawned"), Region)) return false;
    Region->MinResourcesPerCell = 0;
    Region->MaxResourcesPerCell = 0;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World, { Region });
    if (!TestNotNull(TEXT("Manager spawned"), Manager)) { Region->Destroy(); return false; }

    const FGridCell* Cell = Manager->GetCellConst(5, 5);
    if (!TestNotNull(TEXT("Cell exists"), Cell)) { Manager->Destroy(); Region->Destroy(); return false; }
    if (!TestTrue(TEXT("Sanity: cell is claimed by the region"), Manager->IsCellClaimedByBiomeRegion(*Cell)))
    {
        Manager->Destroy(); Region->Destroy(); return false;
    }

    TestEqual(TEXT("MinResourcesPerCell=MaxResourcesPerCell=0 on the claiming region -- no resources spawned at InitializeCells"),
        Cell->ResourceActors.Num(), 0);

    // Регрессия на сам джиттер вызова напрямую -- RandRange(0,0) детерминированно
    // ноль итераций, не зависит от доступности IngredientRegistrySubsystem
    // в тестовом окружении (см. GetClaimingRegion -> MinRes/MaxRes в SpawnResourcesInCell).
    FGridCell MutableCopy = *Cell;
    MutableCopy.ResourceActors.Empty();
    Manager->SpawnResourcesInCell(MutableCopy);
    TestEqual(TEXT("Direct SpawnResourcesInCell call also respects the zero range"), MutableCopy.ResourceActors.Num(), 0);

    Manager->Destroy();
    Region->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistRegionDensity_GetClaimingRegionMatchesTheCoveringVolume,
    "Herbalist.RegionDensity.GetClaimingRegionMatchesTheCoveringVolume",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistRegionDensity_GetClaimingRegionMatchesTheCoveringVolume::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    ABiomeRegionVolume* Region = SpawnRegionCoveringWorldRect(World, EBiomeType::Bog, -50.f, -50.f, 950.f, 1950.f);
    if (!TestNotNull(TEXT("Region spawned"), Region)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World, { Region });
    if (!TestNotNull(TEXT("Manager spawned"), Manager)) { Region->Destroy(); return false; }

    const FGridCell* ClaimedCell = Manager->GetCellConst(5, 5);
    const FGridCell* UnclaimedCell = Manager->GetCellConst(15, 5);
    if (!TestNotNull(TEXT("Claimed cell exists"), ClaimedCell) || !TestNotNull(TEXT("Unclaimed cell exists"), UnclaimedCell))
    {
        Manager->Destroy(); Region->Destroy(); return false;
    }

    TestEqual(TEXT("GetClaimingRegion returns the actual covering volume"), Manager->GetClaimingRegion(*ClaimedCell), Region);
    TestNull(TEXT("GetClaimingRegion returns null outside all regions"), Manager->GetClaimingRegion(*UnclaimedCell));

    Manager->Destroy();
    Region->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistRegionDensity_WaterDensityIsAppliedPerRegionIndependently,
    "Herbalist.RegionDensity.WaterDensityIsAppliedPerRegionIndependently",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistRegionDensity_WaterDensityIsAppliedPerRegionIndependently::RunTest(const FString& Parameters)
{
    // Регион A: X=0..4, Y=0..3 (20 клеток), WaterDensity=1.0 -- почти вся
    // клетка должна стать водой. Регион B: X=5..8, Y=0..3 (16 клеток),
    // WaterDensity=0.0 -- ни одной капли. Непересекающиеся прямоугольники --
    // никакой доли/тай-брейка между регионами, чистая проверка независимости.
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    ABiomeRegionVolume* RegionA = SpawnRegionCoveringWorldRect(World, EBiomeType::Bog, -50.f, -50.f, 450.f, 350.f);
    ABiomeRegionVolume* RegionB = SpawnRegionCoveringWorldRect(World, EBiomeType::Steppe, 450.f, -50.f, 850.f, 350.f);
    if (!TestNotNull(TEXT("Region A spawned"), RegionA) || !TestNotNull(TEXT("Region B spawned"), RegionB)) return false;
    RegionA->WaterDensity = 1.0f;
    RegionB->WaterDensity = 0.0f;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World, { RegionA, RegionB });
    if (!TestNotNull(TEXT("Manager spawned"), Manager)) { RegionA->Destroy(); RegionB->Destroy(); return false; }

    int32 WaterInA = 0, TotalInA = 0, WaterInB = 0, TotalInB = 0;
    for (int32 X = 0; X <= 4; ++X)
    {
        for (int32 Y = 0; Y <= 3; ++Y)
        {
            const FGridCell* Cell = Manager->GetCellConst(X, Y);
            if (!Cell) continue;
            ++TotalInA;
            if (Cell->bIsWater) ++WaterInA;
        }
    }
    for (int32 X = 5; X <= 8; ++X)
    {
        for (int32 Y = 0; Y <= 3; ++Y)
        {
            const FGridCell* Cell = Manager->GetCellConst(X, Y);
            if (!Cell) continue;
            ++TotalInB;
            if (Cell->bIsWater) ++WaterInB;
        }
    }

    if (TestEqual(TEXT("Sanity: 20 cells sampled in Region A"), TotalInA, 20) &&
        TestEqual(TEXT("Sanity: 16 cells sampled in Region B"), TotalInB, 16))
    {
        TestTrue(TEXT("WaterDensity=1.0 region ends up almost entirely water (>=90%)"), WaterInA >= 18);
        TestEqual(TEXT("WaterDensity=0.0 region has zero water cells"), WaterInB, 0);
    }

    Manager->Destroy();
    RegionA->Destroy();
    RegionB->Destroy();
    return true;
}

#endif // WITH_AUTOMATION_TESTS && WITH_EDITOR
