// Source/ProjectHerbalistTests/Private/Tests/GridWorldManagerSpawnPositionTest.cpp
//
// Спавн внутри формы PCG-биома (2026-09-02) — AGridWorldManager::
// GetSpawnPositionWithinBiome. Тот же DispatchBeginPlay-паттерн, что уже
// GridWorldManagerBiomeRegionTest.cpp — регион(ы) спавнятся ДО менеджера
// (InitializeCells читает TActorIterator<ABiomeRegionVolume> внутри
// BeginPlay, порядок важен).

#include "Core/World/GridWorldManager.h"
#include "Core/World/BiomeRegionVolume.h"
#include "Core/Types/BiomeTypes.h"
#include "Misc/AutomationTest.h"
#include "Editor.h"
#include "Engine/World.h"

#if WITH_AUTOMATION_TESTS && WITH_EDITOR

#include "TestWorldHelpers.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistSpawnPosition_ZeroRadiusReturnsExactCellCenter,
    "Herbalist.SpawnPosition.ZeroRadiusReturnsExactCellCenter",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistSpawnPosition_ZeroRadiusReturnsExactCellCenter::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("Manager spawned"), Manager)) return false;

    FRandomStream Rng(1);
    const FVector Pos = Manager->GetSpawnPositionWithinBiome(5, 5, 0.0f, Rng);
    const FVector Center = Manager->GetCellWorldPositionFlat(5, 5);

    TestEqual(TEXT("JitterRadius=0 -- exact cell center, X"), Pos.X, Center.X);
    TestEqual(TEXT("JitterRadius=0 -- exact cell center, Y"), Pos.Y, Center.Y);

    Manager->Destroy();
    return true;
}

// Регрессия 2026-09-03: пользователь пожаловался на "отвратительный
// тайлинг" в расстановке ресурсов -- причиной был джиттер CellSize*0.3f,
// покрывавший только 36% площади клетки вокруг центра (квадрат джиттера
// внутри квадрата клетки), с пустым необитаемым кольцом по краям. На
// масштабе всего мира это читалось как решётка кустов с видимыми швами по
// границам клеток. GetResourceJitterRadius() -- единственный источник
// истины теперь для всех трёх мест, что раньше дублировали формулу
// (SpawnResourcesInCell/SpawnResourceActor/PreviewResourceSpawnPoints);
// этот тест не даёт кому-нибудь молча вернуть маленькую долю обратно.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistSpawnPosition_JitterRadiusCoversTheWholeCellNotJustItsCenter,
    "Herbalist.SpawnPosition.JitterRadiusCoversTheWholeCellNotJustItsCenter",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistSpawnPosition_JitterRadiusCoversTheWholeCellNotJustItsCenter::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("Manager spawned"), Manager)) return false;

    // Половина CellSize -- квадрат джиттера ровно совпадает по размеру с
    // самой клеткой (классический "jittered grid", Cook 1986), не узкая
    // доля вокруг центра.
    TestEqual(TEXT("Jitter radius is exactly half the cell size"),
        Manager->GetResourceJitterRadius(), Manager->CellSize * 0.5f);

    Manager->Destroy();
    return true;
}

