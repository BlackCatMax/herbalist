// Source/ProjectHerbalistTests/Private/Tests/WorldStateWindowTest.cpp
//
// Разметка мира, этап 7 (2026-09-13) -- окно карты состояния мира. Без
// разметки окно -- вся сетка (это держат тесты WorldStateMapTest.cpp). Здесь
// сетка выведена из ландшафта с числами L_TestDev: 224 x 224 клеток по 9 м от
// (-112, -112), окно 128 x 128, тайл 16 клеток. Зрителя-игрока в мире
// редактора нет -- зритель берётся из центра чанка активности (чанк 7 клеток,
// центр чанка k -- клетка 7k + 3).

#include "Core/World/GridWorldManager.h"
#include "Core/World/WorldLayout.h"
#include "Core/Config/HerbalistSettings.h"
#include "Misc/AutomationTest.h"
#include "Editor.h"
#include "Engine/World.h"
#include "EngineUtils.h"

#if WITH_AUTOMATION_TESTS && WITH_EDITOR

namespace
{
    // Ландшафт L_TestDev (DESIGN_World_Layout.md §1): квад 1 м, компонент 126
    // квадов, ячейка стриминга 126 м, дальность 252 м, ±1008 м.
    FHerbalistWorldLayoutSource MakeTestDevSourceForWindowTest()
    {
        FHerbalistWorldLayoutSource Source;
        Source.bHasLandscape = true;
        Source.QuadSizeCm = 100.0;
        Source.ComponentSizeQuads = 126;
        Source.LandscapeOrigin = FVector2D(-100800.0, -100800.0);
        Source.LandscapeMin = FVector2D(-100800.0, -100800.0);
        Source.LandscapeMax = FVector2D(100800.0, 100800.0);
        Source.bHasStreamingGrid = true;
        Source.StreamingGridName = FName(TEXT("MainGrid"));
        Source.StreamingCellSizeCm = 12600.0;
        Source.StreamingLoadingRangeCm = 25200.0;
        Source.StreamingGridOrigin = FVector2D::ZeroVector;
        return Source;
    }

    struct FScopedWindowTestRadius
    {
        UHerbalistSettings* Settings;
        float SavedRadiusMeters;

        FScopedWindowTestRadius()
            : Settings(GetMutableDefault<UHerbalistSettings>())
        {
            SavedRadiusMeters = Settings->ActiveSimulationRadiusMeters;
            Settings->ActiveSimulationRadiusMeters = 100.0f;
        }

        ~FScopedWindowTestRadius()
        {
            Settings->ActiveSimulationRadiusMeters = SavedRadiusMeters;
        }
    };

    AGridWorldManager* SpawnTestDevManagerForWindowTest(UWorld* World)
    {
        for (TActorIterator<AGridWorldManager> It(World); It; ++It)
        {
            It->Destroy();
        }
        AGridWorldManager* Manager = World->SpawnActor<AGridWorldManager>();
        if (Manager)
        {
            Manager->BakedLayoutSource = MakeTestDevSourceForWindowTest();
            // Без таймера карты (как WorldStateMapTest): BeginPlay иначе
            // выгрузил бы настоящий RT_WorldStateMap и записал MPC мира
            // редактора, и окно было бы уже поставлено до теста.
            Manager->WorldStateMapUpdateIntervalSeconds = 0.0f;
            Manager->DispatchBeginPlay();
        }
        return Manager;
    }

