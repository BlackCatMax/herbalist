// Source/ProjectHerbalistTests/Private/Tests/CellPageTest.cpp
//
// Разметка мира, этап 8 (2026-09-13) -- клетки хранятся страницами
// (DESIGN_World_Layout.md §6). Пока все страницы загружены, и поведение
// обязано совпадать с единым массивом: те же клетки, тот же порядок обхода
// (от него зависит расход WorldRNG при посеве мест), те же высоты и снимки
// для отката. Без разметки страница одна.

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

#include "TestWorldHelpers.h"

namespace
{
    struct FScopedPageTestRadius
    {
        UHerbalistSettings* Settings;
        float SavedRadiusMeters;

        FScopedPageTestRadius()
            : Settings(GetMutableDefault<UHerbalistSettings>())
        {
            SavedRadiusMeters = Settings->ActiveSimulationRadiusMeters;
            Settings->ActiveSimulationRadiusMeters = 100.0f;
        }

        ~FScopedPageTestRadius()
        {
            Settings->ActiveSimulationRadiusMeters = SavedRadiusMeters;
        }
    };

    // Ландшафт ±63 м с числами L_TestDev: клетка 9 м, страница 14 клеток,
    // сетка 28 x 28 от (-14, -14) -- четыре страницы 14 x 14.
    AGridWorldManager* SpawnShiftedManagerForPageTest(UWorld* World)
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

