// Source/ProjectHerbalistTests/Private/Tests/ProphetFeatherTest.cpp
//
// Перья вещих птиц (16_Entity_Manifestation.md §16.4, эндгейм-трофеи,
// 2026-09-02). Тот же DispatchBeginPlay-паттерн, что уже ArtifactTest.cpp/
// ArtifactEffectsTest.cpp — IsLegendaryManifested проверяет только
// Cell.ManifestedEntityID на якорной клетке, не требует полного
// BiomeGraphSubsystem-графа (тот класс интеграции уже покрыт
// LegendaryEntityTest.cpp), поэтому эти тесты выставляют ManifestedEntityID
// напрямую, тем же приёмом, что уже ArtifactTest.cpp::HonestOfferingAcquiresArtifact.

#include "Core/World/GridWorldManager.h"
#include "Misc/AutomationTest.h"
#include "Editor.h"
#include "Engine/World.h"

#if WITH_AUTOMATION_TESTS && WITH_EDITOR

#include "TestWorldHelpers.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistFeather_GamayunRequiresWarmedMirrorAndManifestation,
    "Herbalist.Feather.GamayunRequiresWarmedMirrorAndManifestation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistFeather_GamayunRequiresWarmedMirrorAndManifestation::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("Manager spawned"), Manager)) return false;

    const FName GamayunID(TEXT("Гамаюн"));
    const FName FeatherID(TEXT("Перо Гамаюна"));
    const FIntPoint* Anchor = Manager->GetLegendaryAnchors().Find(GamayunID);
    if (!TestNotNull(TEXT("Гамаюн has a seeded anchor cell"), Anchor))
    {
        Manager->Destroy();
        return false;
    }

    TestFalse(TEXT("No Зеркальце at all -- feather not acquired"), Manager->TryAcquireProphetFeather(FeatherID));

    FAcquiredArtifact Mirror;
    Mirror.ArtifactID = FName(TEXT("Зеркальце"));
    Mirror.Warmth = 0.0f;   // held, but not warmed
    Manager->SetAcquiredArtifacts({ Mirror });
    TestFalse(TEXT("Зеркальце held but not warmed -- feather not acquired"), Manager->TryAcquireProphetFeather(FeatherID));

    TArray<FAcquiredArtifact> Artifacts = Manager->GetAcquiredArtifacts();
    Artifacts[0].Warmth = 1.0f;   // now warmed (default threshold)
    Manager->SetAcquiredArtifacts(Artifacts);
    TestFalse(TEXT("Зеркальце warmed but Гамаюн not manifested -- still no feather"), Manager->TryAcquireProphetFeather(FeatherID));

    if (FGridCell* Cell = Manager->GetCell(Anchor->X, Anchor->Y))
    {
        Cell->ManifestedEntityID = GamayunID;
    }
    TestTrue(TEXT("Warmed Зеркальце + manifested Гамаюн -- feather acquired"), Manager->TryAcquireProphetFeather(FeatherID));
    TestTrue(TEXT("Feather listed as acquired"), Manager->GetAcquiredFeathers().Contains(FeatherID));

    TestFalse(TEXT("Cannot acquire the same feather twice"), Manager->TryAcquireProphetFeather(FeatherID));

    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistFeather_AlkonostSirinZharPtitsaOnlyNeedTheirOwnManifestation,
    "Herbalist.Feather.AlkonostSirinZharPtitsaOnlyNeedTheirOwnManifestation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistFeather_AlkonostSirinZharPtitsaOnlyNeedTheirOwnManifestation::RunTest(const FString& Parameters)
{
    // §16.4: только Гамаюн требует уже добытого базового артефакта -- "ИЛИ
    // очень редкого мирового события" покрывает три остальные, без всякой
    // связи с ArtifactTypes.h (ни у одной из этих трёх нет базового
    // артефакта в §21.3 вовсе).
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("Manager spawned"), Manager)) return false;

    struct FCase { FName EntityID; FName FeatherID; };
    const FCase Cases[] = {
        { FName(TEXT("Алконост")), FName(TEXT("Перо Алконоста")) },
        { FName(TEXT("Сирин")), FName(TEXT("Перо Сирина")) },
        { FName(TEXT("жар-птица")), FName(TEXT("Перо Жар-птицы")) },
    };

    for (const FCase& C : Cases)
    {
        const FIntPoint* Anchor = Manager->GetLegendaryAnchors().Find(C.EntityID);
        if (!TestNotNull(*FString::Printf(TEXT("%s has a seeded anchor cell"), *C.EntityID.ToString()), Anchor))
        {
            continue;
        }

        TestFalse(*FString::Printf(TEXT("%s: not manifested -- no feather"), *C.EntityID.ToString()),
            Manager->TryAcquireProphetFeather(C.FeatherID));

        if (FGridCell* Cell = Manager->GetCell(Anchor->X, Anchor->Y))
        {
            Cell->ManifestedEntityID = C.EntityID;
        }
        TestTrue(*FString::Printf(TEXT("%s: manifested -- feather acquired, no artifact needed"), *C.EntityID.ToString()),
            Manager->TryAcquireProphetFeather(C.FeatherID));
    }

    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistFeather_GamayunEatenGuaranteesMirrorPropheticReading,
    "Herbalist.Feather.GamayunEatenGuaranteesMirrorPropheticReading",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistFeather_GamayunEatenGuaranteesMirrorPropheticReading::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("Manager spawned"), Manager)) return false;

    TestFalse(TEXT("No feather -- cannot eat"), Manager->EatGamayunFeather());
    TestFalse(TEXT("Not guaranteed yet"), Manager->IsGamayunPropheticGuaranteed());

    Manager->SetAcquiredFeathers({ FName(TEXT("Перо Гамаюна")) });
    TestTrue(TEXT("Feather eaten"), Manager->EatGamayunFeather());
    TestTrue(TEXT("Prophetic reading now permanently guaranteed"), Manager->IsGamayunPropheticGuaranteed());
    TestEqual(TEXT("Feather consumed, no longer held"), Manager->GetAcquiredFeathers().Num(), 0);

    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistFeather_ZaryanaTrueStateSkipsPerceptionNoise,
    "Herbalist.Feather.ZaryanaTrueStateSkipsPerceptionNoise",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistFeather_ZaryanaTrueStateSkipsPerceptionNoise::RunTest(const FString& Parameters)
{
    // GetZaryanaTrueState -- честное чтение Слоёв 1+3 без шума, даже при
    // Clarity=0 и высоком Distortion (в отличие от GetZaryanaPerceivedState,
    // которая на тех же условиях зашумила бы Purity).
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("Manager spawned"), Manager)) return false;

    Manager->SetZaryanaCellIfUnset(FIntPoint(0, 0));
    if (FGridCell* Cell = Manager->GetCell(0, 0))
    {
        Cell->State.Meta.Purity = 0.8f;
        Cell->State.Meta.Corruption = 0.1f;
        Cell->State.Meta.Distortion = 0.9f;   // высокий -- обычное чтение было бы шумным
    }
    Manager->SetGlobalPerceptionClarity(0.0f);   // низкая -- обычное чтение шумело бы по максимуму

    const FRealState TrueState = Manager->GetZaryanaTrueState();
    TestEqual(TEXT("True state passes Purity through exactly, no noise"), TrueState.Meta.Purity, 0.8f);
    TestEqual(TEXT("True state passes Corruption through exactly, no noise"), TrueState.Meta.Corruption, 0.1f);

    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistFeather_AlkonostSuppressesOnlyItsOwnBiome,
    "Herbalist.Feather.AlkonostSuppressesOnlyItsOwnBiome",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistFeather_AlkonostSuppressesOnlyItsOwnBiome::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("Manager spawned"), Manager)) return false;

    TestFalse(TEXT("No feather -- no effect"), Manager->UseAlkonostFeatherOnBiome(EBiomeType::Bog));
    TestFalse(TEXT("Not active before use"), Manager->IsAlkonostSuppressionActiveForBiome(EBiomeType::Bog));

    Manager->SetAcquiredFeathers({ FName(TEXT("Перо Алконоста")) });
    TestTrue(TEXT("Feather spent, suppression activated"), Manager->UseAlkonostFeatherOnBiome(EBiomeType::Bog));
    TestEqual(TEXT("Feather consumed"), Manager->GetAcquiredFeathers().Num(), 0);

    TestTrue(TEXT("Active for the chosen biome (Bog)"), Manager->IsAlkonostSuppressionActiveForBiome(EBiomeType::Bog));
    TestFalse(TEXT("NOT active for a different biome"), Manager->IsAlkonostSuppressionActiveForBiome(EBiomeType::Taiga));

    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistFeather_SirinRequiresActiveMalignSpikeInCellBiome,
    "Herbalist.Feather.SirinRequiresActiveMalignSpikeInCellBiome",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistFeather_SirinRequiresActiveMalignSpikeInCellBiome::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("Manager spawned"), Manager)) return false;

    const FIntPoint* BogAnchor = Manager->GetLegendaryAnchors().Find(FName(TEXT("Болотный царь")));
    if (!TestNotNull(TEXT("Болотный царь has a seeded anchor cell in Bog"), BogAnchor))
    {
        Manager->Destroy();
        return false;
    }

    FText Disclosure;
    TestFalse(TEXT("No feather -- no reading"), Manager->UseSirinFeatherOnCell(*BogAnchor, Disclosure));

    Manager->SetAcquiredFeathers({ FName(TEXT("Перо Сирина")) });
    TestFalse(TEXT("Feather held, but no Malign spike active in Bog -- still no reading"),
        Manager->UseSirinFeatherOnCell(*BogAnchor, Disclosure));
    TestEqual(TEXT("Feather not consumed by a failed attempt"), Manager->GetAcquiredFeathers().Num(), 1);

    // Активируем Malign-спайк Болотного царя на его якоре -- это тоже "в биоме Болото".
    if (FGridCell* Cell = Manager->GetCell(BogAnchor->X, BogAnchor->Y))
    {
        Cell->ManifestedEntityID = FName(TEXT("Болотный царь"));
        Cell->State.Meta.Purity = 0.37f;
    }
    TestTrue(TEXT("Active Malign spike in Bog -- honest reading given"), Manager->UseSirinFeatherOnCell(*BogAnchor, Disclosure));
    TestTrue(TEXT("Disclosed text carries the real Purity value"), Disclosure.ToString().Contains(TEXT("0.37")));
    TestEqual(TEXT("Feather consumed after use"), Manager->GetAcquiredFeathers().Num(), 0);

    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistFeather_ZharPtitsaMarksCellEternallyPureAndExcludesItFromRelaxation,
    "Herbalist.Feather.ZharPtitsaMarksCellEternallyPureAndExcludesItFromRelaxation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistFeather_ZharPtitsaMarksCellEternallyPureAndExcludesItFromRelaxation::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("Manager spawned"), Manager)) return false;

    const FIntPoint Cell(4, 4);
    TestFalse(TEXT("No feather -- no effect"), Manager->UseZharPtitsaFeatherOnCell(Cell));
    TestFalse(TEXT("Not eternally pure before use"), Manager->IsCellEternallyPure(Cell));

    Manager->SetAcquiredFeathers({ FName(TEXT("Перо Жар-птицы")) });
    TestTrue(TEXT("Feather spent, cell marked"), Manager->UseZharPtitsaFeatherOnCell(Cell));
    TestTrue(TEXT("Cell now eternally pure"), Manager->IsCellEternallyPure(Cell));
    TestEqual(TEXT("Feather consumed"), Manager->GetAcquiredFeathers().Num(), 0);

    // Форсируем клетку в испорченный полюс, затем прогоняем релаксацию --
    // eternally pure клетка обязана остаться нетронутой, вопреки State,
    // который в любой другой ситуации триггернул бы бистабильность.
    if (FGridCell* MutableCell = Manager->GetCell(Cell.X, Cell.Y))
    {
        MutableCell->State.Meta.Corruption = 0.99f;
    }
    const float DistortionBefore = Manager->GetCellConst(Cell.X, Cell.Y)->State.Meta.Distortion;
    const bool bWasDegrading = Manager->GetCellConst(Cell.X, Cell.Y)->Memory.bDegrading;
    Manager->RegenerateCellParameters(10.0f);
    TestEqual(TEXT("Distortion untouched by relaxation"), Manager->GetCellConst(Cell.X, Cell.Y)->State.Meta.Distortion, DistortionBefore);
    TestEqual(TEXT("bDegrading untouched -- RegenerateCellParameters skips this cell entirely"),
        Manager->GetCellConst(Cell.X, Cell.Y)->Memory.bDegrading, bWasDegrading);

    Manager->Destroy();
    return true;
}

#endif // WITH_AUTOMATION_TESTS && WITH_EDITOR