// Тот же баг, найденный пользователем отдельно ("то же касается существ"):
// EntityManifestationJitterRadius был абсолютным числом в сантиметрах
// (30.0f), настроенным на дев-масштабе (CellSize=100). После перехода на
// 5x5 км (CellSize=1000) те же 30 см стали 3% клетки -- сущности стояли
// практически ровно в центре, решётка даже заметнее, чем была у ресурсов.
// Переведено в долю от CellSize (EntityManifestationJitterFraction).
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistSpawnPosition_EntityJitterScalesWithCellSizeNotFixedCentimeters,
    "Herbalist.SpawnPosition.EntityJitterScalesWithCellSizeNotFixedCentimeters",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistSpawnPosition_EntityJitterScalesWithCellSizeNotFixedCentimeters::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("Manager spawned"), Manager)) return false;

    // Меньше, чем у ресурсов (0.5) -- сущность семантически "якорь", не
    // должна плавать по всей клетке, только не стоять штырём по центру.
    TestTrue(TEXT("Entity jitter is smaller than resource jitter (anchor semantics)"),
        Manager->GetEntityManifestationJitterRadius() < Manager->GetResourceJitterRadius());

    // Главная регрессия: радиус растёт вместе с CellSize, не остаётся
    // прибитым к абсолютному числу сантиметров.
    const float JitterAtOriginalScale = Manager->GetEntityManifestationJitterRadius();
    Manager->CellSize *= 10.0f;
    const float JitterAtTenXScale = Manager->GetEntityManifestationJitterRadius();
    TestEqual(TEXT("Jitter radius scales linearly with CellSize"),
        JitterAtTenXScale, JitterAtOriginalScale * 10.0f);

    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistSpawnPosition_NoRegionsStillJittersWithoutCrashing,
    "Herbalist.SpawnPosition.NoRegionsStillJittersWithoutCrashing",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistSpawnPosition_NoRegionsStillJittersWithoutCrashing::RunTest(const FString& Parameters)
{
    // Блочный фолбэк / тестовое окружение без волюмов -- CachedBiomeRegions
    // пуст, поведение обязано остаться "джиттер без проверки формы", тем
    // же, что и раньше у SpawnResourcesInCell, не крашить и не залипать на
    // центре клетки.
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("Manager spawned"), Manager)) return false;

    FRandomStream Rng(1);
    const FVector Center = Manager->GetCellWorldPositionFlat(5, 5);
    bool bAnyDifferentFromCenter = false;
    for (int32 i = 0; i < 10; ++i)
    {
        const FVector Pos = Manager->GetSpawnPositionWithinBiome(5, 5, 30.0f, Rng);
        if (!FMath::IsNearlyEqual(Pos.X, Center.X) || !FMath::IsNearlyEqual(Pos.Y, Center.Y))
        {
            bAnyDifferentFromCenter = true;
        }
    }
    TestTrue(TEXT("Without any placed region, jitter still moves the position away from dead-center"), bAnyDifferentFromCenter);

    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistSpawnPosition_WithRealRegionEveryPositionIsInsideItsShape,
    "Herbalist.SpawnPosition.WithRealRegionEveryPositionIsInsideItsShape",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistSpawnPosition_WithRealRegionEveryPositionIsInsideItsShape::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    // Регион ДО менеджера -- InitializeCells должен его увидеть и заполнить
    // CachedBiomeRegions/Cell.BiomeWeights.
    ABiomeRegionVolume* Region = SpawnRegionCoveringWorldRect(World, EBiomeType::Bog, -50.f, -50.f, 1950.f, 1950.f);
    if (!TestNotNull(TEXT("Region spawned"), Region)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World, { Region });
    if (!TestNotNull(TEXT("Manager spawned"), Manager)) { Region->Destroy(); return false; }

    const FGridCell* Cell = Manager->GetCellConst(5, 5);
    if (!TestTrue(TEXT("Cell is actually covered by the region (sanity check)"), Cell && Cell->BiomeWeights.Num() == 1))
    {
        Manager->Destroy(); Region->Destroy(); return false;
    }

    FRandomStream Rng(1);
    const FVector Center = Manager->GetCellWorldPositionFlat(5, 5);
    bool bAnyDifferentFromCenter = false;
    bool bAllInsideShape = true;
    for (int32 i = 0; i < 20; ++i)
    {
        const FVector Pos = Manager->GetSpawnPositionWithinBiome(5, 5, 30.0f, Rng);
        if (!FMath::IsNearlyEqual(Pos.X, Center.X) || !FMath::IsNearlyEqual(Pos.Y, Center.Y))
        {
            bAnyDifferentFromCenter = true;
        }
        if (!Region->IsPointInside(Pos))
        {
            bAllInsideShape = false;
        }
    }
    TestTrue(TEXT("Jitter actually moves the position away from dead-center"), bAnyDifferentFromCenter);
    TestTrue(TEXT("Every returned position passes IsPointInside for its own region's shape"), bAllInsideShape);

    Manager->Destroy();
    Region->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistSpawnPosition_NearRegionEdgeStillRespectsShape,
    "Herbalist.SpawnPosition.NearRegionEdgeStillRespectsShape",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistSpawnPosition_NearRegionEdgeStillRespectsShape::RunTest(const FString& Parameters)
{
    // Клетка у самого края формы (радиус джиттера больше, чем расстояние
    // до границы) -- то самое условие, ради которого нужна проверка
    // IsPointInside, не просто джиттер: наивный джиттер регулярно уводил
    // бы точку за пределы формы здесь.
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    // Регион кончается ровно на X=550 (мировая) -- клетка (5,5) с центром
    // X=500 (CellSize=100 по умолчанию) лежит в 50 юнитах от границы,
    // радиус джиттера ниже (30) её иногда пересекает.
    ABiomeRegionVolume* Region = SpawnRegionCoveringWorldRect(World, EBiomeType::Bog, -50.f, -50.f, 550.f, 1950.f);
    if (!TestNotNull(TEXT("Region spawned"), Region)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World, { Region });
    if (!TestNotNull(TEXT("Manager spawned"), Manager)) { Region->Destroy(); return false; }

    const FGridCell* Cell = Manager->GetCellConst(5, 5);
    if (!TestTrue(TEXT("Edge cell is covered by the region (sanity check)"), Cell && Cell->BiomeWeights.Num() == 1))
    {
        Manager->Destroy(); Region->Destroy(); return false;
    }

    FRandomStream Rng(7);
    bool bAllInsideShape = true;
    for (int32 i = 0; i < 30; ++i)
    {
        const FVector Pos = Manager->GetSpawnPositionWithinBiome(5, 5, 30.0f, Rng);
        if (!Region->IsPointInside(Pos))
        {
            bAllInsideShape = false;
            break;
        }
    }
    TestTrue(TEXT("Even near the region's edge, every returned position stays inside its shape"), bAllInsideShape);

    Manager->Destroy();
    Region->Destroy();
    return true;
}

#endif // WITH_AUTOMATION_TESTS && WITH_EDITOR
