// Source/ProjectHerbalistTests/Private/Tests/CellPageStreamingTest.cpp
//
// Разметка мира, этап 8 (2026-09-13) -- выгрузка и загрузка страниц клеток
// (DESIGN_World_Layout.md §6, §10). Страница выгружается, когда под ней нет
// загруженной земли и нет активных чанков: отклонения от основы уходят в
// разреженные дельты, клетки выбрасываются. Загрузка собирает основу заново
// (биом по регионам, вода, высоты) и накладывает дельты. Земля в мире
// редактора не стримится -- покрытие задаётся SetGroundCoverageForTests.
//
// Сетка 28 x 28 от (-14, -14): четыре страницы 14 x 14, чанк 7 клеток, радиус
// активной области 1 чанк. Центр в чанке (-2, -2) задевает чанки -2..-1 --
// только страницу (-1, -1).

#include "Core/World/GridWorldManager.h"
#include "Core/World/WorldLayout.h"
#include "Core/Config/HerbalistSettings.h"
#include "Core/Save/HerbalistSaveTypes.h"
#include "Core/Simulation/Public/DeltaTypes.h"
#include "Misc/AutomationTest.h"
#include "Editor.h"
#include "Engine/World.h"
#include "EngineUtils.h"

#if WITH_AUTOMATION_TESTS && WITH_EDITOR

namespace
{
    struct FScopedStreamingTestRadius
    {
        UHerbalistSettings* Settings;
        float SavedRadiusMeters;

        FScopedStreamingTestRadius()
            : Settings(GetMutableDefault<UHerbalistSettings>())
        {
            SavedRadiusMeters = Settings->ActiveSimulationRadiusMeters;
            Settings->ActiveSimulationRadiusMeters = 100.0f;
        }

        ~FScopedStreamingTestRadius()
        {
            Settings->ActiveSimulationRadiusMeters = SavedRadiusMeters;
        }
    };

