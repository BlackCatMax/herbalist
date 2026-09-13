// Source/ProjectHerbalistTests/Private/Tests/ChunkSummaryTest.cpp
//
// Разметка мира, этап 7 (2026-09-13) -- сводки чанков вместо обходов всего
// мира (DESIGN_World_Layout.md §9, §10: «сводки совпадают с полным обходом»).
// Сравнение всегда с независимым полным обходом ForEachCell, а не с тем же
// кодом по чанкам: потеря или двойной счёт клеток на границе чанков видны.

#include "Core/World/GridWorldManager.h"
#include "Core/World/WorldLayout.h"
#include "Core/Types/HerbalistCoreMath.h"
#include "Core/Types/BiomeTypes.h"
#include "Core/Config/HerbalistSettings.h"
#include "Core/Simulation/Public/DeltaTypes.h"
#include "Misc/AutomationTest.h"
#include "Editor.h"
#include "Engine/World.h"
#include "EngineUtils.h"

#if WITH_AUTOMATION_TESTS && WITH_EDITOR

#include "TestWorldHelpers.h"

namespace
{
    struct FFullWalkForSummaryTest
    {
        TMap<FName, FHerbalistBiomeFieldSum> Biomes;
        int32 CellCount = 0;
        int32 DegradingCount = 0;
        double DistortionSum = 0.0;
        float DistortionMin = 1.0f;
        float DistortionMax = 0.0f;
        int32 LandCellCount = 0;
        double LandDistortionSum = 0.0;
        double DistanceWithHistorySum = 0.0;
    };

    // Полный обход -- ровно то, что считали функции до сводок.
    FFullWalkForSummaryTest WalkAllCellsForSummaryTest(AGridWorldManager* Manager)
    {
        FFullWalkForSummaryTest Walk;
        Manager->ForEachCell([&Walk, Manager](const FGridCell& Cell)
        {
            const FRealState Default = BiomeDefaultStateForCell(Cell);
            FHerbalistBiomeFieldSum& Biome = Walk.Biomes.FindOrAdd(FBiomeDefaults::BiomeTypeToName(Cell.Biome));
            Biome.MorokSum += Cell.State.Meta.Distortion - Default.Meta.Distortion;
            Biome.ZaryanaSum += Cell.State.Meta.Stability - Default.Meta.Stability;
            Biome.PositionSum += Manager->GetCellWorldPositionFlat(Cell.X, Cell.Y);
            ++Biome.CellCount;
            ++Walk.CellCount;
            Walk.DegradingCount += Cell.Memory.bDegrading ? 1 : 0;
            Walk.DistortionSum += Cell.State.Meta.Distortion;
            Walk.DistortionMin = FMath::Min(Walk.DistortionMin, Cell.State.Meta.Distortion);
            Walk.DistortionMax = FMath::Max(Walk.DistortionMax, Cell.State.Meta.Distortion);
            if (!Cell.bIsWater)
            {
                ++Walk.LandCellCount;
                Walk.LandDistortionSum += Cell.State.Meta.Distortion;
            }
            Walk.DistanceWithHistorySum += HerbalistCore::Math::DistanceWithHistory(Cell.State, Cell.Memory.AverageCoherence);
        });
        return Walk;
    }

