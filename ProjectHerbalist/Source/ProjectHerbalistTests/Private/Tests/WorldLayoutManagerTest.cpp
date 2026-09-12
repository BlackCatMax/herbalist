// Source/ProjectHerbalistTests/Private/Tests/WorldLayoutManagerTest.cpp
//
// Разметка мира, этап 1 (2026-09-12) -- на менеджере: незапечённый менеджер
// живёт по-старому, запечённые исходные величины при старте задают клетку,
// размер и угол сетки и чанк. Решатель сам по себе -- WorldLayoutSolverTest.cpp.

#include "Core/World/GridWorldManager.h"
#include "Core/World/WorldLayout.h"
#include "Core/Config/HerbalistSettings.h"
#include "Misc/AutomationTest.h"
#include "Editor.h"
#include "Engine/World.h"
#include "EngineUtils.h"

#if WITH_AUTOMATION_TESTS && WITH_EDITOR

#include "TestWorldHelpers.h"

namespace
{
    // Имя отличается от FScopedChunkSettings в GridStreamingTest.cpp: unity-
    // сборка склеивает тесты в одну единицу трансляции.
    struct FScopedLayoutRadiusSettings
    {
        UHerbalistSettings* Settings;
        float SavedRadiusMeters;
        int32 SavedChunkSize;

        explicit FScopedLayoutRadiusSettings(float RadiusMeters)
            : Settings(GetMutableDefault<UHerbalistSettings>())
        {
            SavedRadiusMeters = Settings->ActiveSimulationRadiusMeters;
            SavedChunkSize = Settings->ChunkSizeInCells;
            Settings->ActiveSimulationRadiusMeters = RadiusMeters;
        }

        ~FScopedLayoutRadiusSettings()
        {
            Settings->ActiveSimulationRadiusMeters = SavedRadiusMeters;
            Settings->ChunkSizeInCells = SavedChunkSize;
        }
    };

    // Маленький ландшафт с числами L_TestDev: квад 1 м, компонент 126 квадов,
    // ячейка стриминга 126 м, дальность 252 м, но всего ±63 м -- чтобы
    // сетка вышла 28 x 28 клеток, а не 224 x 224.
    FHerbalistWorldLayoutSource MakeSmallLandscapeSource()
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
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistWorldLayoutManager_UnbakedManagerKeepsManualGrid,
    "Herbalist.WorldLayout.Manager.UnbakedManagerKeepsManualGrid",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistWorldLayoutManager_UnbakedManagerKeepsManualGrid::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;
    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;

    const UHerbalistSettings* Settings = GetDefault<UHerbalistSettings>();
    TestFalse(TEXT("Без запечённого ландшафта разметка не выведена"), Manager->ResolvedLayout.bValid);
    TestEqual(TEXT("GridSizeX -- ручной, как до разметки"), Manager->GridSizeX, 20);
    TestEqual(TEXT("CellSize -- ручной"), Manager->CellSize, 100.0f, 0.001f);
    TestEqual(TEXT("Чанк -- из настроек"), Manager->GetChunkSizeInCells(), FMath::Max(1, Settings->ChunkSizeInCells));
    TestTrue(TEXT("Начало сетки -- положение актора, как до разметки"), Manager->GetGridOrigin().Equals(Manager->GetActorLocation()));
    TestTrue(TEXT("Незапечённый менеджер считается совпадающим с разметкой"), Manager->IsGridMatchingResolvedLayout());

    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistWorldLayoutManager_BakedSourceShapesGridAtBeginPlay,
    "Herbalist.WorldLayout.Manager.BakedSourceShapesGridAtBeginPlay",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistWorldLayoutManager_BakedSourceShapesGridAtBeginPlay::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    // Тот же довод, что у SpawnAndBeginPlay: чужие менеджеры в мире
    // перехватывали бы регистрацию акторов.
    for (TActorIterator<AGridWorldManager> It(World); It; ++It)
    {
        It->Destroy();
    }

    FScopedLayoutRadiusSettings Scoped(/*RadiusMeters=*/100.0f);

    AGridWorldManager* Manager = World->SpawnActor<AGridWorldManager>();
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;
    Manager->BakedLayoutSource = MakeSmallLandscapeSource();
    Manager->DispatchBeginPlay();

