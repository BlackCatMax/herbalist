// Source/ProjectHerbalistTests/Private/Tests/GridWorldManagerBiomeClaimTest.cpp
//
// AGridWorldManager::IsCellClaimedByBiomeRegion (2026-09-02, прямое требование
// пользователя: "сетка просто хранит все переменные, сплайн биома, попадающий
// на сетку, влияет на спавн того, что присуще биому"). Найдено на живом
// примере: пользователь увеличил сетку с 20x20 до 100x100, три размещённых
// на уровне ABiomeRegionVolume остались того же физического размера,
// покрывающего только малую часть новой сетки (91% клеток вне всех
// регионов) -- ресурсы/сущности спавнились по всей площади через блочный
// 5x5-фолбэк, а не только внутри нарисованных сплайнов. Гейт закрывает и
// спавн ресурсов (SpawnResourcesInCell), и биом-специфичное проявление
// сущностей (Низший/Берегиня/SeedTestLandmarks/SeedLegendaryAnchors) --
// НЕ "сквозную" ночную нечисть §16.5 (без привязки к биому по дизайну).

#include "Core/World/GridWorldManager.h"
#include "Core/World/BiomeRegionVolume.h"
#include "Core/Types/BiomeTypes.h"
#include "Misc/AutomationTest.h"
#include "Editor.h"
#include "Engine/World.h"

#if WITH_AUTOMATION_TESTS && WITH_EDITOR

#include "TestWorldHelpers.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistBiomeClaim_NoRegionsOnLevelClaimsEveryCell,
    "Herbalist.BiomeClaim.NoRegionsOnLevelClaimsEveryCell",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistBiomeClaim_NoRegionsOnLevelClaimsEveryCell::RunTest(const FString& Parameters)
{
    // Без единого ABiomeRegionVolume на уровне -- блочный фолбэк остаётся
    // ЕДИНСТВЕННЫМ источником биома для всей сетки (тестовое окружение,
    // сцены без PCG-авторства), не заплаткой. Старое поведение не меняется.
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("Manager spawned"), Manager)) return false;

    const FGridCell* Cell = Manager->GetCellConst(5, 5);
    if (!TestNotNull(TEXT("Cell exists"), Cell)) { Manager->Destroy(); return false; }

    TestTrue(TEXT("Sanity: block fallback used (no BiomeWeights)"), Cell->BiomeWeights.Num() == 0);
    TestTrue(TEXT("No regions on the level -- cell still counts as claimed"), Manager->IsCellClaimedByBiomeRegion(*Cell));

    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistBiomeClaim_CellsOutsideAllRegionsAreUnclaimedWhenRegionsExist,
    "Herbalist.BiomeClaim.CellsOutsideAllRegionsAreUnclaimedWhenRegionsExist",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistBiomeClaim_CellsOutsideAllRegionsAreUnclaimedWhenRegionsExist::RunTest(const FString& Parameters)
{
    // Регион покрывает только половину сетки (X < 1000, т.е. клетки 0-9 при
    // CellSize=100) -- та же ситуация, что и на живой карте пользователя
    // после увеличения GridSize без перерисовки сплайнов: часть клеток
    // реально внутри региона, часть -- вне всех регионов, но всё ещё
    // получает какой-то Biome через блочный фолбэк.
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    ABiomeRegionVolume* Region = SpawnRegionCoveringWorldRect(World, EBiomeType::Bog, -50.f, -50.f, 950.f, 1950.f);
    if (!TestNotNull(TEXT("Region spawned"), Region)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World, { Region });
    if (!TestNotNull(TEXT("Manager spawned"), Manager)) { Region->Destroy(); return false; }

    const FGridCell* ClaimedCell = Manager->GetCellConst(5, 5);     // внутри региона
    const FGridCell* UnclaimedCell = Manager->GetCellConst(15, 5);  // вне региона, за краем сетки старого размера

    if (!TestNotNull(TEXT("Claimed cell exists"), ClaimedCell) || !TestNotNull(TEXT("Unclaimed cell exists"), UnclaimedCell))
    {
        Manager->Destroy(); Region->Destroy(); return false;
    }

    TestTrue(TEXT("Sanity: cell inside the region has real BiomeWeights"), ClaimedCell->BiomeWeights.Num() > 0);
    TestTrue(TEXT("Sanity: cell outside the region fell back to the block pattern"), UnclaimedCell->BiomeWeights.Num() == 0);

    TestTrue(TEXT("Cell covered by a real region -- claimed"), Manager->IsCellClaimedByBiomeRegion(*ClaimedCell));
    TestFalse(TEXT("Cell outside all regions, while regions exist on the level -- NOT claimed, even though block fallback gave it a Biome"),
        Manager->IsCellClaimedByBiomeRegion(*UnclaimedCell));

    Manager->Destroy();
    Region->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistBiomeClaim_UnclaimedCellsDoNotSpawnResources,
    "Herbalist.BiomeClaim.UnclaimedCellsDoNotSpawnResources",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistBiomeClaim_UnclaimedCellsDoNotSpawnResources::RunTest(const FString& Parameters)
{
    // SpawnResourcesInCell гейтится ДО обращения к IngredientRegistrySubsystem
    // (см. GridWorldManagerCore.cpp) -- проверяем ранний выход напрямую:
    // список ResourceActors клетки не должен вообще тронуться, независимо
    // от того, доступен ли реестр ингредиентов в этом тестовом окружении.
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    ABiomeRegionVolume* Region = SpawnRegionCoveringWorldRect(World, EBiomeType::Bog, -50.f, -50.f, 950.f, 1950.f);
    if (!TestNotNull(TEXT("Region spawned"), Region)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World, { Region });
    if (!TestNotNull(TEXT("Manager spawned"), Manager)) { Region->Destroy(); return false; }

    FGridCell* UnclaimedCell = Manager->GetCell(15, 5);
    if (!TestNotNull(TEXT("Unclaimed cell exists"), UnclaimedCell)) { Manager->Destroy(); Region->Destroy(); return false; }
    if (!TestTrue(TEXT("Sanity: cell really is unclaimed"), !Manager->IsCellClaimedByBiomeRegion(*UnclaimedCell)))
    {
        Manager->Destroy(); Region->Destroy(); return false;
    }

    UnclaimedCell->ResourceActors.Empty();
    Manager->SpawnResourcesInCell(*UnclaimedCell);
    TestEqual(TEXT("Unclaimed cell's ResourceActors list stays empty after SpawnResourcesInCell"),
        UnclaimedCell->ResourceActors.Num(), 0);

    Manager->Destroy();
    Region->Destroy();
    return true;
}

#endif // WITH_AUTOMATION_TESTS && WITH_EDITOR