    // Все величины сводок против полного обхода. Возвращает обход -- для
    // проверок, которые нужны не на каждом шаге.
    FFullWalkForSummaryTest ExpectSummariesMatchWalk(FAutomationTestBase& Test, AGridWorldManager* Manager, const TCHAR* Stage)
    {
        const FFullWalkForSummaryTest Walk = WalkAllCellsForSummaryTest(Manager);
        const TMap<FName, FHerbalistBiomeFieldSum> Sums = Manager->GetBiomeFieldSums();
        Test.TestEqual(FString::Printf(TEXT("%s: те же биомы"), Stage), Sums.Num(), Walk.Biomes.Num());
        for (const TPair<FName, FHerbalistBiomeFieldSum>& Pair : Walk.Biomes)
        {
            const FHerbalistBiomeFieldSum* Sum = Sums.Find(Pair.Key);
            const FString Label = FString::Printf(TEXT("%s, %s"), Stage, *Pair.Key.ToString());
            if (!Test.TestNotNull(FString::Printf(TEXT("%s: биом есть в сводке"), *Label), Sum))
            {
                continue;
            }
            Test.TestEqual(FString::Printf(TEXT("%s: клеток"), *Label), Sum->CellCount, Pair.Value.CellCount);
            Test.TestEqual(FString::Printf(TEXT("%s: сумма Морока"), *Label), Sum->MorokSum, Pair.Value.MorokSum, 1e-6);
            Test.TestEqual(FString::Printf(TEXT("%s: сумма Заряны"), *Label), Sum->ZaryanaSum, Pair.Value.ZaryanaSum, 1e-6);
            Test.TestTrue(FString::Printf(TEXT("%s: центр"), *Label),
                (Sum->PositionSum / Sum->CellCount).Equals(Pair.Value.PositionSum / Pair.Value.CellCount, 0.01));
        }

        int32 CellCount = 0;
        int32 DegradingCount = 0;
        float DistortionMax = 0.0f;
        double DistanceSum = 0.0;
        double LandSum = 0.0;
        int32 LandCount = 0;
        Manager->ForEachChunkSummary([&](const FHerbalistChunkSummary& Summary)
        {
            CellCount += Summary.CellCount;
            DegradingCount += Summary.DegradingCount;
            if (Summary.CellCount > 0)
            {
                DistortionMax = FMath::Max(DistortionMax, Summary.DistortionMax);
            }
            DistanceSum += Summary.DistanceWithHistorySum;
            LandSum += Summary.LandDistortionSum;
            LandCount += Summary.LandCellCount;
        });
        Test.TestEqual(FString::Printf(TEXT("%s: клеток в сводках"), Stage), CellCount, Walk.CellCount);
        Test.TestEqual(FString::Printf(TEXT("%s: распадающихся"), Stage), DegradingCount, Walk.DegradingCount);
        Test.TestEqual(FString::Printf(TEXT("%s: максимум Distortion"), Stage), DistortionMax, Walk.DistortionMax, 1e-6f);
        Test.TestEqual(FString::Printf(TEXT("%s: сумма расстояний с историей (Буян)"), Stage), DistanceSum, Walk.DistanceWithHistorySum, 1e-4);
        Test.TestEqual(FString::Printf(TEXT("%s: клетки суши (фрагменты)"), Stage), LandCount, Walk.LandCellCount);
        Test.TestEqual(FString::Printf(TEXT("%s: Distortion суши (фрагменты)"), Stage), LandSum, Walk.LandDistortionSum, 1e-4);
        return Walk;
    }

    struct FScopedSummarySettings
    {
        UHerbalistSettings* Settings;
        float SavedRadiusMeters;
        int32 SavedChunkSize;

        explicit FScopedSummarySettings(float RadiusMeters)
            : Settings(GetMutableDefault<UHerbalistSettings>())
        {
            SavedRadiusMeters = Settings->ActiveSimulationRadiusMeters;
            SavedChunkSize = Settings->ChunkSizeInCells;
            Settings->ActiveSimulationRadiusMeters = RadiusMeters;
        }

        ~FScopedSummarySettings()
        {
            Settings->ActiveSimulationRadiusMeters = SavedRadiusMeters;
            Settings->ChunkSizeInCells = SavedChunkSize;
        }
    };