    TestTrue(TEXT("Разметка выведена при старте"), Manager->ResolvedLayout.bValid);
    TestEqual(TEXT("Клетка 9 м"), Manager->CellSize, 900.0f, 0.001f);
    TestEqual(TEXT("Сетка 28 клеток по X (две страницы по 14)"), Manager->GridSizeX, 28);
    TestEqual(TEXT("...и по Y"), Manager->GridSizeY, 28);
    TestEqual(TEXT("Угол сетки -- клетка -14 от начала отсчёта: -126 м по X"), Manager->GetGridOrigin().X, -12600.0, 0.01);
    TestEqual(TEXT("...и по Y"), Manager->GetGridOrigin().Y, -12600.0, 0.01);
    TestTrue(TEXT("Сам актор не двигается: у C++-менеджера нет корневого компонента"), Manager->GetActorLocation().IsNearlyZero());
    TestEqual(TEXT("Чанк из разметки: 7 клеток = 63 м"), Manager->GetChunkSizeInCells(), 7);
    TestEqual(TEXT("Радиус 100 м при чанке 63 м -- 1 чанк"), Manager->GetActiveRadiusInChunks(), 1);
    TestNotNull(TEXT("Клетки созданы по разметке: последняя (27, 27) есть"), Manager->GetCellConst(27, 27));
    TestNull(TEXT("...а (28, 0) -- уже нет"), Manager->GetCellConst(28, 0));

