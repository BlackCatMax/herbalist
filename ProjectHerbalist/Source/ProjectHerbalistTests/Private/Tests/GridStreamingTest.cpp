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
#include "Core/Resources/AHerbalistResourceActor.h"
#include "Core/Save/HerbalistSaveTypes.h"
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
        float SavedRadiusMeters;
        int32 SavedChunkSize;
        UHerbalistSettings* Settings;

        // Радиус задаётся в МЕТРАХ (см. ActiveSimulationRadiusMeters). В
        // тестах CellSize=100 (1 м на клетку), поэтому при ChunkSize=4 один
        // чанк -- это 4 метра: RadiusMeters=0 даёт «только свой чанк»,
        // RadiusMeters=4 -- «свой и соседние».
        FScopedChunkSettings(float RadiusMeters, int32 ChunkSize)
            : Settings(GetMutableDefault<UHerbalistSettings>())
        {
            SavedRadiusMeters = Settings->ActiveSimulationRadiusMeters;
            SavedChunkSize = Settings->ChunkSizeInCells;
            Settings->ActiveSimulationRadiusMeters = RadiusMeters;
            Settings->ChunkSizeInCells = ChunkSize;
        }
        ~FScopedChunkSettings()
        {
            Settings->ActiveSimulationRadiusMeters = SavedRadiusMeters;
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
        FScopedChunkSettings Scoped(/*RadiusMeters=*/-1.0f, /*ChunkSize=*/8);

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
        TestTrue(TEXT("Negative radius keeps every cell active"), bAllActive);
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
        // Чанк 4 клетки, радиус 4 м = 1 чанк -> активны чанки 0..1 по обеим осям,
        // то есть клетки 0..7. Клетка (0,0) активна, (19,19) -- нет.
        FScopedChunkSettings Scoped(/*RadiusMeters=*/4.0f, /*ChunkSize=*/4);
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
        FScopedChunkSettings Scoped(/*RadiusMeters=*/4.0f, /*ChunkSize=*/4);
        Manager->SetActiveChunkCentersForTests({});

        const FGridCell* Far = Manager->GetCell(19, 19);
        if (!TestNotNull(TEXT("Far cell exists"), Far)) { Manager->Destroy(); return false; }
        TestTrue(TEXT("Without any streaming source every cell stays active"), Manager->IsCellActive(*Far));
    }

    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistGridStreaming_CatchUpMatchesContinuousSimulation,
    "Herbalist.GridStreaming.CatchUpMatchesContinuousSimulation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistGridStreaming_CatchUpMatchesContinuousSimulation::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;

    {
        FScopedChunkSettings Scoped(/*RadiusMeters=*/0.0f, /*ChunkSize=*/4);

        // Две одинаково испорченные клетки в РАЗНЫХ чанках: (1,1) в чанке
        // (0,0), (17,17) в чанке (4,4). Первая будет активна всё время,
        // вторая -- только в конце, и должна догнать первую.
        FGridCell* Continuous = Manager->GetCell(1, 1);
        FGridCell* Streamed   = Manager->GetCell(17, 17);
        if (!Continuous || !Streamed) { Manager->Destroy(); return false; }

        for (FGridCell* C : { Continuous, Streamed })
        {
            C->State.Meta.Purity = 0.0f;
            C->TargetState.Meta.Purity = 1.0f;
            C->State.Meta.Distortion = 1.0f;
            C->TargetState.Meta.Distortion = 0.0f;
        }

        // 30 шагов по 1 секунде. Активен только чанк первой клетки.
        Manager->SetActiveChunkCentersForTests({ FIntPoint(0, 0) });
        Manager->CatchUpActivatedChunks();   // зафиксировать стартовое время чанка
        for (int32 i = 0; i < 30; ++i)
        {
            Manager->SetGameClockSeconds(Manager->GetGameClockSeconds() + 1.0f);
            Manager->RegenerateCellParameters(1.0f);
        }

        TestTrue(TEXT("Sanity: the continuously simulated cell moved"), Continuous->State.Meta.Purity > 0.0f);
        TestEqual(TEXT("Sanity: the streamed-out cell did not move at all"), Streamed->State.Meta.Purity, 0.0f);

        // Теперь игрок «пришёл» в дальний чанк -- он активируется и догоняет.
        Manager->SetActiveChunkCentersForTests({ FIntPoint(4, 4) });
        Manager->CatchUpActivatedChunks();

        // Догон должен дать ровно то же, что непрерывный прогон: релаксация
        // идёт через MoveToward (линейный шаг с остановкой у цели), поэтому
        // один шаг на 30 секунд равен тридцати шагам по секунде. Эпсилон --
        // только на накопление ошибки float в длинной цепочке шагов.
        TestTrue(FString::Printf(TEXT("Caught-up Purity %.5f matches continuous %.5f"),
                Streamed->State.Meta.Purity, Continuous->State.Meta.Purity),
            FMath::IsNearlyEqual(Streamed->State.Meta.Purity, Continuous->State.Meta.Purity, 1e-3f));
        TestTrue(FString::Printf(TEXT("Caught-up Distortion %.5f matches continuous %.5f"),
                Streamed->State.Meta.Distortion, Continuous->State.Meta.Distortion),
            FMath::IsNearlyEqual(Streamed->State.Meta.Distortion, Continuous->State.Meta.Distortion, 1e-3f));
    }

    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistGridStreaming_CatchUpDoesNotDoubleCountWhileChunkStaysActive,
    "Herbalist.GridStreaming.CatchUpDoesNotDoubleCountWhileChunkStaysActive",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistGridStreaming_CatchUpDoesNotDoubleCountWhileChunkStaysActive::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;

    {
        FScopedChunkSettings Scoped(/*RadiusMeters=*/0.0f, /*ChunkSize=*/4);
        Manager->SetActiveChunkCentersForTests({ FIntPoint(0, 0) });

        FGridCell* Cell = Manager->GetCell(1, 1);
        if (!Cell) { Manager->Destroy(); return false; }
        Cell->State.Meta.Purity = 0.0f;
        Cell->TargetState.Meta.Purity = 1.0f;

        // Чанк не менял активности -- повторный CatchUp не должен добавить
        // ни одного лишнего шага релаксации поверх обычного прохода.
        Manager->CatchUpActivatedChunks();
        Manager->SetGameClockSeconds(Manager->GetGameClockSeconds() + 10.0f);
        Manager->CatchUpActivatedChunks();

        TestEqual(TEXT("Staying active never triggers a catch-up step"), Cell->State.Meta.Purity, 0.0f);
    }

    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistGridStreaming_DormantChunkKeepsWhatGrewThere,
    "Herbalist.GridStreaming.DormantChunkKeepsWhatGrewThere",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistGridStreaming_DormantChunkKeepsWhatGrewThere::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;

    {
        FScopedChunkSettings Scoped(/*RadiusMeters=*/0.0f, /*ChunkSize=*/4);

        FGridCell* Cell = Manager->GetCell(1, 1);
        if (!Cell) { Manager->Destroy(); return false; }

        // Помечаем клетку тронутой публичным путём (ApplySaveCells делает это
        // сам) -- CaptureSaveCells пишет только тронутые, а нетронутый мир
        // намеренно переигрывается из RngBaseSeed и в сейв не идёт.
        {
            FSavedCellState Seed;
            Seed.X = 1;
            Seed.Y = 1;
            Manager->ApplySaveCells({ Seed });
        }

        // Ставим в клетку актор руками (реестра ингредиентов в тестовом мире
        // нет, поэтому обычный спавн ничего бы не дал) и помечаем клетку
        // заселённой -- иначе активация приняла бы её за нетронутую.
        const FVector Pos = Manager->GetCellWorldPosition(1, 1);
        AHerbalistResourceActor* Grown = World->SpawnActor<AHerbalistResourceActor>(
            AHerbalistResourceActor::StaticClass(), Pos, FRotator::ZeroRotator);
        if (!TestNotNull(TEXT("Resource actor spawned"), Grown)) { Manager->Destroy(); return false; }
        FRealState Dummy;
        Grown->Init(FName(TEXT("StreamTestHerb")), FText::GetEmpty(), nullptr, Dummy, Pos, Manager, 1, 1);
        Cell->bResourcesSeeded = true;

        TestEqual(TEXT("Cell starts with one live actor"), Cell->ResourceActors.Num(), 1);

        // Игрок ушёл: чанк усыпляется.
        Manager->SetChunkResourcesActive(FIntPoint(0, 0), false);

        TestEqual(TEXT("Live actors are gone once the chunk sleeps"), Cell->ResourceActors.Num(), 0);
        TestEqual(TEXT("What grew there is remembered"), Cell->DormantResourceIDs.Num(), 1);
        if (Cell->DormantResourceIDs.Num() == 1)
        {
            TestEqual(TEXT("Remembered by its ingredient ID"), Cell->DormantResourceIDs[0], FName(TEXT("StreamTestHerb")));
        }

        // Сохранение в этот момент обязано помнить спящее растение, иначе
        // сейв, сделанный вдали от дома, стирал бы дальний мир начисто.
        bool bFoundInSave = false;
        for (const FSavedCellState& Saved : Manager->CaptureSaveCells())
        {
            if (Saved.X == 1 && Saved.Y == 1 && Saved.ResourceIngredientIDs.Contains(FName(TEXT("StreamTestHerb"))))
            {
                bFoundInSave = true;
            }
        }
        TestTrue(TEXT("Dormant resource is written into the save"), bFoundInSave);
    }

    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistGridStreaming_SleepingChunkNeverTouchesPcgActors,
    "Herbalist.GridStreaming.SleepingChunkNeverTouchesPcgActors",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistGridStreaming_SleepingChunkNeverTouchesPcgActors::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;

    {
        FScopedChunkSettings Scoped(/*RadiusMeters=*/0.0f, /*ChunkSize=*/4);

        FGridCell* Cell = Manager->GetCell(2, 2);
        if (!Cell) { Manager->Destroy(); return false; }

        // Актор «из PCG-графа»: Init() не вызывался, значит сетка его не
        // спавнила. Регистрируется на клетке сам, через BeginPlay.
        const FVector Pos = Manager->GetCellWorldPosition(2, 2);
        AHerbalistResourceActor* FromPcg = World->SpawnActor<AHerbalistResourceActor>(
            AHerbalistResourceActor::StaticClass(), Pos, FRotator::ZeroRotator);
        if (!TestNotNull(TEXT("PCG-like actor spawned"), FromPcg)) { Manager->Destroy(); return false; }
        FromPcg->SetWorldManager(Manager);
        FromPcg->DispatchBeginPlay();

        TestFalse(TEXT("Sanity: this actor is not owned by the grid"), FromPcg->WasSpawnedByGrid());
        TestTrue(TEXT("Sanity: but it did register on its cell"), Cell->ResourceActors.Contains(FromPcg));

        Manager->SetChunkResourcesActive(FIntPoint(0, 0), false);

        // Главное: чужой актор пережил усыпление чанка и не уехал в
        // DormantResourceIDs -- иначе сетка «украла» бы его у PCG и при
        // следующей активации создала дубль.
        TestTrue(TEXT("PCG actor survives the chunk going to sleep"), IsValid(FromPcg));
        TestEqual(TEXT("PCG actor is not recorded as dormant"), Cell->DormantResourceIDs.Num(), 0);

        FromPcg->Destroy();
    }

    Manager->Destroy();
    return true;
}

#endif // WITH_AUTOMATION_TESTS && WITH_EDITOR
