// Source/ProjectHerbalistTests/Private/Tests/POITest.cpp
//
// Точки интереса (DESIGN_Brewing_Situations_And_Lore.md §4, 2026-09-06,
// прямой запрос пользователя "проработаем POI"). См. довод у POITypes.h/
// GridWorldManagerPOI.cpp за архитектурой. Курганы (§4.3) уже покрыты
// KurganTest.cpp -- здесь только новый общий каркас сева + Тотем/Светлояр/
// Соловей.

#include "Core/World/GridWorldManager.h"
#include "Misc/AutomationTest.h"
#include "Editor.h"
#include "Engine/World.h"

#if WITH_AUTOMATION_TESTS && WITH_EDITOR

#include "TestWorldHelpers.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistPOI_SeedingPlacesEachSingletonOnDistinctNonWaterCells,
    "Herbalist.POI.SeedingPlacesEachSingletonOnDistinctNonWaterCells",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistPOI_SeedingPlacesEachSingletonOnDistinctNonWaterCells::RunTest(const FString& Parameters)
{
    // SpawnAndBeginPlay -> InitializeCells уже вызвал SeedPointsOfInterest один раз.
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;

    const FIntPoint Unset(-1, -1);
    const FIntPoint Totem = Manager->GetTotemSite();
    const FIntPoint Svetloyar = Manager->GetSvetloyarSite();
    const FIntPoint GoryuchKamen = Manager->GetGoryuchKamenSite();
    const FIntPoint Solovey = Manager->GetSoloveySite();

    TestTrue(TEXT("Totem was placed"), Totem != Unset);
    TestTrue(TEXT("Svetloyar was placed"), Svetloyar != Unset);
    TestTrue(TEXT("GoryuchKamen was placed"), GoryuchKamen != Unset);
    TestTrue(TEXT("Solovey was placed"), Solovey != Unset);

    TArray<FIntPoint> AllSites = { Totem, Svetloyar, GoryuchKamen, Solovey };
    for (const auto& Pair : Manager->GetKurganSites())
    {
        AllSites.Add(Pair.Key);
    }

    TSet<FIntPoint> Distinct(AllSites);
    TestEqual(TEXT("No two POI (including kurgans) share a cell"), Distinct.Num(), AllSites.Num());

    for (const FIntPoint& Site : AllSites)
    {
        const FGridCell* Cell = Manager->GetCellConst(Site.X, Site.Y);
        TestTrue(TEXT("Every seeded POI site is on a non-water cell"), Cell && !Cell->bIsWater);
    }

    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistPOI_TotemRevealTextTracksCellDistortionAndPurity,
    "Herbalist.POI.TotemRevealTextTracksCellDistortionAndPurity",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistPOI_TotemRevealTextTracksCellDistortionAndPurity::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;

    const FIntPoint Totem = Manager->GetTotemSite();
    FGridCell* Cell = Manager->GetCell(Totem.X, Totem.Y);
    if (!TestNotNull(TEXT("Totem cell exists"), Cell)) { Manager->Destroy(); return false; }

    Cell->State.Meta.Distortion = 0.2f;
    Cell->State.Meta.Purity = 0.1f;
    const FString LowDistortionLowPurity = Manager->GetTotemRevealText();
    TestTrue(TEXT("Low Distortion reads as clear-faced"), LowDistortionLowPurity.Contains(TEXT("читается ясно")));
    TestFalse(TEXT("Low Purity does NOT reveal the upper tier"), LowDistortionLowPurity.Contains(TEXT("Верхний ярус")));

    Cell->State.Meta.Distortion = 0.8f;
    Cell->State.Meta.Purity = 0.9f;
    const FString HighDistortionHighPurity = Manager->GetTotemRevealText();
    TestTrue(TEXT("High Distortion reads as distorted-faced"), HighDistortionHighPurity.Contains(TEXT("искажён")));
    TestTrue(TEXT("High Purity reveals the upper tier"), HighDistortionHighPurity.Contains(TEXT("Верхний ярус")));

    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistPOI_SvetloyarVisibilityFollowsGlobalPerceptionClarity,
    "Herbalist.POI.SvetloyarVisibilityFollowsGlobalPerceptionClarity",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistPOI_SvetloyarVisibilityFollowsGlobalPerceptionClarity::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;

    Manager->SetGlobalPerceptionClarity(0.5f);
    TestFalse(TEXT("Below threshold: Svetloyar is hidden"), Manager->IsSvetloyarVisible());

    Manager->SetGlobalPerceptionClarity(0.9f);
    TestTrue(TEXT("Above threshold: Svetloyar is visible"), Manager->IsSvetloyarVisible());

    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistPOI_ActivateSoloveyAppliesAoECorruptionOnceThenNoOps,
    "Herbalist.POI.ActivateSoloveyAppliesAoECorruptionOnceThenNoOps",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistPOI_ActivateSoloveyAppliesAoECorruptionOnceThenNoOps::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;

    const FIntPoint Solovey = Manager->GetSoloveySite();
    FGridCell* Cell = Manager->GetCell(Solovey.X, Solovey.Y);
    if (!TestNotNull(TEXT("Solovey cell exists"), Cell)) { Manager->Destroy(); return false; }

    Cell->State.Meta.Purity = 0.8f;
    Cell->State.Meta.Stability = 0.8f;

    // Клетка далеко за пределами радиуса AoE (SoloveyCorruptionRadius=3
    // по умолчанию) -- сеточная граница 20x20, Chebyshev-дистанция > 3 от
    // почти любой точки гарантированно найдётся у клетки (0,0) или (19,19).
    const FIntPoint FarCoord = (FMath::Abs(Solovey.X - 0) > 3 || FMath::Abs(Solovey.Y - 0) > 3) ? FIntPoint(0, 0) : FIntPoint(19, 19);
    FGridCell* FarCell = Manager->GetCell(FarCoord.X, FarCoord.Y);
    if (!TestNotNull(TEXT("Far cell exists"), FarCell)) { Manager->Destroy(); return false; }
    FarCell->State.Meta.Purity = 0.8f;

    TestFalse(TEXT("Sanity: not triggered yet"), Manager->IsSoloveyTriggered());
    TestTrue(TEXT("First activation succeeds"), Manager->ActivateSolovey());
    TestTrue(TEXT("Now marked triggered"), Manager->IsSoloveyTriggered());

    TestTrue(TEXT("Purity at the POI cell dropped"), Cell->State.Meta.Purity < 0.8f);
    TestTrue(TEXT("Stability at the POI cell dropped"), Cell->State.Meta.Stability < 0.8f);
    TestEqual(TEXT("Purity far outside the radius is untouched"), FarCell->State.Meta.Purity, 0.8f);

    const float PurityAfterFirstTrigger = Cell->State.Meta.Purity;
    TestFalse(TEXT("Second activation is a no-op (already triggered)"), Manager->ActivateSolovey());
    TestEqual(TEXT("Purity unchanged by the no-op second activation"), Cell->State.Meta.Purity, PurityAfterFirstTrigger);

    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistPOI_ActivateSoloveyUnderWardConcealmentSkipsCorruption,
    "Herbalist.POI.ActivateSoloveyUnderWardConcealmentSkipsCorruption",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistPOI_ActivateSoloveyUnderWardConcealmentSkipsCorruption::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;

    const FIntPoint Solovey = Manager->GetSoloveySite();
    FGridCell* Cell = Manager->GetCell(Solovey.X, Solovey.Y);
    if (!TestNotNull(TEXT("Solovey cell exists"), Cell)) { Manager->Destroy(); return false; }
    Cell->State.Meta.Purity = 0.8f;

    // Одолень-трава ДО контакта (§4.4) -- тот же оберег, что уже даёт
    // "скрытие" от бестиария (IsWardConcealmentActive).
    Manager->ActivateWardConcealment(Solovey);

    TestTrue(TEXT("Passing under concealment still marks the site handled"), Manager->ActivateSolovey());
    TestTrue(TEXT("Marked triggered even though nothing was corrupted"), Manager->IsSoloveyTriggered());
    TestEqual(TEXT("Purity untouched -- concealment prevented the AoE burst"), Cell->State.Meta.Purity, 0.8f);

    Manager->Destroy();
    return true;
}

#endif // WITH_AUTOMATION_TESTS && WITH_EDITOR
