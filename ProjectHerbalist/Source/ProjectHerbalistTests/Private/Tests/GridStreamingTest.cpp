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
#include "Core/Entities/HerbalistEntityActor.h"
#include "Core/Entities/LandmarkTypes.h"
#include "Core/Save/HerbalistSaveTypes.h"
#include "Core/Types/BiomeTypes.h"
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

// Регрессия 2026-09-03: разбор жалобы на низкую производительность на
// масштабе 500x500 показал, что RegenerateCellParameters/ApplyBiomeInfluences/
// UpdateEntityManifestations честно проверяли IsCellActive перед дорогой
// работой, но САМ обход по-прежнему шёл `for (Cell : Cells) { if (!Active)
// continue; ... }` -- то есть трогал ВСЕ 250 000 клеток каждый тик, просто
// чтобы отбраковать почти все. ForEachActiveCell идёт прямо по активным
// чанкам, не касаясь остальной сетки.
//
// Первая версия ForEachActiveCell переиспользовала TSet ActiveChunks --
// кэш, который заполняет только CatchUpActivatedChunks() (обычно вызывается
// из Tick перед этими тремя функциями). Это сломало
// RadiusGatesCellsByChunkDistance выше: тот тест задаёт ActiveChunkCenters
// напрямую и сразу зовёт RegenerateCellParameters, БЕЗ Tick -- легитимный
// сценарий (изолированное юнит-тестирование релаксации), и ActiveChunks в
// нём оставался пустым. ForEachActiveCell молча обрабатывал ноль клеток.
// Правка: своя геометрия (ComputeChunksWithinRadius), не зависящая от
// того, прогонялся ли в этом кадре Tick. Этот тест — прямая регрессия на
// тот же сценарий, но уже для самого ForEachActiveCell, а не только для
// функции, которая его использует.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistGridStreaming_ForEachActiveCellDoesNotDependOnTick,
    "Herbalist.GridStreaming.ForEachActiveCellDoesNotDependOnTick",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistGridStreaming_ForEachActiveCellDoesNotDependOnTick::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;

    {
        // Те же параметры, что и в RadiusGatesCellsByChunkDistance выше:
        // чанк 4 клетки, радиус 4 м = 1 чанк -> активны чанки (0,0)/(0,1)/
        // (1,0)/(1,1) = блок 8x8 = 64 клетки из 400 в сетке 20x20.
        FScopedChunkSettings Scoped(/*RadiusMeters=*/4.0f, /*ChunkSize=*/4);
        Manager->SetActiveChunkCentersForTests({ FIntPoint(0, 0) });

        // НЕ вызываем Tick()/CatchUpActivatedChunks() -- намеренно, это и
        // есть проверяемый сценарий (см. комментарий выше).
        int32 VisitedCount = 0;
        TSet<FIntPoint> VisitedCells;
        Manager->ForEachActiveCell([&](FGridCell& Cell)
        {
            ++VisitedCount;
            VisitedCells.Add(FIntPoint(Cell.X, Cell.Y));
        });

        TestEqual(TEXT("Exactly the 8x8 block of active chunks was visited, not all 400 cells of the grid"), VisitedCount, 64);

        // Перекрёстная проверка с IsCellActive -- независимо давно
        // работающим методом, той же клетка-за-клеткой семантикой.
        bool bAllMatch = true;
        for (int32 Y = 0; Y < Manager->GridSizeY; ++Y)
        {
            for (int32 X = 0; X < Manager->GridSizeX; ++X)
            {
                const FGridCell* Cell = Manager->GetCell(X, Y);
                const bool bShouldBeActive = Cell && Manager->IsCellActive(*Cell);
                const bool bWasVisited = VisitedCells.Contains(FIntPoint(X, Y));
                if (bShouldBeActive != bWasVisited)
                {
                    bAllMatch = false;
                    AddError(FString::Printf(TEXT("Cell (%d,%d): IsCellActive=%s, visited=%s"),
                        X, Y, bShouldBeActive ? TEXT("true") : TEXT("false"), bWasVisited ? TEXT("true") : TEXT("false")));
                }
            }
        }
        TestTrue(TEXT("ForEachActiveCell visits exactly the cells IsCellActive agrees with"), bAllMatch);
    }

    Manager->Destroy();
    return true;
}

