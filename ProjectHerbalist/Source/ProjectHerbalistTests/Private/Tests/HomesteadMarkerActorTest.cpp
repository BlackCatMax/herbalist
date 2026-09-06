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

#endif // WITH_AUTOMATION_TESTS && WITH_EDITOR
