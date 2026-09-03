// Source/ProjectHerbalistTests/Private/Tests/GridStreamingTest.cpp
//
// Стриминг сетки, юнит 1 (2026-09-03): активное множество чанков. Данные
// всех клеток остаются в памяти всегда -- стримится СТОИМОСТЬ, а не данные,
// поэтому мировые сканы (посев якорей, условие Буяна, проверка
// проявленности Легендарного) продолжают видеть весь мир. Активность решает
// только одно: считаются ли на клетке дорогие проходы.
//
// Проверяется три вещи: арифметика чанков, выключенное по умолчанию
// состояние (радиус -1 = активно всё, поведение как до механизма) и само
// гейтирование релаксации, когда радиус задан.

#include "Core/World/GridWorldManager.h"
#include "Core/Config/HerbalistSettings.h"
#include "Misc/AutomationTest.h"
#include "Editor.h"
#include "Engine/World.h"

#if WITH_AUTOMATION_TESTS && WITH_EDITOR

#include "TestWorldHelpers.h"

namespace
{
    // Настройки -- UDeveloperSettings, синглтон на процесс: меняем на время
    // теста и обязательно возвращаем, иначе следующий тест в прогоне получит
    // чужой радиус (тот же класс ловушки, что уже ловили с изоляцией мира).
    struct FScopedChunkSettings
    {
        int32 SavedRadius;
        int32 SavedChunkSize;
        UHerbalistSettings* Settings;

        FScopedChunkSettings(int32 Radius, int32 ChunkSize)
            : Settings(GetMutableDefault<UHerbalistSettings>())
        {
            SavedRadius = Settings->ActiveChunkRadius;
            SavedChunkSize = Settings->ChunkSizeInCells;
            Settings->ActiveChunkRadius = Radius;
            Settings->ChunkSizeInCells = ChunkSize;
        }
        ~FScopedChunkSettings()
        {
            Settings->ActiveChunkRadius = SavedRadius;
            Settings->ChunkSizeInCells = SavedChunkSize;
        }
    };
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistGridStreaming_ChunkCoordMathAndDefaultAllActive,
    "Herbalist.GridStreaming.ChunkCoordMathAndDefaultAllActive",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistGridStreaming_ChunkCoordMathAndDefaultAllActive::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;

    {
        FScopedChunkSettings Scoped(/*Radius=*/-1, /*ChunkSize=*/8);

        TestEqual(TEXT("Cell (0,0) belongs to chunk (0,0)"), Manager->GetChunkCoordForCell(0, 0), FIntPoint(0, 0));
        TestEqual(TEXT("Cell (7,7) is still chunk (0,0) at chunk size 8"), Manager->GetChunkCoordForCell(7, 7), FIntPoint(0, 0));
        TestEqual(TEXT("Cell (8,0) rolls into chunk (1,0)"), Manager->GetChunkCoordForCell(8, 0), FIntPoint(1, 0));
        TestEqual(TEXT("Cell (17,9) is chunk (2,1)"), Manager->GetChunkCoordForCell(17, 9), FIntPoint(2, 1));

        // Радиус -1 -- механизм выключен: активны все клетки, что бы ни
        // стояло в центрах. Это дефолт, и именно поэтому включение
        // стриминга остаётся осознанным шагом, а не тихой сменой симуляции.
        Manager->SetActiveChunkCentersForTests({ FIntPoint(0, 0) });
        bool bAllActive = true;
        for (int32 Y = 0; Y < Manager->GridSizeY; ++Y)
        {
            for (int32 X = 0; X < Manager->GridSizeX; ++X)
            {
                const FGridCell* Cell = Manager->GetCell(X, Y);
                if (Cell && !Manager->IsCellActive(*Cell)) { bAllActive = false; break; }
            }
        }
        TestTrue(TEXT("Radius -1 keeps every cell active"), bAllActive);
    }

    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistGridStreaming_RadiusGatesCellsByChunkDistance,
    "Herbalist.GridStreaming.RadiusGatesCellsByChunkDistance",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistGridStreaming_RadiusGatesCellsByChunkDistance::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;

    {
        // Чанк 4 клетки, радиус 1 -> активны чанки 0..1 по обеим осям,
        // то есть клетки 0..7. Клетка (0,0) активна, (19,19) -- нет.
        FScopedChunkSettings Scoped(/*Radius=*/1, /*ChunkSize=*/4);
        Manager->SetActiveChunkCentersForTests({ FIntPoint(0, 0) });

        const FGridCell* Near = Manager->GetCell(0, 0);
        const FGridCell* Edge = Manager->GetCell(7, 7);
        const FGridCell* Far  = Manager->GetCell(19, 19);
        if (!TestNotNull(TEXT("Cells exist"), Near) || !Edge || !Far) { Manager->Destroy(); return false; }

        TestTrue(TEXT("Cell in the centre chunk is active"), Manager->IsCellActive(*Near));
        TestTrue(TEXT("Cell in the neighbouring chunk is active (radius 1)"), Manager->IsCellActive(*Edge));
        TestFalse(TEXT("Cell four chunks away is not active"), Manager->IsCellActive(*Far));

        // И главное: неактивная клетка действительно не считается. Портим
        // обеим клеткам состояние одинаково и прогоняем релаксацию.
        FGridCell* NearMut = Manager->GetCell(0, 0);
        FGridCell* FarMut  = Manager->GetCell(19, 19);
        NearMut->State.Meta.Purity = 0.0f;
        NearMut->TargetState.Meta.Purity = 1.0f;
        FarMut->State.Meta.Purity = 0.0f;
        FarMut->TargetState.Meta.Purity = 1.0f;

        for (int32 i = 0; i < 20; ++i)
        {
            Manager->RegenerateCellParameters(1.0f);
        }

        TestTrue(TEXT("Active cell relaxes towards its target"), NearMut->State.Meta.Purity > 0.0f);
        TestEqual(TEXT("Inactive cell is left untouched"), FarMut->State.Meta.Purity, 0.0f);
    }

    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistGridStreaming_NoCentresMeansEverythingActive,
    "Herbalist.GridStreaming.NoCentresMeansEverythingActive",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistGridStreaming_NoCentresMeansEverythingActive::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;

    {
        // Радиус задан, но источников стриминга нет (нет игрока, нет
        // партишена -- ровно ситуация headless-теста). Сознательный выбор:
        // считать всё активным, а не всё мёртвым. Тихо остановившаяся
        // симуляция -- худший из отказов, её никто не заметит.
        FScopedChunkSettings Scoped(/*Radius=*/1, /*ChunkSize=*/4);
        Manager->SetActiveChunkCentersForTests({});

        const FGridCell* Far = Manager->GetCell(19, 19);
        if (!TestNotNull(TEXT("Far cell exists"), Far)) { Manager->Destroy(); return false; }
        TestTrue(TEXT("Without any streaming source every cell stays active"), Manager->IsCellActive(*Far));
    }

    Manager->Destroy();
    return true;
}

#endif // WITH_AUTOMATION_TESTS && WITH_EDITOR