    // Клетка (0, 0) начинается в углу сетки: мировая точка чуть внутри неё
    // отвечает клеткой (0, 0), а не клеткой положения актора.
    int32 CellX = -1;
    int32 CellY = -1;
    const bool bInside = Manager->WorldPositionToCell(FVector(-12550.0, -12550.0, 0.0), CellX, CellY);
    TestTrue(TEXT("Точка у угла сетки -- внутри"), bInside && CellX == 0 && CellY == 0);
    TestTrue(TEXT("После старта поля совпадают с разметкой"), Manager->IsGridMatchingResolvedLayout());

    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistWorldLayoutManager_ChunkFollowsSimulationRadiusSetting,
    "Herbalist.WorldLayout.Manager.ChunkFollowsSimulationRadiusSetting",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistWorldLayoutManager_ChunkFollowsSimulationRadiusSetting::RunTest(const FString& Parameters)
{
    // Радиус симуляции -- ручная настройка (решение пользователя 3), а чанк
    // выводится под него при каждом пересчёте, не запекается.
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;
    AGridWorldManager* Manager = World->SpawnActor<AGridWorldManager>();
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;
    Manager->BakedLayoutSource = MakeSmallLandscapeSource();

    TArray<FString> Warnings;
    {
        FScopedLayoutRadiusSettings Scoped(/*RadiusMeters=*/100.0f);
        Manager->ResolveWorldLayout(Warnings);
        TestEqual(TEXT("R = 100 м -> чанк 7"), Manager->GetChunkSizeInCells(), 7);
    }
    {
        FScopedLayoutRadiusSettings Scoped(/*RadiusMeters=*/50.0f);
        Manager->ResolveWorldLayout(Warnings);
        TestEqual(TEXT("R = 50 м -> чанк 2"), Manager->GetChunkSizeInCells(), 2);
    }
    {
        FScopedLayoutRadiusSettings Scoped(/*RadiusMeters=*/-1.0f);
        Manager->ResolveWorldLayout(Warnings);
        TestEqual(TEXT("Стриминг сетки выключен -> чанк = страница 14"), Manager->GetChunkSizeInCells(), 14);
        TestEqual(TEXT("...и активно всё"), Manager->GetActiveRadiusInChunks(), -1);
    }

    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistWorldLayoutManager_ActiveRadiusUsesClampedRadius,
    "Herbalist.WorldLayout.Manager.ActiveRadiusUsesClampedRadius",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistWorldLayoutManager_ActiveRadiusUsesClampedRadius::RunTest(const FString& Parameters)
{
    // Найдено ревью: разметка урезала радиус до дальности загрузки, а
    // GetActiveRadiusInChunks брал радиус прямо из настроек. R = 400 м при
    // дальности 252 м: разметка выбирает чанк 14 (126 м) под 252 м -- 2 чанка;
    // из настроек вышло бы floor(400/126) = 3 чанка, дальше загруженных страниц.
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;
    AGridWorldManager* Manager = World->SpawnActor<AGridWorldManager>();
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;
    Manager->BakedLayoutSource = MakeSmallLandscapeSource();

    FScopedLayoutRadiusSettings Scoped(/*RadiusMeters=*/400.0f);
    TArray<FString> Warnings;
    Manager->ResolveWorldLayout(Warnings);
    Manager->ApplyResolvedLayout();

    TestEqual(TEXT("Чанк 14 клеток"), Manager->GetChunkSizeInCells(), 14);
    TestEqual(TEXT("Радиус в чанках -- от урезанных 252 м, а не от 400 м"), Manager->GetActiveRadiusInChunks(), 2);

    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistWorldLayoutManager_ManagerIsNotSpatiallyLoaded,
    "Herbalist.WorldLayout.Manager.ManagerIsNotSpatiallyLoaded",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistWorldLayoutManager_ManagerIsNotSpatiallyLoaded::RunTest(const FString& Parameters)
{
    // Найдено ревью: менеджер -- внешний актор карты World Partition, и с
    // пространственной загрузкой он выгружался бы вместе со своей ячейкой, как
    // только игрок уйдёт дальше дальности загрузки, -- вместе со всей
    // симуляцией мира.
    const AGridWorldManager* Defaults = GetDefault<AGridWorldManager>();
    TestFalse(TEXT("Менеджер не загружается пространственно"), Defaults->GetIsSpatiallyLoaded());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistWorldLayoutManager_GatherReadsEditorWorld,
    "Herbalist.WorldLayout.Manager.GatherReadsEditorWorld",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistWorldLayoutManager_GatherReadsEditorWorld::RunTest(const FString& Parameters)
{
    // Сбор из настоящего мира редактора. На L_TestDev числа известны: они
    // прочитаны из ассетов карты (DESIGN_World_Layout.md §1). На другой
    // стартовой карте проверяется только то, что сбор не падает.
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    TArray<FString> Warnings;
    const FHerbalistWorldLayoutSource Source = AGridWorldManager::GatherWorldLayoutSource(World, Warnings);
    AddInfo(FString::Printf(TEXT("Карта %s: ландшафт %d, квад %.1f см, компонент %d, вершина (%.1f, %.1f), границы (%.1f, %.1f)..(%.1f, %.1f); стриминг %d, разбиение %s, ячейка %.1f, дальность %.1f, начало (%.1f, %.1f)"),
        *World->GetOutermost()->GetName(), Source.bHasLandscape, Source.QuadSizeCm, Source.ComponentSizeQuads,
        Source.LandscapeOrigin.X, Source.LandscapeOrigin.Y, Source.LandscapeMin.X, Source.LandscapeMin.Y,
        Source.LandscapeMax.X, Source.LandscapeMax.Y, Source.bHasStreamingGrid, *Source.StreamingGridName.ToString(),
        Source.StreamingCellSizeCm, Source.StreamingLoadingRangeCm, Source.StreamingGridOrigin.X, Source.StreamingGridOrigin.Y));
    for (const FString& Warning : Warnings)
    {
        AddInfo(Warning);
    }

    if (!World->GetOutermost()->GetName().Contains(TEXT("L_TestDev")))
    {
        AddInfo(TEXT("Стартовая карта -- не L_TestDev, точные числа не проверяются"));
        return true;
    }

    TestTrue(TEXT("Ландшафт найден"), Source.bHasLandscape);
    TestEqual(TEXT("Квад 1 м"), Source.QuadSizeCm, 100.0, 0.01);
    TestEqual(TEXT("Компонент 126 квадов"), Source.ComponentSizeQuads, 126);
    TestEqual(TEXT("Вершина (0,0) по X в -1008 м"), Source.LandscapeOrigin.X, -100800.0, 0.5);
    TestEqual(TEXT("...по Y"), Source.LandscapeOrigin.Y, -100800.0, 0.5);
    TestEqual(TEXT("Граница ландшафта: min X -1008 м"), Source.LandscapeMin.X, -100800.0, 1.0);
    TestEqual(TEXT("...min Y -1008 м"), Source.LandscapeMin.Y, -100800.0, 1.0);
    TestEqual(TEXT("...max X +1008 м"), Source.LandscapeMax.X, 100800.0, 1.0);
    TestEqual(TEXT("...max Y +1008 м"), Source.LandscapeMax.Y, 100800.0, 1.0);
    TestTrue(TEXT("Сетка стриминга найдена"), Source.bHasStreamingGrid);
    TestEqual(TEXT("Ячейка стриминга 126 м"), Source.StreamingCellSizeCm, 12600.0, 0.01);
    TestEqual(TEXT("Дальность загрузки 252 м"), Source.StreamingLoadingRangeCm, 25200.0, 0.01);
    return true;
}

#endif // WITH_AUTOMATION_TESTS && WITH_EDITOR