// Найдено пользователем 2026-09-03 при ручной проверке 0.8: "отлично
// режется чанками растительность, но не деревья-заглушки для entities и
// прочих". Ресурсы усыпляются SetChunkResourcesActive (уже покрыт тестами
// выше), у проявленных сущностей (капище-заглушки, бестиарий) такой
// деактивации не было вовсе -- SyncManifestedEntityActor вызывается
// ТОЛЬКО изнутри UpdateEntityManifestations, а деактивированный чанк
// просто перестаёт через него проходить. Актор оставался в мире
// бессрочно, сколько бы игрок ни отходил.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistGridStreaming_DespawnChunkEntitiesDestroysActorButKeepsID,
    "Herbalist.GridStreaming.DespawnChunkEntitiesDestroysActorButKeepsID",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistGridStreaming_DespawnChunkEntitiesDestroysActorButKeepsID::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;

    FGridCell* Cell = Manager->GetCell(0, 0);
    if (!TestNotNull(TEXT("Cell exists"), Cell)) { Manager->Destroy(); return false; }

    // Симулируем то, что обычно делает SyncManifestedEntityActor (она
    // protected, тестируем через публичный DespawnChunkEntities и прямую
    // подготовку клетки -- тот же приём, что уже у RadiusGatesCellsByChunkDistance
    // выше, которая тоже готовит клетку напрямую, не гоняя весь пайплайн триггеров).
    AHerbalistEntityActor* Placeholder = World->SpawnActor<AHerbalistEntityActor>();
    if (!TestNotNull(TEXT("Placeholder actor spawned"), Placeholder)) { Manager->Destroy(); return false; }

    Cell->ManifestedEntityID = FName(TEXT("TestSpirit"));
    Cell->ManifestedEntityActor = Placeholder;

    // Чанк 4 клетки -- клетка (0,0) в чанке (0,0), тот же расклад, что
    // ChunkCoordMathAndDefaultAllActive выше.
    FScopedChunkSettings Scoped(/*RadiusMeters=*/4.0f, /*ChunkSize=*/4);
    Manager->DespawnChunkEntities(FIntPoint(0, 0));

    TestFalse(TEXT("Placeholder actor was destroyed"), IsValid(Placeholder));
    TestFalse(TEXT("Cell no longer references the destroyed actor"), Cell->ManifestedEntityActor.IsValid());
    TestEqual(TEXT("ManifestedEntityID survives despawn -- it's data, not presence"),
        Cell->ManifestedEntityID, FName(TEXT("TestSpirit")));

    Manager->Destroy();
    return true;
}

// Аудит 2026-09-05: тот же класс бага, что уже чинился 2026-09-03 выше
// (DespawnChunkEntitiesDestroysActorButKeepsID) для СТОРОНЫ ДЕАКТИВАЦИИ, но
// найденный теперь для СТОРОНЫ СПАВНА. Низший ранг/per-клеточный Легендарный
// (Гнильники/Берегиня) уже гейтятся -- живут внутри ForEachActiveCell,
// физически не обходят неактивные чанки. Основной ранг (Домовой/Полевик и
// т.п., обход EntityLandmarks) и Легендарный ранг по якорным клеткам
// (обход LegendaryAnchors) — фиксированные списки, не ForEachActiveCell:
// SyncManifestedEntityActor звался для них безусловно, независимо от
// чанков, поэтому Домовой мог воскреснуть в чанке, который игрок уже
// покинул. Сама ЛОГИКА проявления (bWasActive/Respect/гистерезис) осталась
// негейченной намеренно -- она путь-зависима и должна идти своим чередом
// даже вдали от игрока (тот же принцип, что и у ManifestedEntityID,
// переживающего DespawnChunkEntities); гейтится только материализация
// актора.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistGridStreaming_LandmarkManifestationDoesNotSpawnActorOutsideActiveRadius,
    "Herbalist.GridStreaming.LandmarkManifestationDoesNotSpawnActorOutsideActiveRadius",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistGridStreaming_LandmarkManifestationDoesNotSpawnActorOutsideActiveRadius::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;

    FGridCell* Cell = Manager->GetCell(2, 2);
    if (!TestNotNull(TEXT("Cell (2,2) exists"), Cell)) { Manager->Destroy(); return false; }
    Cell->Biome = EBiomeType::ForestSteppe;
    Cell->bIsWater = false;

    // Respect уже высок -- тот же приём прямой установки, что уже
    // LandmarkTest.cpp (Полевик благословляется при Respect >= 0.5).
    FEntityLandmark Landmark;
    Landmark.EntityID = FName(TEXT("Полевик"));
    Landmark.Cell = FIntPoint(2, 2);
    Landmark.Respect = 0.8f;
    Manager->SetEntityLandmarks({ Landmark });

    // Чанк 4 клетки, радиус 0 -- активен только "свой" чанк центра. Центр
    // ДАЛЕКО от (2,2) -- клетка вне активного радиуса.
    FScopedChunkSettings Scoped(/*RadiusMeters=*/0.0f, /*ChunkSize=*/4);
    Manager->SetActiveChunkCentersForTests({ FIntPoint(50, 50) });

    Manager->UpdateEntityManifestations(1.0f);

    TestEqual(TEXT("ManifestedEntityID всё равно обновляется -- логика проявления не гейтится чанками"),
        Cell->ManifestedEntityID, FName(TEXT("Полевик")));
    TestFalse(TEXT("Но актор НЕ заспавнен -- клетка вне активного радиуса"),
        Cell->ManifestedEntityActor.IsValid());

    // Игрок подходит -- центр активности теперь накрывает (2,2) (чанк (0,0)
    // при ChunkSize=4).
    Manager->SetActiveChunkCentersForTests({ FIntPoint(0, 0) });
    Manager->UpdateEntityManifestations(1.0f);

    TestTrue(TEXT("Актор материализуется, как только чанк становится активным"),
        Cell->ManifestedEntityActor.IsValid());

    Manager->Destroy();
    return true;
}

#endif // WITH_AUTOMATION_TESTS && WITH_EDITOR
