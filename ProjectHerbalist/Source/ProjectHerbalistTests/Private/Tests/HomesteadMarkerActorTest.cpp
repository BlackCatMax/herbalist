// Source/ProjectHerbalistTests/Private/Tests/HomesteadMarkerActorTest.cpp
//
// "Обставление" (ROADMAP.md, 2026-09-06) — RegisterGardenPlot/RegisterBase
// теперь спавнят AHomesteadMarkerActor, тем же классом решения, что уже
// POI-акторы/AKurganActor. Домашнее хранилище не тестируется здесь — у
// него уже свой актор (AStorageContainer, AGridWorldManager::
// SpawnHomeStorageContainer), не новая находка этого захода.

#include "Core/World/GridWorldManager.h"
#include "Core/World/HomesteadMarkerActor.h"
#include "Misc/AutomationTest.h"
#include "Editor.h"
#include "Engine/World.h"
#include "EngineUtils.h"

#if WITH_AUTOMATION_TESTS && WITH_EDITOR

#include "TestWorldHelpers.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistHomestead_RegisteringGardenPlotSpawnsAndUpdatesMarker,
    "Herbalist.Homestead.RegisteringGardenPlotSpawnsAndUpdatesMarker",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistHomestead_RegisteringGardenPlotSpawnsAndUpdatesMarker::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;

    const FIntPoint PlotCell(1, 1);
    Manager->RegisterGardenPlot(PlotCell, EGardenNiche::Mycelium);

    AHomesteadMarkerActor* Marker = nullptr;
    for (TActorIterator<AHomesteadMarkerActor> It(World); It; ++It)
    {
        if (It->GetKind() == EHomesteadMarkerKind::GardenPristroyka && It->GetGridCell() == PlotCell)
        {
            Marker = *It;
            break;
        }
    }
    if (!TestNotNull(TEXT("Marker spawned for the new plot"), Marker)) { Manager->Destroy(); return false; }
    TestEqual(TEXT("Marker reflects the registered niche"), Marker->Niche, EGardenNiche::Mycelium);

    // Смена ниши на той же клетке -- обновляет тот же актор, не спавнит второй.
    Manager->RegisterGardenPlot(PlotCell, EGardenNiche::Cave);
    int32 MarkerCountAtCell = 0;
    for (TActorIterator<AHomesteadMarkerActor> It(World); It; ++It)
    {
        if (It->GetKind() == EHomesteadMarkerKind::GardenPristroyka && It->GetGridCell() == PlotCell) ++MarkerCountAtCell;
    }
    TestEqual(TEXT("Still exactly one marker at that cell after a niche change"), MarkerCountAtCell, 1);
    TestEqual(TEXT("Marker's Niche updated in place"), Marker->Niche, EGardenNiche::Cave);

    // Снятие пристройки (None) -- уничтожает маркер.
    Manager->RegisterGardenPlot(PlotCell, EGardenNiche::None);
    TestFalse(TEXT("Marker destroyed after clearing the plot"), IsValid(Marker));

    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistHomestead_RegisteringBaseSpawnsMarker,
    "Herbalist.Homestead.RegisteringBaseSpawnsMarker",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistHomestead_RegisteringBaseSpawnsMarker::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;

    FGridCell* Cell = Manager->GetCell(4, 4);
    if (!TestNotNull(TEXT("Cell (4,4) exists"), Cell)) { Manager->Destroy(); return false; }
    Cell->bIsWater = false;

    const FIntPoint BaseCell(4, 4);
    Manager->RegisterBase(BaseCell);

    AHomesteadMarkerActor* Marker = nullptr;
    for (TActorIterator<AHomesteadMarkerActor> It(World); It; ++It)
    {
        if (It->GetKind() == EHomesteadMarkerKind::Base && It->GetGridCell() == BaseCell)
        {
            Marker = *It;
            break;
        }
    }
    TestNotNull(TEXT("Marker spawned for the new base"), Marker);

    Manager->Destroy();
    return true;
}

namespace
{
    int32 CountHomesteadMarkers(UWorld* World, EHomesteadMarkerKind Kind, const FIntPoint& Cell, EGardenNiche* OutNiche = nullptr)
    {
        int32 Count = 0;
        for (TActorIterator<AHomesteadMarkerActor> It(World); It; ++It)
        {
            if (It->GetKind() == Kind && It->GetGridCell() == Cell)
            {
                ++Count;
                if (OutNiche) *OutNiche = It->Niche;
            }
        }
        return Count;
    }
}

// Загрузка сейва присваивает GardenPlots и Bases напрямую -- маркеры
// приводит в порядок SyncHomesteadMarkers (2026-09-22; до этого после
// загрузки построек не было видно).
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistHomestead_SyncRebuildsMarkersFromState,
    "Herbalist.Homestead.SyncRebuildsMarkersFromState",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistHomestead_SyncRebuildsMarkersFromState::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;

    const FIntPoint PlotCell(7, 7);
    const FIntPoint StalePlotCell(8, 7);
    const FIntPoint BaseCell(7, 8);

    // Живая сессия до загрузки: пристройка, которой в сейве не будет.
    Manager->RegisterGardenPlot(StalePlotCell, EGardenNiche::Cave);
    TestEqual(TEXT("Session plot has a marker"), CountHomesteadMarkers(World, EHomesteadMarkerKind::GardenPristroyka, StalePlotCell), 1);

    // Как LoadGame: состояние напрямую.
    Manager->GardenPlots.Reset();
    Manager->GardenPlots.Add(PlotCell, EGardenNiche::Mycelium);
    FHerbalistBase Base;
    Base.Cell = BaseCell;
    Manager->SetBases({ Base });
    Manager->SyncHomesteadMarkers();

    EGardenNiche Niche = EGardenNiche::None;
    TestEqual(TEXT("Saved plot got a marker"), CountHomesteadMarkers(World, EHomesteadMarkerKind::GardenPristroyka, PlotCell, &Niche), 1);
    TestEqual(TEXT("Marker carries the saved niche"), Niche, EGardenNiche::Mycelium);
    TestEqual(TEXT("Saved base got a marker"), CountHomesteadMarkers(World, EHomesteadMarkerKind::Base, BaseCell), 1);
    TestEqual(TEXT("Plot absent from the save lost its marker"), CountHomesteadMarkers(World, EHomesteadMarkerKind::GardenPristroyka, StalePlotCell), 0);

    // Повтор -- без дублей; смена ниши -- на месте.
    Manager->GardenPlots.Add(PlotCell, EGardenNiche::Cave);
    Manager->SyncHomesteadMarkers();
    TestEqual(TEXT("Second sync: still one plot marker"), CountHomesteadMarkers(World, EHomesteadMarkerKind::GardenPristroyka, PlotCell, &Niche), 1);
    TestEqual(TEXT("Second sync: niche updated"), Niche, EGardenNiche::Cave);
    TestEqual(TEXT("Second sync: still one base marker"), CountHomesteadMarkers(World, EHomesteadMarkerKind::Base, BaseCell), 1);

    // Сейв без построек убирает всё.
    Manager->GardenPlots.Reset();
    Manager->SetBases({});
    Manager->SyncHomesteadMarkers();
    TestEqual(TEXT("Empty state: no plot marker"), CountHomesteadMarkers(World, EHomesteadMarkerKind::GardenPristroyka, PlotCell), 0);
    TestEqual(TEXT("Empty state: no base marker"), CountHomesteadMarkers(World, EHomesteadMarkerKind::Base, BaseCell), 0);

    Manager->Destroy();
    return true;
}

#endif // WITH_AUTOMATION_TESTS && WITH_EDITOR