    int32 PixelIndexForWindowTest(const FIntPoint& Cell, const FIntPoint& Min, const FIntPoint& Size)
    {
        return (Cell.Y - Min.Y) * Size.X + (Cell.X - Min.X);
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistWorldStateWindow_WindowIsSmallerThanGrid,
    "Herbalist.WorldLayout.Window.WindowIsSmallerThanGrid",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistWorldStateWindow_WindowIsSmallerThanGrid::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;
    FScopedWindowTestRadius ScopedRadius;
    AGridWorldManager* Manager = SpawnTestDevManagerForWindowTest(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;

    TestTrue(TEXT("Сетка 224 x 224"), Manager->GridSizeX == 224 && Manager->GridSizeY == 224);
    FIntPoint Min;
    FIntPoint Size;
    Manager->GetWorldStateWindow(Min, Size);
    TestTrue(TEXT("Окно 128 x 128"), Size == FIntPoint(128, 128));
    TestTrue(TEXT("Не поставленное окно -- по центру сетки, угол (-64, -64)"), Min == FIntPoint(-64, -64));
    TestEqual(TEXT("Пикселей -- по клетке окна"), Manager->BuildWorldStateMapPixels().Num(), 128 * 128);

    FVector Origin;
    FVector2D WorldSize;
    Manager->GetWorldStateMapFrame(Origin, WorldSize);
    TestTrue(FString::Printf(TEXT("Начало рамки -- угол клетки (-64, -64): (%.0f, %.0f) см"), Origin.X, Origin.Y),
        FVector2D(Origin).Equals(FVector2D(-57600.0, -57600.0), 0.01));
    TestTrue(TEXT("Рамка -- 128 клеток по 9 м"), WorldSize.Equals(FVector2D(115200.0, 115200.0), 0.01));

    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistWorldStateWindow_WindowFollowsViewerByTiles,
    "Herbalist.WorldLayout.Window.WindowFollowsViewerByTiles",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistWorldStateWindow_WindowFollowsViewerByTiles::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;
    FScopedWindowTestRadius ScopedRadius;
    AGridWorldManager* Manager = SpawnTestDevManagerForWindowTest(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;
    if (!TestEqual(TEXT("Чанк 7 клеток"), Manager->GetChunkSizeInCells(), 7)) { Manager->Destroy(); return false; }

    FIntPoint Min;
    FIntPoint Size;
    auto ExpectWindow = [this, Manager, &Min, &Size](const TCHAR* What, const FIntPoint& Expected)
    {
        Manager->GetWorldStateWindow(Min, Size);
        TestTrue(FString::Printf(TEXT("%s: угол (%d, %d), ждали (%d, %d)"), What, Min.X, Min.Y, Expected.X, Expected.Y), Min == Expected);
    };

    // Угол = ближайшее кратное 16 к (зритель - 64).
    // Чанк (0,0): зритель 3, 3 - 64 = -61 -> -64. Первая расстановка.
    Manager->SetActiveChunkCentersForTests({ FIntPoint(0, 0) });
    TestTrue(TEXT("Первая расстановка -- окно встаёт"), Manager->UpdateWorldStateWindow());
    ExpectWindow(TEXT("Зритель у начала сетки WP"), FIntPoint(-64, -64));
    TestFalse(TEXT("Тот же зритель -- окно стоит"), Manager->UpdateWorldStateWindow());

    // Чанк (1,1): зритель 10, от центра окна (0) на 10 клеток -- меньше тайла.
    Manager->SetActiveChunkCentersForTests({ FIntPoint(1, 1) });
    TestFalse(TEXT("Сдвиг меньше тайла -- окно не устарело"), Manager->IsWorldStateWindowStale());
    TestFalse(TEXT("...и стоит"), Manager->UpdateWorldStateWindow());

    // Чанк (2,2): зритель 17 > 16 -- окно устарело; 17 - 64 = -47 -> -48.
    Manager->SetActiveChunkCentersForTests({ FIntPoint(2, 2) });
    TestTrue(TEXT("Дальше тайла -- окно устарело"), Manager->IsWorldStateWindowStale());
    TestTrue(TEXT("...и переезжает"), Manager->UpdateWorldStateWindow());
    ExpectWindow(TEXT("Зритель в клетке 17"), FIntPoint(-48, -48));

    // Гистерезис: назад в чанк (1,1) -- зритель 10, центр окна 16, до него 6.
    Manager->SetActiveChunkCentersForTests({ FIntPoint(1, 1) });
    TestFalse(TEXT("Шаг назад через границу тайла -- окно не прыгает обратно"), Manager->UpdateWorldStateWindow());
    ExpectWindow(TEXT("После шага назад"), FIntPoint(-48, -48));

    // Несимметрично: чанк (2,-5) -- зритель (17, -32): X -> -48, Y -96 -> -96.
    Manager->SetActiveChunkCentersForTests({ FIntPoint(2, -5) });
    TestTrue(TEXT("Зритель к югу -- окно переезжает"), Manager->UpdateWorldStateWindow());
    ExpectWindow(TEXT("Зритель (17, -32)"), FIntPoint(-48, -96));

    // Отрицательный незажатый чанк (-3,-1): зритель (-18, -4): -82 -> -80, -68 -> -64.
    Manager->SetActiveChunkCentersForTests({ FIntPoint(-3, -1) });
    TestTrue(TEXT("Зритель к западу -- окно переезжает"), Manager->UpdateWorldStateWindow());
    ExpectWindow(TEXT("Зритель (-18, -4)"), FIntPoint(-80, -64));

    // Западный край: чанк (-15,-15) -- зритель -102, угол -160 зажат до -112.
    Manager->SetActiveChunkCentersForTests({ FIntPoint(-15, -15) });
    TestTrue(TEXT("Зритель у края -- окно переезжает"), Manager->UpdateWorldStateWindow());
    ExpectWindow(TEXT("Окно прижато к первой клетке сетки"), FIntPoint(-112, -112));

    // Прижатое окно не устаревает, пока новый угол тот же: чанк (-14,-14).
    Manager->SetActiveChunkCentersForTests({ FIntPoint(-14, -14) });
    TestFalse(TEXT("Зритель у края дальше тайла от центра, но угол тот же -- окно не устарело"), Manager->IsWorldStateWindowStale());

    // Восточный край: чанк (15,15) -- зритель 108, угол 48 зажат до -16.
    Manager->SetActiveChunkCentersForTests({ FIntPoint(15, 15) });
    TestTrue(TEXT("Зритель у восточного края -- окно переезжает"), Manager->UpdateWorldStateWindow());
    ExpectWindow(TEXT("Окно прижато к последней клетке сетки"), FIntPoint(-16, -16));

    // Без зрителя окно стоит, где стояло.
    Manager->SetActiveChunkCentersForTests({});
    TestFalse(TEXT("Без зрителя -- окно не устарело"), Manager->IsWorldStateWindowStale());
    TestFalse(TEXT("...и стоит"), Manager->UpdateWorldStateWindow());
    ExpectWindow(TEXT("Без зрителя"), FIntPoint(-16, -16));

    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistWorldStateWindow_UVAndPixelsAgreeWithCellsInWindow,
    "Herbalist.WorldLayout.Window.UVAndPixelsAgreeWithCellsInWindow",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistWorldStateWindow_UVAndPixelsAgreeWithCellsInWindow::RunTest(const FString& Parameters)
{
    // Тот же довод, что у WorldStateMapTest.UVAgreesWithWorldPositionToCell:
    // тексель, куда материал возьмёт точку, обязан быть клеткой симуляции.
    // Окно несимметрично (угол (-48, -96)): перестановка X и Y видна.
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;
    FScopedWindowTestRadius ScopedRadius;
    AGridWorldManager* Manager = SpawnTestDevManagerForWindowTest(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;

    Manager->SetActiveChunkCentersForTests({ FIntPoint(2, -5) });
    Manager->UpdateWorldStateWindow();
    FIntPoint Min;
    FIntPoint Size;
    Manager->GetWorldStateWindow(Min, Size);
    if (!TestTrue(TEXT("Окно с углом (-48, -96)"), Min == FIntPoint(-48, -96)))
    {
        Manager->Destroy();
        return false;
    }

    int32 Checked = 0;
    bool bAllAgree = true;
    const float Offsets[] = { 0.01f, 0.5f, 0.99f };
    for (int32 Y = Min.Y; Y < Min.Y + Size.Y; Y += 9)
    {
        for (int32 X = Min.X; X < Min.X + Size.X; X += 9)
        {
            for (float Frac : Offsets)
            {
                const FVector WorldPos = Manager->GetCellWorldPositionFlat(X, Y) + FVector(Frac * Manager->CellSize, Frac * Manager->CellSize, 0.0f);
                int32 SimX = 0;
                int32 SimY = 0;
                FVector2D UV;
                if (!Manager->WorldPositionToCell(WorldPos, SimX, SimY) || !Manager->GetWorldStateMapUV(WorldPos, UV))
                {
                    bAllAgree = false;
                    continue;
                }
                const int32 TexelX = FMath::FloorToInt(UV.X * Size.X);
                const int32 TexelY = FMath::FloorToInt(UV.Y * Size.Y);
                bAllAgree &= (Min.X + TexelX == SimX) && (Min.Y + TexelY == SimY);
                ++Checked;
            }
        }
    }
    TestTrue(TEXT("Проверено достаточно точек"), Checked > 100);
    TestTrue(TEXT("Каждая точка окна -- та же клетка в карте и в симуляции"), bAllAgree);

    FVector2D OutsideUV;
    TestFalse(TEXT("Клетка сетки за окном -- вне карты"),
        Manager->GetWorldStateMapUV(Manager->GetCellWorldPositionFlat(Min.X + Size.X + 1, Min.Y), OutsideUV));

    // Значение пикселя -- клетка, на которую он приходится; соседи по строке и
    // столбцу -- нули.
    for (int32 Y = Min.Y; Y < Min.Y + Size.Y; ++Y)
    {
        for (int32 X = Min.X; X < Min.X + Size.X; ++X)
        {
            if (FGridCell* Cell = Manager->GetCell(X, Y))
            {
                Cell->State.Meta.Distortion = 0.0f;
            }
        }
    }
    const FIntPoint Probe(5, -20);
    FGridCell* ProbeCell = Manager->GetCell(Probe.X, Probe.Y);
    if (TestNotNull(TEXT("Клетка (5, -20) есть"), ProbeCell))
    {
        ProbeCell->State.Meta.Distortion = 0.5f;
        Manager->SnapWorldStateMapDisplayToWorld();
        const TArray<FColor> Pixels = Manager->BuildWorldStateMapPixels();
        const int32 Index = PixelIndexForWindowTest(Probe, Min, Size);
        if (TestTrue(TEXT("Индекс в буфере"), Pixels.IsValidIndex(Index + Size.X) && Pixels.IsValidIndex(Index - Size.X)))
        {
            TestTrue(TEXT("Пиксель клетки (5, -20) несёт её Distortion 0.5"), FMath::Abs(int32(Pixels[Index].R) - 128) <= 1);
            TestTrue(TEXT("Соседи по строке и столбцу -- нули"),
                Pixels[Index - 1].R == 0 && Pixels[Index + 1].R == 0 && Pixels[Index - Size.X].R == 0 && Pixels[Index + Size.X].R == 0);
        }
    }

    Manager->SetActiveChunkCentersForTests({});
    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistWorldStateWindow_MoveKeepsOverlapSmoothing,
    "Herbalist.WorldLayout.Window.MoveKeepsOverlapSmoothing",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistWorldStateWindow_MoveKeepsOverlapSmoothing::RunTest(const FString& Parameters)
{
    // Переезд окна сдвигает показ: клетка, оставшаяся в окне, сохраняет
    // сглаженное значение, вошедшая -- приравнивается к миру. Рамка сдвигается
    // ровно на тайл.
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;
    FScopedWindowTestRadius ScopedRadius;
    AGridWorldManager* Manager = SpawnTestDevManagerForWindowTest(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;

    const FIntPoint Staying(0, 0);
    const FIntPoint Entering(70, 70);
    FGridCell* StayingCell = Manager->GetCell(Staying.X, Staying.Y);
    FGridCell* EnteringCell = Manager->GetCell(Entering.X, Entering.Y);
    if (!TestNotNull(TEXT("Клетка (0, 0)"), StayingCell) || !TestNotNull(TEXT("Клетка (70, 70)"), EnteringCell))
    {
        Manager->Destroy();
        return false;
    }

    Manager->SetActiveChunkCentersForTests({ FIntPoint(0, 0) });
    Manager->UpdateWorldStateWindow();
    FIntPoint Min;
    FIntPoint Size;
    Manager->GetWorldStateWindow(Min, Size);
    TestTrue(TEXT("Окно (-64, -64): клетка (70, 70) за ним"), Min == FIntPoint(-64, -64) && Entering.X >= Min.X + Size.X);
    FVector OriginBefore;
    FVector2D WorldSize;
    Manager->GetWorldStateMapFrame(OriginBefore, WorldSize);

    StayingCell->State.Meta.Distortion = 0.1f;
    EnteringCell->State.Meta.Distortion = 0.1f;
    Manager->SnapWorldStateMapDisplayToWorld();

    // Мир изменился, показ ещё нет.
    StayingCell->State.Meta.Distortion = 0.9f;
    EnteringCell->State.Meta.Distortion = 0.8f;

    Manager->SetActiveChunkCentersForTests({ FIntPoint(2, 2) });
    TestTrue(TEXT("Окно переезжает на тайл"), Manager->UpdateWorldStateWindow());
    Manager->AdvanceWorldStateMapDisplay(0.0f);
    Manager->GetWorldStateWindow(Min, Size);
    TestTrue(TEXT("Окно (-48, -48)"), Min == FIntPoint(-48, -48));

    const TArray<FColor> Pixels = Manager->BuildWorldStateMapPixels();
    const int32 StayingIndex = PixelIndexForWindowTest(Staying, Min, Size);
    const int32 EnteringIndex = PixelIndexForWindowTest(Entering, Min, Size);
    if (TestTrue(TEXT("Обе клетки в новом окне"), Pixels.IsValidIndex(StayingIndex) && Pixels.IsValidIndex(EnteringIndex)))
    {
        TestTrue(FString::Printf(TEXT("Оставшаяся клетка сохранила показ 0.1, а не мир 0.9 (R=%d)"), Pixels[StayingIndex].R),
            FMath::Abs(int32(Pixels[StayingIndex].R) - 26) <= 1);
        TestTrue(FString::Printf(TEXT("Вошедшая клетка приравнена к миру 0.8 (R=%d)"), Pixels[EnteringIndex].R),
            FMath::Abs(int32(Pixels[EnteringIndex].R) - 204) <= 1);
    }

    FVector OriginAfter;
    Manager->GetWorldStateMapFrame(OriginAfter, WorldSize);
    TestTrue(FString::Printf(TEXT("Рамка сдвинулась ровно на 16 клеток по 9 м: %.0f см"), OriginAfter.X - OriginBefore.X),
        FVector2D(OriginAfter - OriginBefore).Equals(FVector2D(14400.0, 14400.0), 0.01));

    Manager->SetActiveChunkCentersForTests({});
    Manager->Destroy();
    return true;
}

#endif // WITH_AUTOMATION_TESTS && WITH_EDITOR