    // Обход идёт строками сетки: i-я клетка -- (MinX + i % Width, MinY + i / Width).
    bool WalkFollowsGridRows(AGridWorldManager* Manager, int32& OutVisited)
    {
        const FIntPoint GridMin = Manager->GetGridMinCell();
        const int32 Width = Manager->GridSizeX;
        bool bInOrder = true;
        OutVisited = 0;
        Manager->ForEachCell([&bInOrder, &OutVisited, GridMin, Width](const FGridCell& Cell)
        {
            bInOrder &= Cell.X == GridMin.X + OutVisited % Width && Cell.Y == GridMin.Y + OutVisited / Width;
            ++OutVisited;
        });
        return bInOrder;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistCellPage_PagesTileShiftedGrid,
    "Herbalist.WorldLayout.Pages.PagesTileShiftedGrid",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistCellPage_PagesTileShiftedGrid::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;
    FScopedPageTestRadius ScopedRadius;
    AGridWorldManager* Manager = SpawnShiftedManagerForPageTest(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;
    if (!TestTrue(TEXT("Сетка 28 x 28 от (-14, -14), страница 14"),
        Manager->GetGridMinCell() == FIntPoint(-14, -14) && Manager->GridSizeX == 28 && Manager->ResolvedLayout.PageSizeInCells == 14))
    {
        Manager->Destroy();
        return false;
    }

    TestEqual(TEXT("Четыре страницы"), Manager->GetCellPageCountForTests(), 4);
    TestEqual(TEXT("Все 784 клетки загружены"), Manager->GetLoadedCellCount(), 28 * 28);

    // Каждая клетка -- в странице, начинающейся с кратного 14 (деление вниз),
    // размером 14 x 14, и адресуется по своей координате.
    bool bAllInPage = true;
    bool bAllAddressed = true;
    for (int32 Y = -14; Y < 14; ++Y)
    {
        for (int32 X = -14; X < 14; ++X)
        {
            FIntPoint PageMin;
            FIntPoint PageSize;
            const FIntPoint ExpectedMin(X < 0 ? -14 : 0, Y < 0 ? -14 : 0);
            bAllInPage &= Manager->GetCellPageBoundsForTests(X, Y, PageMin, PageSize) && PageMin == ExpectedMin && PageSize == FIntPoint(14, 14);
            const FGridCell* Cell = Manager->GetCellConst(X, Y);
            bAllAddressed &= Cell && Cell->X == X && Cell->Y == Y;
        }
    }
    TestTrue(TEXT("Каждая клетка -- в своей странице 14 x 14 от кратного 14"), bAllInPage);
    TestTrue(TEXT("Каждая клетка адресуется по своей координате"), bAllAddressed);

    FIntPoint PageMin;
    FIntPoint PageSize;
    TestFalse(TEXT("У клетки за сеткой страницы нет"), Manager->GetCellPageBoundsForTests(14, 0, PageMin, PageSize));
    TestNull(TEXT("Индекс сетки за концом -- не клетка"), Manager->GetCellByGridIndex(28 * 28));
    TestNull(TEXT("Отрицательный индекс -- не клетка"), Manager->GetCellByGridIndex(-1));

    // Соседи через границу страниц -- разные страницы, та же адресация.
    const FGridCell* West = Manager->GetCellConst(-1, 0);
    const FGridCell* East = Manager->GetCellConst(0, 0);
    TestTrue(TEXT("Клетки (-1, 0) и (0, 0) по разные стороны границы страниц"), West && East && West != East && West->X == -1 && East->X == 0);

    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistCellPage_WalkKeepsGridRowOrder,
    "Herbalist.WorldLayout.Pages.WalkKeepsGridRowOrder",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistCellPage_WalkKeepsGridRowOrder::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    {
        FScopedPageTestRadius ScopedRadius;
        AGridWorldManager* Shifted = SpawnShiftedManagerForPageTest(World);
        if (!TestNotNull(TEXT("Сдвинутая сетка"), Shifted)) return false;
        int32 Visited = 0;
        TestTrue(TEXT("Со страницами обход идёт строками сетки, а не по страницам"), WalkFollowsGridRows(Shifted, Visited));
        TestEqual(TEXT("...все 784 клетки по разу"), Visited, 28 * 28);

        bool bIndexMatches = true;
        for (int32 Index = 0; Index < Shifted->GetGridCellCount(); ++Index)
        {
            const FGridCell* Cell = Shifted->GetCellByGridIndex(Index);
            bIndexMatches &= Cell && Cell->X == -14 + Index % 28 && Cell->Y == -14 + Index / 28;
        }
        TestTrue(TEXT("Индекс сетки -- та же строка и столбец"), bIndexMatches);

        // Ветка «активно всё» (центров нет) идёт тем же обходом.
        Shifted->SetActiveChunkCentersForTests({});
        int32 ActiveVisited = 0;
        bool bActiveInOrder = true;
        Shifted->ForEachActiveCell([&ActiveVisited, &bActiveInOrder](FGridCell& Cell)
        {
            bActiveInOrder &= Cell.X == -14 + ActiveVisited % 28 && Cell.Y == -14 + ActiveVisited / 28;
            ++ActiveVisited;
        });
        TestTrue(TEXT("Без центров активности активные клетки идут строками сетки"), bActiveInOrder && ActiveVisited == 28 * 28);
        Shifted->Destroy();
    }

    // Без разметки -- одна страница во всю сетку.
    AGridWorldManager* Plain = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("Сетка без разметки"), Plain)) return false;
    TestEqual(TEXT("Без разметки страница одна"), Plain->GetCellPageCountForTests(), 1);
    TestEqual(TEXT("...и в ней вся сетка"), Plain->GetLoadedCellCount(), Plain->GridSizeX * Plain->GridSizeY);
    int32 Visited = 0;
    TestTrue(TEXT("Обход строками сетки"), WalkFollowsGridRows(Plain, Visited));
    TestEqual(TEXT("...все клетки по разу"), Visited, Plain->GridSizeX * Plain->GridSizeY);
    Plain->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistCellPage_BaselinesLiveWithTheirPages,
    "Herbalist.WorldLayout.Pages.BaselinesLiveWithTheirPages",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistCellPage_BaselinesLiveWithTheirPages::RunTest(const FString& Parameters)
{
    // Снимок клетки после инициализации лежит в её странице: загрузка сейва
    // без клетки откатывает тронутую клетку к нему в любой странице.
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;
    FScopedPageTestRadius ScopedRadius;
    AGridWorldManager* Manager = SpawnShiftedManagerForPageTest(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;

    const TArray<FIntPoint> Probes = { FIntPoint(-14, -14), FIntPoint(13, -14), FIntPoint(-14, 13), FIntPoint(13, 13) };
    TMap<FIntPoint, float> DistortionBefore;
    FStateDelta Delta;
    for (const FIntPoint& Coord : Probes)
    {
        const FGridCell* Original = Manager->GetCellConst(Coord.X, Coord.Y);
        if (!TestNotNull(FString::Printf(TEXT("Клетка (%d, %d)"), Coord.X, Coord.Y), Original))
        {
            Manager->Destroy();
            return false;
        }
        DistortionBefore.Add(Coord, Original->State.Meta.Distortion);
        FGridCell Changed = *Original;
        Changed.State.Meta.Distortion = FMath::Frac(Original->State.Meta.Distortion + 0.5f);
        Delta.WorldChanges.Add(Coord, Changed);
    }
    Manager->ApplyStateDelta(Delta);
    TestEqual(TEXT("Тронуты четыре клетки в четырёх страницах"), Manager->CaptureSaveCells().Num(), Probes.Num());

    // Снимок каждой клетки -- её собственный (перепутанный индекс страницы и
    // сетки дал бы снимок соседа того же биома с тем же Distortion).
    bool bBaselinesMatchCells = true;
    for (int32 Index = 0; Index < Manager->GetGridCellCount(); ++Index)
    {
        FIntPoint BaselineCell;
        bBaselinesMatchCells &= Manager->GetCellBaselineCellForTests(Index, BaselineCell)
            && BaselineCell == FIntPoint(-14 + Index % 28, -14 + Index / 28);
    }
    TestTrue(TEXT("Снимок каждой из 784 клеток снят с неё самой"), bBaselinesMatchCells);

    Manager->ApplySaveCells({});
    for (const FIntPoint& Coord : Probes)
    {
        TestEqual(FString::Printf(TEXT("Клетка (%d, %d) откатилась к снимку своей страницы"), Coord.X, Coord.Y),
            Manager->GetCellConst(Coord.X, Coord.Y)->State.Meta.Distortion, DistortionBefore[Coord], 1e-6f);
    }
    TestEqual(TEXT("После отката тронутых нет"), Manager->CaptureSaveCells().Num(), 0);

    TestEqual(TEXT("Высота клетки за сеткой -- 0"), Manager->GetCellHeight(14, 0), 0.0f);

    Manager->Destroy();
    return true;
}

#endif // WITH_AUTOMATION_TESTS && WITH_EDITOR
