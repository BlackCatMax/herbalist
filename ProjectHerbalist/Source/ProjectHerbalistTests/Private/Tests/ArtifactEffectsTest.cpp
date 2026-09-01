// Source/ProjectHerbalistTests/Private/Tests/ArtifactEffectsTest.cpp
//
// Семь эффектов артефактов (21_Journey_And_Artifacts.md §21.3, 2026-09-01,
// ревизия "Ending and artifacts"). Тот же DispatchBeginPlay-паттерн, что
// уже обкатан в ArtifactTest.cpp/BasesTest.cpp. Вертикальный срез —
// проверяет состояние/флаги, не гоняет полный UpdateEntityManifestations
// с BiomeGraphSubsystem (тот класс интеграции уже покрыт LegendaryEntityTest.cpp).

#include "Core/World/GridWorldManager.h"
#include "Misc/AutomationTest.h"
#include "Editor.h"
#include "Engine/World.h"

#if WITH_AUTOMATION_TESTS && WITH_EDITOR

#include "TestWorldHelpers.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistArtifactEffects_HornDiagnosesWaterHonestly,
    "Herbalist.ArtifactEffects.HornDiagnosesWaterHonestly",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistArtifactEffects_HornDiagnosesWaterHonestly::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;

    FText Diagnosis;
    TestFalse(TEXT("No Рог -- no diagnosis"), Manager->UseHornOnCell(FIntPoint(1, 1), Diagnosis));

    FAcquiredArtifact Horn;
    Horn.ArtifactID = FName(TEXT("Рог"));
    Manager->SetAcquiredArtifacts({ Horn });

    if (FGridCell* Cell = Manager->GetCell(1, 1))
    {
        Cell->bIsWater = false;
    }
    TestFalse(TEXT("Non-water cell -- no diagnosis"), Manager->UseHornOnCell(FIntPoint(1, 1), Diagnosis));

    if (FGridCell* Cell = Manager->GetCell(1, 1))
    {
        Cell->bIsWater = true;
        Cell->State.Meta.Corruption = 0.9f;
    }
    TestTrue(TEXT("Water cell with Рог -- diagnosis given"), Manager->UseHornOnCell(FIntPoint(1, 1), Diagnosis));
    TestTrue(TEXT("High Corruption reads as corrupted"), Diagnosis.ToString().Contains(TEXT("испорчен")));

    if (FGridCell* Cell = Manager->GetCell(1, 1))
    {
        Cell->State.Meta.Corruption = 0.1f;
    }
    Manager->UseHornOnCell(FIntPoint(1, 1), Diagnosis);
    TestTrue(TEXT("Low Corruption reads as clean"), Diagnosis.ToString().Contains(TEXT("чист")));

    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistArtifactEffects_CombClearsEntityAndIsSpent,
    "Herbalist.ArtifactEffects.CombClearsEntityAndIsSpent",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistArtifactEffects_CombClearsEntityAndIsSpent::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;

    TestFalse(TEXT("No Гребень -- no effect"), Manager->UseCombOnCell(FIntPoint(2, 2)));

    FAcquiredArtifact Comb;
    Comb.ArtifactID = FName(TEXT("Гребень"));
    Manager->SetAcquiredArtifacts({ Comb });

    if (FGridCell* Cell = Manager->GetCell(2, 2))
    {
        Cell->ManifestedEntityID = FName(TEXT("Гнильники"));
    }

    TestTrue(TEXT("Гребень clears the manifested entity"), Manager->UseCombOnCell(FIntPoint(2, 2)));
    if (const FGridCell* Cell = Manager->GetCellConst(2, 2))
    {
        TestEqual(TEXT("Entity actually cleared"), Cell->ManifestedEntityID, FName(NAME_None));
    }
    TestEqual(TEXT("Гребень consumed after one use"), Manager->GetAcquiredArtifacts().Num(), 0);

    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistArtifactEffects_YouthAppleGrantsWindowAndIsSpent,
    "Herbalist.ArtifactEffects.YouthAppleGrantsWindowAndIsSpent",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistArtifactEffects_YouthAppleGrantsWindowAndIsSpent::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;

    TestFalse(TEXT("No Яблоко -- no effect"), Manager->UseYouthApple());

    FAcquiredArtifact Apple;
    Apple.ArtifactID = FName(TEXT("Молодильное яблоко"));
    Manager->SetAcquiredArtifacts({ Apple });

    TestTrue(TEXT("Яблоко grants the window"), Manager->UseYouthApple());
    TestEqual(TEXT("Consumed after one use"), Manager->GetAcquiredArtifacts().Num(), 0);

    // Слой 1+3 при полной Clarity должен точно совпасть со Слоем 1 (тот же
    // инвариант, что уже RosaLayer1MatchesCellStateAtFullClarity, шаг 2) --
    // здесь Clarity=0 базово, но окно яблока должно поднять эффективную до 1.0.
    Manager->SetZaryanaCellIfUnset(FIntPoint(0, 0));
    if (FGridCell* Cell = Manager->GetCell(0, 0))
    {
        Cell->State.Meta.Purity = 0.8f;
    }
    FRandomStream Rng(1);
    const FRealState Perceived = Manager->GetZaryanaPerceivedState(Rng);
    TestEqual(TEXT("Youth apple window silences Rosa's noise like full Clarity would"), Perceived.Meta.Purity, 0.8f);

    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistArtifactEffects_InvisibilityCapActivatesAndDoesNotConsume,
    "Herbalist.ArtifactEffects.InvisibilityCapActivatesAndDoesNotConsume",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistArtifactEffects_InvisibilityCapActivatesAndDoesNotConsume::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;

    TestFalse(TEXT("Not active before use"), Manager->IsInvisibilityCapActive());
    TestFalse(TEXT("No Шапка -- no effect"), Manager->UseInvisibilityCap());

    FAcquiredArtifact Cap;
    Cap.ArtifactID = FName(TEXT("Шапка-невидимка"));
    Manager->SetAcquiredArtifacts({ Cap });

    TestTrue(TEXT("Шапка activates"), Manager->UseInvisibilityCap());
    TestTrue(TEXT("Active immediately after use"), Manager->IsInvisibilityCapActive());
    TestEqual(TEXT("NOT consumed -- reusable, unlike Гребень/Яблоко"), Manager->GetAcquiredArtifacts().Num(), 1);

    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistArtifactEffects_BifurcationCharmDetectedWhileUnspent,
    "Herbalist.ArtifactEffects.BifurcationCharmDetectedWhileUnspent",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistArtifactEffects_BifurcationCharmDetectedWhileUnspent::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;

    TestFalse(TEXT("No stone -- no charm"), Manager->HasUnspentBifurcationCharm());

    FAcquiredArtifact Stone;
    Stone.ArtifactID = FName(TEXT("Камень-оберег"));
    Manager->SetAcquiredArtifacts({ Stone });
    TestTrue(TEXT("Unspent stone -- charm active"), Manager->HasUnspentBifurcationCharm());

    TArray<FAcquiredArtifact> Artifacts = Manager->GetAcquiredArtifacts();
    Artifacts[0].bBifurcationChargeSpent = true;
    Manager->SetAcquiredArtifacts(Artifacts);
    TestFalse(TEXT("Spent stone -- no charm"), Manager->HasUnspentBifurcationCharm());

    Manager->Destroy();
    return true;
}

#endif // WITH_AUTOMATION_TESTS && WITH_EDITOR