    AGridWorldManager* SpawnStreamingTestManager(UWorld* World, double HalfExtentCm)
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
            Source.LandscapeOrigin = FVector2D(-HalfExtentCm, -HalfExtentCm);
            Source.LandscapeMin = FVector2D(-HalfExtentCm, -HalfExtentCm);
            Source.LandscapeMax = FVector2D(HalfExtentCm, HalfExtentCm);
            Source.bHasStreamingGrid = true;
            Source.StreamingGridName = FName(TEXT("MainPartition"));
            Source.StreamingCellSizeCm = 12600.0;
            Source.StreamingLoadingRangeCm = 25200.0;
            Source.StreamingGridOrigin = FVector2D::ZeroVector;
            Manager->BakedLayoutSource = Source;
            Manager->DispatchBeginPlay();
            // Места, засеянные при старте, закрепляют свои страницы от выгрузки
            // -- в тестах геометрии выгрузки мест нет, закрепление проверяет
            // отдельный тест.
            Manager->SetEntityLandmarks({});
            Manager->SetShrines({});
            Manager->SetLegendaryAnchorsForTests({});
        }
        return Manager;
    }

    // Земля только под страницей (-1, -1): клетки -14..-1 -- мировые -126..0 м.
    // Границы включительные: чанк (0, ...) касается края, но его дальние углы
    // не покрыты.
    const FBox2D GroundUnderWestPage(FVector2D(-12600.0, -12600.0), FVector2D(0.0, 0.0));
    const FBox2D GroundUnderWholeSmallGrid(FVector2D(-12600.0, -12600.0), FVector2D(12600.0, 12600.0));

    void StreamTo(AGridWorldManager* Manager, const FBox2D& Ground, const FIntPoint& CentreChunk)
    {
        Manager->SetGroundCoverageForTests({ Ground });
        Manager->SetActiveChunkCentersForTests({ CentreChunk });
        Manager->CatchUpActivatedChunks();
    }

    void StopStreaming(AGridWorldManager* Manager)
    {
        Manager->SetActiveChunkCentersForTests({});
        Manager->ClearGroundCoverageForTests();
    }

    // То, что основа клетки обязана воспроизвести один в один.
    struct FCellBaseSnapshot
    {
        FIntPoint Cell;
        EBiomeType Biome = EBiomeType::Tundra;
        TArray<FBiomeWeightEntry> Weights;
        bool bIsWater = false;
        FName WaterTypeID;
        FRealState State;
        FRealState TargetState;
        float HarvestStress = 0.0f;
        bool bDegrading = false;
        float Height = 0.0f;
    };

    FCellBaseSnapshot SnapshotCellBase(const AGridWorldManager* Manager, const FGridCell& Cell)
    {
        FCellBaseSnapshot Snapshot;
        Snapshot.Cell = FIntPoint(Cell.X, Cell.Y);
        Snapshot.Biome = Cell.Biome;
        Snapshot.Weights = Cell.BiomeWeights;
        Snapshot.bIsWater = Cell.bIsWater;
        Snapshot.WaterTypeID = Cell.WaterTypeID;
        Snapshot.State = Cell.State;
        Snapshot.TargetState = Cell.TargetState;
        Snapshot.HarvestStress = Cell.HarvestStress;
        Snapshot.bDegrading = Cell.Memory.bDegrading;
        Snapshot.Height = Manager->GetCellHeight(Cell.X, Cell.Y);
        return Snapshot;
    }

    bool SameMeta(const FRealState& A, const FRealState& B)
    {
        return A.Meta.Purity == B.Meta.Purity && A.Meta.Distortion == B.Meta.Distortion && A.Meta.Stability == B.Meta.Stability
            && A.Meta.Potency == B.Meta.Potency && A.Meta.Corruption == B.Meta.Corruption;
    }

    bool SameCellBase(const FCellBaseSnapshot& A, const FCellBaseSnapshot& B)
    {
        if (A.Weights.Num() != B.Weights.Num())
        {
            return false;
        }
        for (int32 Index = 0; Index < A.Weights.Num(); ++Index)
        {
            if (A.Weights[Index].Biome != B.Weights[Index].Biome || A.Weights[Index].Weight != B.Weights[Index].Weight)
            {
                return false;
            }
        }
        return A.Cell == B.Cell && A.Biome == B.Biome && A.bIsWater == B.bIsWater && A.WaterTypeID == B.WaterTypeID
            && SameMeta(A.State, B.State) && SameMeta(A.TargetState, B.TargetState)
            && A.HarvestStress == B.HarvestStress && A.bDegrading == B.bDegrading && A.Height == B.Height;
    }

    int32 TotalBiomeCells(const TMap<FName, FHerbalistBiomeFieldSum>& Sums)
    {
        int32 Count = 0;
        for (const TPair<FName, FHerbalistBiomeFieldSum>& Pair : Sums)
        {
            Count += Pair.Value.CellCount;
        }
        return Count;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistCellPageStreaming_PageUnloadsAndRebuildsFromBase,
    "Herbalist.WorldLayout.PageStreaming.PageUnloadsAndRebuildsFromBase",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistCellPageStreaming_PageUnloadsAndRebuildsFromBase::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World))
    {
        return false;
    }
    FScopedStreamingTestRadius ScopedRadius;
    AGridWorldManager* Manager = SpawnStreamingTestManager(World, 6300.0);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager))
    {
        return false;
    }
    if (!TestTrue(TEXT("Сетка 28 x 28, радиус 1 чанк"), Manager->GetGridCellCount() == 784 && Manager->GetActiveRadiusInChunks() == 1))
    {
        Manager->Destroy();
        return false;
    }

    // Снимок страницы (0, 0) до выгрузки.
    TArray<FCellBaseSnapshot> Before;
    for (int32 Y = 0; Y < 14; ++Y)
    {
        for (int32 X = 0; X < 14; ++X)
        {
            if (const FGridCell* Cell = Manager->GetCellConst(X, Y))
            {
                Before.Add(SnapshotCellBase(Manager, *Cell));
            }
        }
    }
    TestEqual(TEXT("Снята вся страница"), Before.Num(), 196);

    // Отклонения: тронутая клетка (13, 13) и проявление сущности в (12, 12) --
    // последнее пишется без пометки клетки.
    const FName TestEntity(TEXT("PageStreamingTestEntity"));
    const FGridCell* ToTouch = Manager->GetCellConst(13, 13);
    FGridCell* Manifested = Manager->GetCell(12, 12);
    if (!TestNotNull(TEXT("Клетка (13, 13)"), ToTouch) || !TestNotNull(TEXT("Клетка (12, 12)"), Manifested))
    {
        Manager->Destroy();
        return false;
    }
    FStateDelta Delta;
    FGridCell Touched = *ToTouch;
    Touched.State.Meta.Distortion = 0.77f;
    Delta.WorldChanges.Add(FIntPoint(13, 13), Touched);
    Manager->ApplyStateDelta(Delta);
    Manifested->ManifestedEntityID = TestEntity;

    // Земля только под западной страницей, зритель там же.
    StreamTo(Manager, GroundUnderWestPage, FIntPoint(-2, -2));
    TestEqual(TEXT("Загружена одна страница"), Manager->GetLoadedCellCount(), 196);
    TestNotNull(TEXT("Клетка (-14, -14) загружена"), Manager->GetCellConst(-14, -14));
    TestNull(TEXT("Клетка (13, 13) выгружена"), Manager->GetCellConst(13, 13));
    TestNull(TEXT("Клетка (0, 0) выгружена"), Manager->GetCellConst(0, 0));

    TestEqual(TEXT("Полные дельты -- только у тронутой клетки и проявления"), Manager->GetUnloadedCellDeltaCountForTests(), 2);

    const TArray<FSavedCellState> Captured = Manager->CaptureSaveCells();
    const FSavedCellState* TouchedSaved = Captured.FindByPredicate([](const FSavedCellState& Saved) { return Saved.X == 13 && Saved.Y == 13; });
    if (TestNotNull(TEXT("Сейв снимает тронутую клетку выгруженной страницы"), TouchedSaved))
    {
        TestEqual(TEXT("...с её состоянием"), TouchedSaved->State.Meta.Distortion, 0.77f, 1e-6f);
    }
    TestTrue(TEXT("Проявление в выгруженной странице видно"), Manager->IsLegendaryManifested(TestEntity));

    // Земля везде, зритель на востоке -- страницы собираются заново.
    StreamTo(Manager, GroundUnderWholeSmallGrid, FIntPoint(1, 1));
    TestEqual(TEXT("Загружены все страницы"), Manager->GetLoadedCellCount(), 784);

    const FGridCell* TouchedAfter = Manager->GetCellConst(13, 13);
    const FGridCell* ManifestedAfter = Manager->GetCellConst(12, 12);
    if (TestNotNull(TEXT("Клетка (13, 13) снова загружена"), TouchedAfter) && TestNotNull(TEXT("Клетка (12, 12) снова загружена"), ManifestedAfter))
    {
        TestEqual(TEXT("Тронутая клетка сохранила состояние"), TouchedAfter->State.Meta.Distortion, 0.77f, 1e-6f);
        TestTrue(TEXT("Проявление сохранилось"), ManifestedAfter->ManifestedEntityID == TestEntity);
    }

    int32 Mismatches = 0;
    for (const FCellBaseSnapshot& Snapshot : Before)
    {
        if (Snapshot.Cell == FIntPoint(13, 13) || Snapshot.Cell == FIntPoint(12, 12))
        {
            continue;
        }
        const FGridCell* Cell = Manager->GetCellConst(Snapshot.Cell.X, Snapshot.Cell.Y);
        if (!Cell || !SameCellBase(Snapshot, SnapshotCellBase(Manager, *Cell)))
        {
            ++Mismatches;
        }
    }
    TestEqual(TEXT("Нетронутые клетки страницы пересобраны один в один (биом, веса, вода, состояние, высота)"), Mismatches, 0);

    StopStreaming(Manager);
    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistCellPageStreaming_LoadedCellsDoNotGrowWithWorld,
    "Herbalist.WorldLayout.PageStreaming.LoadedCellsDoNotGrowWithWorld",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistCellPageStreaming_LoadedCellsDoNotGrowWithWorld::RunTest(const FString& Parameters)
{
    // Та же загруженная земля и тот же зритель на мире ±63 м и ±252 м: в памяти
    // одно и то же число клеток. Центр в чанке (-1, -1) задевает чанки -2..0 --
    // страницы -1 и 0 по обеим осям, в обеих сетках.
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World))
    {
        return false;
    }
    FScopedStreamingTestRadius ScopedRadius;

    const double HalfExtents[] = { 6300.0, 25200.0 };
    const int32 ExpectedGridCells[] = { 784, 3136 };
    for (int32 Case = 0; Case < 2; ++Case)
    {
        AGridWorldManager* Manager = SpawnStreamingTestManager(World, HalfExtents[Case]);
        if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager))
        {
            return false;
        }
        TestEqual(FString::Printf(TEXT("Сетка %d: всего клеток"), Case), Manager->GetGridCellCount(), ExpectedGridCells[Case]);
        StreamTo(Manager, GroundUnderWholeSmallGrid, FIntPoint(-1, -1));
        TestEqual(FString::Printf(TEXT("Сетка %d: в памяти четыре страницы -- 784 клетки"), Case), Manager->GetLoadedCellCount(), 784);
        StopStreaming(Manager);
        Manager->Destroy();
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistCellPageStreaming_SaveReachesUnloadedPage,
    "Herbalist.WorldLayout.PageStreaming.SaveReachesUnloadedPage",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistCellPageStreaming_SaveReachesUnloadedPage::RunTest(const FString& Parameters)
{
    // Загрузка сейва пишет клетку выгруженной страницы в её дельту, а не
    // отбрасывает; тронутая после сейва клетка выгруженной страницы
    // откатывается к основе.
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World))
    {
        return false;
    }
    FScopedStreamingTestRadius ScopedRadius;
    AGridWorldManager* Manager = SpawnStreamingTestManager(World, 6300.0);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager))
    {
        return false;
    }

    const FGridCell* Saved = Manager->GetCellConst(10, 10);
    const FGridCell* Rollback = Manager->GetCellConst(13, 13);
    if (!TestNotNull(TEXT("Клетка (10, 10)"), Saved) || !TestNotNull(TEXT("Клетка (13, 13)"), Rollback))
    {
        Manager->Destroy();
        return false;
    }
    FSavedCellState SavedCell = FSavedCellState();
    SavedCell.X = 10;
    SavedCell.Y = 10;
    SavedCell.State = Saved->State;
    SavedCell.State.Meta.Distortion = 0.33f;
    SavedCell.TargetState = Saved->TargetState;
    SavedCell.Memory = Saved->Memory;
    const float RollbackDistortionBefore = Rollback->State.Meta.Distortion;

    FStateDelta Delta;
    FGridCell Touched = *Rollback;
    Touched.State.Meta.Distortion = FMath::Frac(RollbackDistortionBefore + 0.5f);
    Delta.WorldChanges.Add(FIntPoint(13, 13), Touched);
    Manager->ApplyStateDelta(Delta);

    StreamTo(Manager, GroundUnderWestPage, FIntPoint(-2, -2));
    TestNull(TEXT("Страница (0, 0) выгружена"), Manager->GetCellConst(10, 10));

    TestEqual(TEXT("Клетка выгруженной страницы не отброшена"), Manager->ApplySaveCells({ SavedCell }), 0);
    const TArray<FSavedCellState> Captured = Manager->CaptureSaveCells();
    TestEqual(TEXT("Сейв снимает ровно загруженную из сейва клетку"), Captured.Num(), 1);
    TestTrue(TEXT("...это (10, 10)"), Captured.Num() == 1 && Captured[0].X == 10 && Captured[0].Y == 10);

    StreamTo(Manager, GroundUnderWholeSmallGrid, FIntPoint(1, 1));
    const FGridCell* SavedAfter = Manager->GetCellConst(10, 10);
    const FGridCell* RollbackAfter = Manager->GetCellConst(13, 13);
    if (TestNotNull(TEXT("Клетка (10, 10) загружена"), SavedAfter) && TestNotNull(TEXT("Клетка (13, 13) загружена"), RollbackAfter))
    {
        TestEqual(TEXT("Клетка из сейва легла в выгруженную страницу"), SavedAfter->State.Meta.Distortion, 0.33f, 1e-6f);
        TestEqual(TEXT("Тронутая после сейва клетка откатилась к основе"), RollbackAfter->State.Meta.Distortion, RollbackDistortionBefore, 1e-6f);
    }

    StopStreaming(Manager);
    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistCellPageStreaming_UnloadedChunksKeepTheirSummaries,
    "Herbalist.WorldLayout.PageStreaming.UnloadedChunksKeepTheirSummaries",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistCellPageStreaming_UnloadedChunksKeepTheirSummaries::RunTest(const FString& Parameters)
{
    // DESIGN §9: у выгруженной страницы -- последняя сводка. Биомный граф не
    // теряет клетки ушедших страниц, в том числе после загрузки сейва,
    // сбрасывающей кэш сводок.
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World))
    {
        return false;
    }
    FScopedStreamingTestRadius ScopedRadius;
    AGridWorldManager* Manager = SpawnStreamingTestManager(World, 6300.0);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager))
    {
        return false;
    }

    const TMap<FName, FHerbalistBiomeFieldSum> Before = Manager->GetBiomeFieldSums();
    TestEqual(TEXT("До выгрузки сводки видят все клетки"), TotalBiomeCells(Before), 784);

    auto ExpectSame = [this, &Before](const TCHAR* Stage, const TMap<FName, FHerbalistBiomeFieldSum>& After)
    {
        TestEqual(FString::Printf(TEXT("%s: клеток в сводках"), Stage), TotalBiomeCells(After), 784);
        for (const TPair<FName, FHerbalistBiomeFieldSum>& Pair : Before)
        {
            const FHerbalistBiomeFieldSum* Sum = After.Find(Pair.Key);
            if (TestNotNull(FString::Printf(TEXT("%s: биом %s"), Stage, *Pair.Key.ToString()), Sum))
            {
                TestEqual(FString::Printf(TEXT("%s, %s: клеток"), Stage, *Pair.Key.ToString()), Sum->CellCount, Pair.Value.CellCount);
                TestEqual(FString::Printf(TEXT("%s, %s: сумма Морока"), Stage, *Pair.Key.ToString()), Sum->MorokSum, Pair.Value.MorokSum, 1e-6);
            }
        }
    };

    StreamTo(Manager, GroundUnderWestPage, FIntPoint(-2, -2));
    TestEqual(TEXT("Загружена одна страница"), Manager->GetLoadedCellCount(), 196);
    ExpectSame(TEXT("После выгрузки"), Manager->GetBiomeFieldSums());

    Manager->ApplySaveCells({});
    ExpectSame(TEXT("После загрузки сейва"), Manager->GetBiomeFieldSums());

    StopStreaming(Manager);
    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistCellPageStreaming_SitePagesStayLoaded,
    "Herbalist.WorldLayout.PageStreaming.SitePagesStayLoaded",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistCellPageStreaming_SitePagesStayLoaded::RunTest(const FString& Parameters)
{
    // Хозяева мест, легендарные якоря и капища живут своей логикой вдали от
    // игрока -- их страницы не выгружаются.
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World))
    {
        return false;
    }
    FScopedStreamingTestRadius ScopedRadius;
    AGridWorldManager* Manager = SpawnStreamingTestManager(World, 6300.0);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager))
    {
        return false;
    }

    // Якорь легендарной сущности -- на странице (0, 0), ориентир -- на (0, -1).
    const FName AnchoredEntity(TEXT("PageStreamingAnchoredEntity"));
    Manager->SetLegendaryAnchorsForTests({ { AnchoredEntity, FIntPoint(12, 12) } });
    FEntityLandmark Landmark;
    Landmark.Cell = FIntPoint(5, -5);
    Manager->SetEntityLandmarks({ Landmark });
    if (FGridCell* AnchorCell = Manager->GetCell(12, 12))
    {
        AnchorCell->ManifestedEntityID = AnchoredEntity;
    }

    StreamTo(Manager, GroundUnderWestPage, FIntPoint(-2, -2));
    TestEqual(TEXT("Загружены страница зрителя и две закреплённые"), Manager->GetLoadedCellCount(), 196 * 3);
    TestNotNull(TEXT("Страница якоря (0, 0) загружена"), Manager->GetCellConst(12, 12));
    TestNotNull(TEXT("Страница ориентира (0, -1) загружена"), Manager->GetCellConst(5, -5));
    TestNull(TEXT("Страница (-1, 0) без мест выгружена"), Manager->GetCellConst(-5, 5));
    TestTrue(TEXT("Легендарная сущность с якорем видна"), Manager->IsLegendaryManifested(AnchoredEntity));

    StopStreaming(Manager);
    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistCellPageStreaming_RosterComesBackAsleep,
    "Herbalist.WorldLayout.PageStreaming.RosterComesBackAsleep",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistCellPageStreaming_RosterComesBackAsleep::RunTest(const FString& Parameters)
{
    // Нетронутая засеянная клетка выгружается без полной дельты: бит засева и
    // ростер. Страница, загруженная активным чанком без земли под ним,
    // возвращает ростер спящим -- до материализации акторов нет.
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World))
    {
        return false;
    }
    FScopedStreamingTestRadius ScopedRadius;
    AGridWorldManager* Manager = SpawnStreamingTestManager(World, 6300.0);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager))
    {
        return false;
    }

    const FName Herb(TEXT("PageStreamingTestHerb"));
    FGridCell* WithRoster = Manager->GetCell(10, 10);
    FGridCell* SeededEmpty = Manager->GetCell(11, 11);
    if (!TestNotNull(TEXT("Клетка (10, 10)"), WithRoster) || !TestNotNull(TEXT("Клетка (11, 11)"), SeededEmpty))
    {
        Manager->Destroy();
        return false;
    }
    WithRoster->bResourcesSeeded = true;
    WithRoster->DormantResourceIDs = { Herb };
    WithRoster->DormantResourceSlots = { 2 };
    SeededEmpty->bResourcesSeeded = true;

    StreamTo(Manager, GroundUnderWestPage, FIntPoint(-2, -2));
    TestNull(TEXT("Страница (0, 0) выгружена"), Manager->GetCellConst(10, 10));
    TestEqual(TEXT("Нетронутым засеянным клеткам полная дельта не нужна"), Manager->GetUnloadedCellDeltaCountForTests(), 0);

    // Зритель на востоке, земля по-прежнему только на западе: страница (0, 0)
    // грузится активным чанком, но не материализуется.
    StreamTo(Manager, GroundUnderWestPage, FIntPoint(1, 1));
    const FGridCell* WithRosterAfter = Manager->GetCellConst(10, 10);
    const FGridCell* SeededEmptyAfter = Manager->GetCellConst(11, 11);
    if (TestNotNull(TEXT("Клетка (10, 10) загружена"), WithRosterAfter) && TestNotNull(TEXT("Клетка (11, 11) загружена"), SeededEmptyAfter))
    {
        TestTrue(TEXT("Засев клетки с ростером сохранился"), WithRosterAfter->bResourcesSeeded);
        TestTrue(TEXT("Ростер вернулся спящим"), WithRosterAfter->DormantResourceIDs.Num() == 1 && WithRosterAfter->DormantResourceIDs[0] == Herb);
        TestTrue(TEXT("...на свой слот места"), WithRosterAfter->DormantResourceSlots.Num() == 1 && WithRosterAfter->DormantResourceSlots[0] == 2);
        TestEqual(TEXT("Акторов до материализации нет"), WithRosterAfter->ResourceActors.Num(), 0);
        TestTrue(TEXT("Засев пустой клетки сохранился -- заново не засеется"), SeededEmptyAfter->bResourcesSeeded);
    }

    StopStreaming(Manager);
    Manager->Destroy();
    return true;
}

#endif // WITH_AUTOMATION_TESTS && WITH_EDITOR
