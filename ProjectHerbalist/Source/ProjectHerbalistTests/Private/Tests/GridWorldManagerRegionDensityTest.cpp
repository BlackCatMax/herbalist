// Source/ProjectHerbalistTests/Private/Tests/GridWorldManagerRegionDensityTest.cpp
//
// Пер-региональная плотность контента на ABiomeRegionVolume (2026-09-02,
// прямой запрос пользователя: "настройки под все дела в этих волюмах").
// MinResourcesPer100SquareMeters/MaxResourcesPer100SquareMeters (до 2026-09-12 --
// на клетку) -- раньше были одним числом на весь мир (AGridWorldManager),
// теперь настройка самого региона. WaterDensity снята 2026-09-13: вода только
// из регионов воды (GridWorldManagerWaterRegionTest.cpp).
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
    Region->MinResourcesPer100SquareMeters = 0.0f;
    Region->MaxResourcesPer100SquareMeters = 0.0f;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World, { Region });
    if (!TestNotNull(TEXT("Manager spawned"), Manager)) { Region->Destroy(); return false; }

    const FGridCell* Cell = Manager->GetCellConst(5, 5);
    if (!TestNotNull(TEXT("Cell exists"), Cell)) { Manager->Destroy(); Region->Destroy(); return false; }
    if (!TestTrue(TEXT("Sanity: cell is claimed by the region"), Manager->IsCellClaimedByBiomeRegion(*Cell)))
    {
        Manager->Destroy(); Region->Destroy(); return false;
    }

    TestEqual(TEXT("Zero density per 100 m2 on the claiming region -- no resources spawned at InitializeCells"),
        Cell->ResourceActors.Num(), 0);

    // Регрессия на сам джиттер вызова напрямую -- нулевая плотность детерминированно
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

#endif // WITH_AUTOMATION_TESTS && WITH_EDITOR
