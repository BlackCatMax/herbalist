// Source/ProjectHerbalistTests/Private/Tests/POITest.cpp
//
// Точки интереса (DESIGN_Brewing_Situations_And_Lore.md §4, 2026-09-06,
// прямой запрос пользователя "проработаем POI"). См. довод у POITypes.h/
// GridWorldManagerPOI.cpp за архитектурой. Курганы (§4.3) уже покрыты
// KurganTest.cpp -- здесь только новый общий каркас сева + Тотем/Светлояр/
// Соловей.

#include "Core/World/GridWorldManager.h"
#include "Core/World/POIActors.h"
#include "Misc/AutomationTest.h"
#include "Editor.h"
#include "Engine/World.h"
#include "EngineUtils.h"

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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistPOI_GoryuchKamenRelaxesTowardTargetStateSlowerThanAnOrdinaryCell,
    "Herbalist.POI.GoryuchKamenRelaxesTowardTargetStateSlowerThanAnOrdinaryCell",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistPOI_GoryuchKamenRelaxesTowardTargetStateSlowerThanAnOrdinaryCell::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;

    const FIntPoint GoryuchKamen = Manager->GetGoryuchKamenSite();
    FGridCell* StoneCell = Manager->GetCell(GoryuchKamen.X, GoryuchKamen.Y);
    if (!TestNotNull(TEXT("GoryuchKamen cell exists"), StoneCell)) { Manager->Destroy(); return false; }

    // Обычная клетка вдали от точки, с той же самой стартовой девиацией --
    // Corruption/Distortion держим низкими и равными State=TargetState, чтобы
    // не задеть гистерезис деградации (DegradeCenter=0.75), сравниваем чистое
    // MoveToward-затухание Purity.
    const FIntPoint FarCoord = (FMath::Abs(GoryuchKamen.X - 0) > 1 || FMath::Abs(GoryuchKamen.Y - 0) > 1) ? FIntPoint(0, 0) : FIntPoint(19, 19);
    FGridCell* OrdinaryCell = Manager->GetCell(FarCoord.X, FarCoord.Y);
    if (!TestNotNull(TEXT("Ordinary cell exists"), OrdinaryCell)) { Manager->Destroy(); return false; }

    for (FGridCell* Cell : { StoneCell, OrdinaryCell })
    {
        Cell->State.Meta.Purity = 0.0f;
        Cell->TargetState.Meta.Purity = 1.0f;
        Cell->State.Meta.Corruption = 0.1f;
        Cell->TargetState.Meta.Corruption = 0.1f;
        Cell->Memory.bDegrading = false;
    }

    Manager->RegenerateCellParameters(10.0f);

    TestTrue(TEXT("Ordinary cell moved toward its TargetState"), OrdinaryCell->State.Meta.Purity > 0.0f);
    TestTrue(TEXT("GoryuchKamen cell also moved, just less"), StoneCell->State.Meta.Purity > 0.0f);
    TestTrue(TEXT("GoryuchKamen resists the shift -- moves less than an ordinary cell in the same step"),
        StoneCell->State.Meta.Purity < OrdinaryCell->State.Meta.Purity);

    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistPOI_TotemMiddleTierVisibilityFollowsMolva,
    "Herbalist.POI.TotemMiddleTierVisibilityFollowsMolva",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistPOI_TotemMiddleTierVisibilityFollowsMolva::RunTest(const FString& Parameters)
{
    // Средний ярус (DESIGN_POI_Art_And_LevelDesign.md, "открытые вопросы —
    // решения", 2026-09-06) -- читается по Молве, закрывает честный пробел
    // юнита 1/2 (не было структуры для "состояния игрока").
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;

    Manager->Molva = 0.2f;
    TestFalse(TEXT("Below threshold: middle tier hidden"), Manager->IsTotemMiddleTierVisible());

    Manager->Molva = 0.9f;
    TestTrue(TEXT("Above threshold: middle tier visible"), Manager->IsTotemMiddleTierVisible());

    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistPOI_SvetloyarSoundTierStepsWithClarity,
    "Herbalist.POI.SvetloyarSoundTierStepsWithClarity",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistPOI_SvetloyarSoundTierStepsWithClarity::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;

    Manager->SetGlobalPerceptionClarity(0.5f);
    TestEqual(TEXT("Below visibility threshold: tier 0 (mute)"), Manager->GetSvetloyarSoundTier(), 0);

    Manager->SetGlobalPerceptionClarity(0.75f);
    TestEqual(TEXT("Just above threshold: tier 1 (far bell)"), Manager->GetSvetloyarSoundTier(), 1);

    Manager->SetGlobalPerceptionClarity(0.9f);
    TestEqual(TEXT("Mid-range: tier 2 (bell + singing)"), Manager->GetSvetloyarSoundTier(), 2);

    Manager->SetGlobalPerceptionClarity(1.0f);
    TestEqual(TEXT("Near-max Clarity: tier 3 (dome flash + choir)"), Manager->GetSvetloyarSoundTier(), 3);

    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistPOI_SeedingSpawnsOneVisualActorPerPOI,
    "Herbalist.POI.SeedingSpawnsOneVisualActorPerPOI",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistPOI_SeedingSpawnsOneVisualActorPerPOI::RunTest(const FString& Parameters)
{
    // DESIGN_POI_Art_And_LevelDesign.md, 2026-09-06 -- SeedPointsOfInterest
    // теперь спавнит акторы-заглушки, не только резолвит координаты.
    // Считаем ДЕЛЬТУ до/после, не абсолютное число -- тесты этого же файла
    // делят один персистентный editor-мир (тот же класс проблемы, что уже
    // решает явный Deinitialize() у UBiomeGraphSubsystem в других тестах),
    // и каждый ранее отработавший SpawnAndBeginPlay мог оставить в мире
    // свои собственные Totem/Svetloyar/GoryuchKamen -- эти акторы не
    // владеются AGridWorldManager и не удаляются вместе с ним.
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    int32 TotemBefore = 0, SvetloyarBefore = 0, GoryuchKamenBefore = 0;
    for (TActorIterator<APOI_Totem> It(World); It; ++It) ++TotemBefore;
    for (TActorIterator<APOI_Svetloyar> It(World); It; ++It) ++SvetloyarBefore;
    for (TActorIterator<APOI_GoryuchKamen> It(World); It; ++It) ++GoryuchKamenBefore;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;

    int32 TotemAfter = 0, SvetloyarAfter = 0, GoryuchKamenAfter = 0;
    for (TActorIterator<APOI_Totem> It(World); It; ++It) ++TotemAfter;
    for (TActorIterator<APOI_Svetloyar> It(World); It; ++It) ++SvetloyarAfter;
    for (TActorIterator<APOI_GoryuchKamen> It(World); It; ++It) ++GoryuchKamenAfter;

    TestEqual(TEXT("Exactly one new Totem actor spawned"), TotemAfter - TotemBefore, 1);
    TestEqual(TEXT("Exactly one new Svetloyar actor spawned"), SvetloyarAfter - SvetloyarBefore, 1);
    TestEqual(TEXT("Exactly one new GoryuchKamen actor spawned"), GoryuchKamenAfter - GoryuchKamenBefore, 1);

    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistPOI_ApplyingPlakunTravaToSoloveyCalmsItPermanently,
    "Herbalist.POI.ApplyingPlakunTravaToSoloveyCalmsItPermanently",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistPOI_ApplyingPlakunTravaToSoloveyCalmsItPermanently::RunTest(const FString& Parameters)
{
    // §4.4, DESIGN_POI_Art_And_LevelDesign.md §4, 2026-09-06 -- "riv_11" --
    // реальный RowName Плакун-травы в живой DT_IngredientClass (проверено
    // точечным запросом таблицы, не по устаревшему CSV-экспорту).
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;

    const FIntPoint Solovey = Manager->GetSoloveySite();
    TestFalse(TEXT("Sanity: not calmed yet"), Manager->IsSoloveyCalmed());

    FInventoryItem Plakun;
    Plakun.IngredientID = FName(TEXT("riv_11"));
    Manager->ApplyAlchemyResult(Solovey.X, Solovey.Y, { Plakun }, FIntent());

    TestTrue(TEXT("Calmed after applying Плакун-трава to its cell"), Manager->IsSoloveyCalmed());

    // Усмирён -- даже первый (никогда не пройденный) контакт теперь безопасен.
    FGridCell* Cell = Manager->GetCell(Solovey.X, Solovey.Y);
    if (TestNotNull(TEXT("Solovey cell exists"), Cell))
    {
        Cell->State.Meta.Purity = 0.8f;
        TestTrue(TEXT("ActivateSolovey succeeds harmlessly once calmed"), Manager->ActivateSolovey());
        TestEqual(TEXT("Purity untouched -- calmed, no Morok"), Cell->State.Meta.Purity, 0.8f);
    }

    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistPOI_SoloveyAmbientZoneCapsPurityUntilCalmed,
    "Herbalist.POI.SoloveyAmbientZoneCapsPurityUntilCalmed",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistPOI_SoloveyAmbientZoneCapsPurityUntilCalmed::RunTest(const FString& Parameters)
{
    // Постоянная зона порчи (DESIGN_POI_Art_And_LevelDesign.md §4:
    // "постоянный, не разовый признак") -- потолок TargetState.Meta.Purity
    // в радиусе, снят навсегда после усмирения.
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;

    const FIntPoint Solovey = Manager->GetSoloveySite();
    FGridCell* Cell = Manager->GetCell(Solovey.X, Solovey.Y);
    if (!TestNotNull(TEXT("Solovey cell exists"), Cell)) { Manager->Destroy(); return false; }

    Cell->TargetState.Meta.Purity = 0.95f;
    Manager->RegenerateCellParameters(1.0f);
    TestTrue(TEXT("TargetState.Purity capped down to the ambient ceiling"), Cell->TargetState.Meta.Purity <= 0.5f + KINDA_SMALL_NUMBER);

    FInventoryItem Plakun;
    Plakun.IngredientID = FName(TEXT("riv_11"));
    Manager->ApplyAlchemyResult(Solovey.X, Solovey.Y, { Plakun }, FIntent());

    Cell->TargetState.Meta.Purity = 0.95f;
    Manager->RegenerateCellParameters(1.0f);
    TestEqual(TEXT("Once calmed, TargetState.Purity is no longer capped"), Cell->TargetState.Meta.Purity, 0.95f);

    Manager->Destroy();
    return true;
}

#endif // WITH_AUTOMATION_TESTS && WITH_EDITOR