    // Ландшафт ±63 м с числами L_TestDev: клетка 9 м, сетка 28 x 28 от
    // (-14, -14), чанк 7 клеток, чанки сетки -2..1 (как в GlobalCellCoordTest).
    AGridWorldManager* SpawnShiftedManagerForSummaryTest(UWorld* World)
    {
        for (TActorIterator<AGridWorldManager> It(World); It; ++It)
        {
            It->Destroy();
        }
        AGridWorldManager* Manager = World->SpawnActor<AGridWorldManager>();
        if (Manager)
        {
            FHerbalistWorldLayoutSource Source;
            Source.bHasLandscape = true;
            Source.QuadSizeCm = 100.0;
            Source.ComponentSizeQuads = 126;
            Source.LandscapeOrigin = FVector2D(-6300.0, -6300.0);
            Source.LandscapeMin = FVector2D(-6300.0, -6300.0);
            Source.LandscapeMax = FVector2D(6300.0, 6300.0);
            Source.bHasStreamingGrid = true;
            Source.StreamingGridName = FName(TEXT("MainPartition"));
            Source.StreamingCellSizeCm = 12600.0;
            Source.StreamingLoadingRangeCm = 25200.0;
            Source.StreamingGridOrigin = FVector2D::ZeroVector;
            Manager->BakedLayoutSource = Source;
            Manager->DispatchBeginPlay();
        }
        return Manager;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistChunkSummary_SummariesMatchFullWalk,
    "Herbalist.WorldLayout.Summary.SummariesMatchFullWalk",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistChunkSummary_SummariesMatchFullWalk::RunTest(const FString& Parameters)
{
    // Без центров активности живы все чанки -- путь автотестов и первых кадров
    // PIE. Кэш не участвует; путь через кэш -- CachedSummariesMatchFullWalkOnShiftedGrid.
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;
    AGridWorldManager* Manager = SpawnAndBeginPlay(World, {}, 777);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;

    // Разнообразим мир: порча, распад, история -- по детерминированному потоку.
    // Вода -- явно, строка из пяти клеток: с 2026-09-13 вода только из регионов
    // воды, а регионов здесь нет.
    const FIntPoint GridMin = Manager->GetGridMinCell();
    FRandomStream Rng(2026);
    for (int32 Step = 0; Step < 60; ++Step)
    {
        FGridCell* Cell = Manager->GetCell(GridMin.X + Rng.RandRange(0, Manager->GridSizeX - 1), GridMin.Y + Rng.RandRange(0, Manager->GridSizeY - 1));
        if (Cell)
        {
            Cell->State.Meta.Distortion = Rng.FRand();
            Cell->State.Meta.Stability = Rng.FRand();
            Cell->Memory.bDegrading = Rng.FRand() < 0.3f;
            Cell->Memory.AverageCoherence = Rng.FRand();
        }
    }
    for (int32 X = 0; X < 5; ++X)
    {
        if (FGridCell* Cell = Manager->GetCell(GridMin.X + X, GridMin.Y))
        {
            Cell->bIsWater = true;
        }
    }

    const FFullWalkForSummaryTest Walk = ExpectSummariesMatchWalk(*this, Manager, TEXT("Все чанки живы"));
    TestTrue(FString::Printf(TEXT("Есть и суша, и вода: суши %d из %d"), Walk.LandCellCount, Walk.CellCount),
        Walk.LandCellCount > 0 && Walk.LandCellCount < Walk.CellCount);

    const FString Report = Manager->GetGridCorruptionReport();
    TestTrue(FString::Printf(TEXT("Отчёт: клеток и распадающихся как у полного обхода (%s)"), *Report),
        Report.Contains(FString::Printf(TEXT("%d cells, %d degrading"), Walk.CellCount, Walk.DegradingCount)));
    TestTrue(FString::Printf(TEXT("Отчёт: среднее, минимум и максимум Distortion (%s)"), *Report),
        Report.Contains(FString::Printf(TEXT("avg=%.3f min=%.3f max=%.3f"),
            static_cast<float>(Walk.DistortionSum / Walk.CellCount), Walk.DistortionMin, Walk.DistortionMax)));

    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistChunkSummary_CachedSummariesMatchFullWalkOnShiftedGrid,
    "Herbalist.WorldLayout.Summary.CachedSummariesMatchFullWalkOnShiftedGrid",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistChunkSummary_CachedSummariesMatchFullWalkOnShiftedGrid::RunTest(const FString& Parameters)
{
    // Путь игры: центр активности задан, неживые чанки -- из кэша. Сетка
    // сдвинута, чанки с отрицательными номерами.
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;
    FScopedSummarySettings ScopedSettings(/*RadiusMeters=*/100.0f);
    AGridWorldManager* Manager = SpawnShiftedManagerForSummaryTest(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;
    if (!TestTrue(TEXT("Сетка от (-14, -14), чанк 7, радиус 1 чанк"),
        Manager->GetGridMinCell() == FIntPoint(-14, -14) && Manager->GetChunkSizeInCells() == 7 && Manager->GetActiveRadiusInChunks() == 1))
    {
        Manager->Destroy();
        return false;
    }

    // Живы чанки (-2..-1, -2..-1); кэш заполняется первым запросом.
    Manager->SetActiveChunkCentersForTests({ FIntPoint(-2, -2) });
    ExpectSummariesMatchWalk(*this, Manager, TEXT("Исходно"));

    // Правки через ApplyStateDelta (помечает клетки): живой чанк (-2, -2),
    // неживые (1, 1) и (0, -2).
    FStateDelta Delta;
    for (const FIntPoint& Coord : { FIntPoint(-14, -14), FIntPoint(13, 13), FIntPoint(0, -14) })
    {
        if (const FGridCell* Original = Manager->GetCellConst(Coord.X, Coord.Y))
        {
            FGridCell Changed = *Original;
            Changed.State.Meta.Distortion = 0.87f;
            Changed.State.Meta.Stability = 0.13f;
            Changed.Memory.bDegrading = true;
            Changed.Memory.AverageCoherence = 0.25f;
            Delta.WorldChanges.Add(Coord, Changed);
        }
    }
    TestEqual(TEXT("Три правки"), Delta.WorldChanges.Num(), 3);
    Manager->ApplyStateDelta(Delta);
    ExpectSummariesMatchWalk(*this, Manager, TEXT("После ApplyStateDelta"));

    // Живой чанк стал неживым и наоборот.
    Manager->SetActiveChunkCentersForTests({ FIntPoint(1, 1) });
    ExpectSummariesMatchWalk(*this, Manager, TEXT("Центр сдвинут"));

    // Запись мимо пометки в неживом чанке (-2, 1): кэш её не видит, пока
    // загрузка сейва не сбросит его целиком.
    FGridCell* Unmarked = Manager->GetCell(-14, 13);
    if (TestNotNull(TEXT("Клетка (-14, 13)"), Unmarked))
    {
        Unmarked->State.Meta.Distortion = 0.42f;
        Manager->ApplySaveCells(Manager->CaptureSaveCells());
        ExpectSummariesMatchWalk(*this, Manager, TEXT("После загрузки сейва"));
    }

    Manager->SetActiveChunkCentersForTests({});
    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistChunkSummary_InactiveChunkKeepsCacheUntilMarked,
    "Herbalist.WorldLayout.Summary.InactiveChunkKeepsCacheUntilMarked",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistChunkSummary_InactiveChunkKeepsCacheUntilMarked::RunTest(const FString& Parameters)
{
    // Неживой чанк заморожен: релаксация и граф его не трогают, и сводка из
    // кэша верна. Изменение, прошедшее через MarkCellDirty, её обновляет.
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;
    FScopedSummarySettings ScopedSettings(/*RadiusMeters=*/0.0f);
    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;
    if (!TestEqual(TEXT("Радиус 0 -- живой только чанк центра"), Manager->GetActiveRadiusInChunks(), 0)) { Manager->Destroy(); return false; }

    const FIntPoint GridMin = Manager->GetGridMinCell();
    const FIntPoint NearCell = GridMin;
    const FIntPoint FarCell(GridMin.X + Manager->GridSizeX - 1, GridMin.Y + Manager->GridSizeY - 1);
    const FIntPoint CentreChunk = Manager->GetChunkCoordForCell(NearCell.X, NearCell.Y);
    Manager->SetActiveChunkCentersForTests({ CentreChunk });
    if (!TestTrue(TEXT("Дальняя клетка -- в другом чанке"), Manager->GetChunkCoordForCell(FarCell.X, FarCell.Y) != CentreChunk))
    {
        Manager->SetActiveChunkCentersForTests({});
        Manager->Destroy();
        return false;
    }

    auto SummaryTotals = [Manager](int32& OutCells)
    {
        float Max = 0.0f;
        OutCells = 0;
        Manager->ForEachChunkSummary([&Max, &OutCells](const FHerbalistChunkSummary& Summary)
        {
            OutCells += Summary.CellCount;
            if (Summary.CellCount > 0)
            {
                Max = FMath::Max(Max, Summary.DistortionMax);
            }
        });
        return Max;
    };

    Manager->ForEachCell([](FGridCell& Cell) { Cell.State.Meta.Distortion = 0.1f; });
    Manager->InvalidateAllChunkSummariesForTests();
    int32 Cells = 0;
    TestEqual(TEXT("Исходно максимум 0.1"), SummaryTotals(Cells), 0.1f, 1e-6f);

    FGridCell* Far = Manager->GetCell(FarCell.X, FarCell.Y);
    FGridCell* Near = Manager->GetCell(NearCell.X, NearCell.Y);
    if (!TestNotNull(TEXT("Дальняя клетка"), Far) || !TestNotNull(TEXT("Ближняя клетка"), Near))
    {
        Manager->SetActiveChunkCentersForTests({});
        Manager->Destroy();
        return false;
    }

    Far->State.Meta.Distortion = 0.9f;
    TestEqual(TEXT("Запись мимо MarkCellDirty в неживом чанке -- сводка из кэша"), SummaryTotals(Cells), 0.1f, 1e-6f);

    Manager->MarkCellDirtyForTests(FarCell.X, FarCell.Y);
    TestEqual(TEXT("После MarkCellDirty -- сводка пересчитана"), SummaryTotals(Cells), 0.9f, 1e-6f);

    Near->State.Meta.Distortion = 0.95f;
    TestEqual(TEXT("Живой чанк пересчитывается при каждом запросе"), SummaryTotals(Cells), 0.95f, 1e-6f);

    // Размер чанка из настроек сменился на лету (без разметки он не
    // выводится): кэш старой нарезки отброшен, клетки не теряются и не
    // считаются дважды, запись мимо пометки видна.
    Far->State.Meta.Distortion = 0.97f;
    const int32 OldChunkSize = Manager->GetChunkSizeInCells();
    ScopedSettings.Settings->ChunkSizeInCells = OldChunkSize == 5 ? 6 : 5;
    if (TestTrue(TEXT("Размер чанка сменился"), Manager->GetChunkSizeInCells() != OldChunkSize))
    {
        Manager->SetActiveChunkCentersForTests({ Manager->GetChunkCoordForCell(NearCell.X, NearCell.Y) });
        TestEqual(TEXT("Новая нарезка -- максимум с записью мимо пометки"), SummaryTotals(Cells), 0.97f, 1e-6f);
        TestEqual(TEXT("...и все клетки сетки ровно по разу"), Cells, Manager->GridSizeX * Manager->GridSizeY);
    }

    Manager->SetActiveChunkCentersForTests({});
    Manager->Destroy();
    return true;
}

#endif // WITH_AUTOMATION_TESTS && WITH_EDITOR
