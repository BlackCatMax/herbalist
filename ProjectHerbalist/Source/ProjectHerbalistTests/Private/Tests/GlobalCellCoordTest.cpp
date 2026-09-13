// Source/ProjectHerbalistTests/Private/Tests/GlobalCellCoordTest.cpp
//
// Разметка мира, этап 6 (2026-09-13) -- глобальная координата клетки. Cell.X,
// Cell.Y и все функции с (X, Y) -- координаты от начала сетки World Partition
// (решение пользователя 13); массивы клеток -- локальный индекс. У менеджера
// автотеста разметки нет, и начало сетки там (0,0), поэтому весь прочий набор
// тестов сдвига не видит. Здесь сетка выведена из маленького ландшафта и
// начинается с клетки (-14, -14).

#include "Core/World/GridWorldManager.h"
#include "Core/World/WorldLayout.h"
#include "Core/Config/HerbalistSettings.h"
#include "Core/Save/HerbalistSaveTypes.h"
#include "Core/Resources/AHerbalistResourceActor.h"
#include "Misc/AutomationTest.h"
#include "Editor.h"
#include "Engine/World.h"
#include "EngineUtils.h"

#if WITH_AUTOMATION_TESTS && WITH_EDITOR

namespace
{
    // Тот же ландшафт, что в WorldLayoutManagerTest.cpp: квад 1 м, компонент
    // 126 квадов, ячейка стриминга 126 м, дальность 252 м, границы ±63 м.
    // Разметка: клетка 9 м, сетка 28 x 28 от клетки (-14, -14), чанк 7 клеток
    // при радиусе 100 м. Имя своё: unity-сборка склеивает тесты.
    FHerbalistWorldLayoutSource MakeGlobalCoordTestSource()
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
        return Source;
    }

    struct FScopedGlobalCoordRadius
    {
        UHerbalistSettings* Settings;
        float SavedRadiusMeters;

        FScopedGlobalCoordRadius()
            : Settings(GetMutableDefault<UHerbalistSettings>())
        {
            SavedRadiusMeters = Settings->ActiveSimulationRadiusMeters;
            Settings->ActiveSimulationRadiusMeters = 100.0f;
        }

        ~FScopedGlobalCoordRadius()
        {
            Settings->ActiveSimulationRadiusMeters = SavedRadiusMeters;
        }
    };

    AGridWorldManager* SpawnShiftedGridManager(UWorld* World)
    {
        // Чужие менеджеры перехватывали бы регистрацию акторов (довод у
        // SpawnAndBeginPlay).
        for (TActorIterator<AGridWorldManager> It(World); It; ++It)
        {
            It->Destroy();
        }
        AGridWorldManager* Manager = World->SpawnActor<AGridWorldManager>();
        if (Manager)
        {
            Manager->BakedLayoutSource = MakeGlobalCoordTestSource();
            Manager->DispatchBeginPlay();
        }
        return Manager;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistGlobalCoord_GridStartsAtLayoutMinCell,
    "Herbalist.WorldLayout.GlobalCoord.GridStartsAtLayoutMinCell",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistGlobalCoord_GridStartsAtLayoutMinCell::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;
    FScopedGlobalCoordRadius ScopedRadius;
    AGridWorldManager* Manager = SpawnShiftedGridManager(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;
    if (!TestTrue(TEXT("Разметка выведена"), Manager->ResolvedLayout.bValid)) { Manager->Destroy(); return false; }

    TestTrue(TEXT("Первая клетка сетки -- (-14, -14)"), Manager->GetGridMinCell() == FIntPoint(-14, -14));

    auto CheckCell = [this, Manager](int32 X, int32 Y)
    {
        const FGridCell* Cell = Manager->GetCellConst(X, Y);
        if (TestNotNull(FString::Printf(TEXT("Клетка (%d, %d) есть"), X, Y), Cell))
        {
            TestTrue(FString::Printf(TEXT("У клетки (%d, %d) те же координаты"), X, Y), Cell->X == X && Cell->Y == Y);
        }
    };
    CheckCell(-14, -14);
    CheckCell(13, 13);
    CheckCell(-1, -1);
    CheckCell(0, 0);
    TestNull(TEXT("(14, 0) -- за сеткой"), Manager->GetCellConst(14, 0));
    TestNull(TEXT("(-15, 0) -- за сеткой"), Manager->GetCellConst(-15, 0));
    TestNull(TEXT("(0, 14) -- за сеткой"), Manager->GetCellConst(0, 14));
    TestNull(TEXT("InvalidCell -- не клетка и не переполнение"), Manager->GetCellConst(HerbalistCore::InvalidCellCoord, HerbalistCore::InvalidCellCoord));

    // Мировая точка клетки -- её угол. Угол первой клетки -- угол сетки, клетка
    // (0,0) -- начало сетки World Partition.
    TestTrue(TEXT("Угол клетки (-14, -14) -- угол сетки"),
        Manager->GetCellWorldPositionFlat(-14, -14).Equals(Manager->GetGridOrigin(), 0.01));
    TestTrue(TEXT("Угол клетки (0, 0) -- начало сетки World Partition"),
        FVector2D(Manager->GetCellWorldPositionFlat(0, 0)).Equals(FVector2D::ZeroVector, 0.01));

    int32 X = 0;
    int32 Y = 0;
    TestTrue(TEXT("Точка (-4.5 м, -4.5 м) -- клетка (-1, -1)"),
        Manager->WorldPositionToCell(FVector(-450.0, -450.0, 0.0), X, Y) && X == -1 && Y == -1);
    TestTrue(TEXT("Точка (4.5 м, 4.5 м) -- клетка (0, 0)"),
        Manager->WorldPositionToCell(FVector(450.0, 450.0, 0.0), X, Y) && X == 0 && Y == 0);
    TestFalse(TEXT("Точка (126.5 м, 0) -- за сеткой"), Manager->WorldPositionToCell(FVector(12650.0, 0.0, 0.0), X, Y));

    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistGlobalCoord_ChunksCountFromGlobalZero,
    "Herbalist.WorldLayout.GlobalCoord.ChunksCountFromGlobalZero",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistGlobalCoord_ChunksCountFromGlobalZero::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;
    FScopedGlobalCoordRadius ScopedRadius;
    AGridWorldManager* Manager = SpawnShiftedGridManager(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;
    if (!TestEqual(TEXT("Чанк 7 клеток"), Manager->GetChunkSizeInCells(), 7)) { Manager->Destroy(); return false; }

    TestTrue(TEXT("Клетка (-1, -1) -- чанк (-1, -1)"), Manager->GetChunkCoordForCell(-1, -1) == FIntPoint(-1, -1));
    TestTrue(TEXT("Клетка (-7, -7) -- чанк (-1, -1)"), Manager->GetChunkCoordForCell(-7, -7) == FIntPoint(-1, -1));
    TestTrue(TEXT("Клетка (-8, -8) -- чанк (-2, -2)"), Manager->GetChunkCoordForCell(-8, -8) == FIntPoint(-2, -2));
    TestTrue(TEXT("Клетка (-14, -14) -- чанк (-2, -2)"), Manager->GetChunkCoordForCell(-14, -14) == FIntPoint(-2, -2));
    TestTrue(TEXT("Клетка (13, 13) -- чанк (1, 1)"), Manager->GetChunkCoordForCell(13, 13) == FIntPoint(1, 1));
    TestTrue(TEXT("Точка (-4.5 м, -4.5 м) -- чанк (-1, -1)"), Manager->WorldPositionToChunk(FVector(-450.0, -450.0, 0.0)) == FIntPoint(-1, -1));

    FIntPoint MinChunk;
    FIntPoint MaxChunk;
    Manager->GetGridChunkRange(MinChunk, MaxChunk);
    TestTrue(TEXT("Чанки сетки -- от (-2, -2)"), MinChunk == FIntPoint(-2, -2));
    TestTrue(TEXT("...до (1, 1)"), MaxChunk == FIntPoint(1, 1));

    int32 Visited = 0;
    bool bAllInside = true;
    Manager->ForEachCellInChunk(FIntPoint(-1, -1), [&Visited, &bAllInside](FGridCell& Cell)
    {
        ++Visited;
        bAllInside &= Cell.X >= -7 && Cell.X <= -1 && Cell.Y >= -7 && Cell.Y <= -1;
    });
    TestEqual(TEXT("В чанке (-1, -1) 7 x 7 клеток"), Visited, 49);
    TestTrue(TEXT("...и все они от -7 до -1"), bAllInside);

    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistGlobalCoord_ActiveCellsAroundCentreBeyondEdge,
    "Herbalist.WorldLayout.GlobalCoord.ActiveCellsAroundCentreBeyondEdge",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistGlobalCoord_ActiveCellsAroundCentreBeyondEdge::RunTest(const FString& Parameters)
{
    // Центр активности за западным краем (чанк -3) с радиусом 1 чанк задевает
    // чанки -4..-2; в сетке из них только (-2, -2). Раньше отсечение шло по
    // 0..Max, и отрицательные чанки отбрасывались бы целиком.
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;
    FScopedGlobalCoordRadius ScopedRadius;
    AGridWorldManager* Manager = SpawnShiftedGridManager(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;
    if (!TestEqual(TEXT("Радиус 1 чанк"), Manager->GetActiveRadiusInChunks(), 1)) { Manager->Destroy(); return false; }

    Manager->SetActiveChunkCentersForTests({ FIntPoint(-3, -3) });
    int32 Visited = 0;
    bool bAllInChunk = true;
    Manager->ForEachActiveCell([&Visited, &bAllInChunk](FGridCell& Cell)
    {
        ++Visited;
        bAllInChunk &= Cell.X >= -14 && Cell.X <= -8 && Cell.Y >= -14 && Cell.Y <= -8;
    });
    TestEqual(TEXT("Активен ровно чанк (-2, -2): 49 клеток"), Visited, 49);
    TestTrue(TEXT("...от -14 до -8"), bAllInChunk);

    const FGridCell* Corner = Manager->GetCellConst(-14, -14);
    const FGridCell* Far = Manager->GetCellConst(0, 0);
    TestTrue(TEXT("Угловая клетка активна"), Corner && Manager->IsCellActive(*Corner));
    TestTrue(TEXT("Клетка (0, 0) -- нет"), Far && !Manager->IsCellActive(*Far));

    Manager->SetActiveChunkCentersForTests({});
    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistGlobalCoord_SavedCellRoundTripsOnItsGlobalCell,
    "Herbalist.WorldLayout.GlobalCoord.SavedCellRoundTripsOnItsGlobalCell",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistGlobalCoord_SavedCellRoundTripsOnItsGlobalCell::RunTest(const FString& Parameters)
{
    // Сейв хранит координаты клеток: клетка (-1, -1) ложится в (-1, -1), и
    // сейв снимает её с той же координатой.
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;
    FScopedGlobalCoordRadius ScopedRadius;
    AGridWorldManager* Manager = SpawnShiftedGridManager(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;

    const FGridCell* Target = Manager->GetCellConst(-1, -1);
    const FGridCell* ArrayNeighbour = Manager->GetCellConst(-2, -1);   // предыдущий элемент массива
    if (!TestNotNull(TEXT("Клетка (-1, -1) есть"), Target) || !TestNotNull(TEXT("Клетка (-2, -1) есть"), ArrayNeighbour))
    {
        Manager->Destroy();
        return false;
    }

    const float MarkedPurity = 0.123456f;
    const float NeighbourPurityBefore = ArrayNeighbour->State.Meta.Purity;

    FSavedCellState Saved;
    Saved.X = -1;
    Saved.Y = -1;
    Saved.State = Target->State;
    Saved.State.Meta.Purity = MarkedPurity;
    Saved.TargetState = Target->TargetState;
    Saved.Memory = Target->Memory;
    Saved.bResourcesSeeded = Target->bResourcesSeeded;
    Manager->ApplySaveCells({ Saved });

    TestEqual(TEXT("Сохранённая клетка легла в (-1, -1)"), Manager->GetCellConst(-1, -1)->State.Meta.Purity, MarkedPurity, 1e-6f);
    TestEqual(TEXT("Соседний по массиву (-2, -1) не тронут"), Manager->GetCellConst(-2, -1)->State.Meta.Purity, NeighbourPurityBefore, 1e-6f);

    const TArray<FSavedCellState> Captured = Manager->CaptureSaveCells();
    const FSavedCellState* Round = Captured.FindByPredicate([](const FSavedCellState& Cell) { return Cell.X == -1 && Cell.Y == -1; });
    if (TestNotNull(TEXT("Сейв снимает клетку (-1, -1) с той же координатой"), Round))
    {
        TestEqual(TEXT("...и с её состоянием"), Round->State.Meta.Purity, MarkedPurity, 1e-6f);
    }

    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistGlobalCoord_MapAndResourceUseGlobalCells,
    "Herbalist.WorldLayout.GlobalCoord.MapAndResourceUseGlobalCells",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistGlobalCoord_MapAndResourceUseGlobalCells::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;
    FScopedGlobalCoordRadius ScopedRadius;
    AGridWorldManager* Manager = SpawnShiftedGridManager(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;

    // Карта состояния: пиксель 0 -- угловая клетка (-14, -14), а не (0, 0).
    for (int32 Y = -14; Y < 14; ++Y)
    {
        for (int32 X = -14; X < 14; ++X)
        {
            Manager->GetCell(X, Y)->State.Meta.Distortion = 0.0f;
        }
    }
    Manager->GetCell(-14, -14)->State.Meta.Distortion = 1.0f;
    Manager->SnapWorldStateMapDisplayToWorld();
    const TArray<FColor> Pixels = Manager->BuildWorldStateMapPixels();
    if (TestEqual(TEXT("Карта -- по пикселю на клетку сетки"), Pixels.Num(), 28 * 28))
    {
        TestEqual(TEXT("Пиксель 0 -- клетка (-14, -14)"), int32(Pixels[0].R), 255);
        TestEqual(TEXT("Пиксель 1 -- клетка (-13, -14)"), int32(Pixels[1].R), 0);
    }
    FVector2D UV;
    TestTrue(TEXT("Точка у угла сетки -- UV у нуля"),
        Manager->GetWorldStateMapUV(Manager->GetGridOrigin() + FVector(1.0, 1.0, 0.0), UV) && UV.X < 0.01 && UV.Y < 0.01);

    // Ресурс, поставленный в мир без Init (PCG, уровень), сам находит свою
    // глобальную клетку. Ресурсы клетки лежат вокруг её угла (разброс
    // GetSpawnPositionWithinBiome), поэтому клетка -- ближайший угол: точка
    // на 0.3 клетки к востоку и к югу от угла (-5, 3) -- его ресурс.
    const FVector Place = Manager->GetCellWorldPositionFlat(-5, 3) + FVector(Manager->CellSize * 0.3f, -Manager->CellSize * 0.3f, 0.0f);
    AHerbalistResourceActor* Resource = World->SpawnActor<AHerbalistResourceActor>(AHerbalistResourceActor::StaticClass(), Place, FRotator::ZeroRotator);
    if (TestNotNull(TEXT("Ресурс заспавнен"), Resource))
    {
        Resource->DispatchBeginPlay();
        TestTrue(TEXT("Ресурс нашёл клетку (-5, 3)"), Resource->GetGridX() == -5 && Resource->GetGridY() == 3);
        Resource->Destroy();
    }

    Manager->Destroy();
    return true;
}

#endif // WITH_AUTOMATION_TESTS && WITH_EDITOR
