// Source/ProjectHerbalistTests/Private/Tests/KurganTest.cpp
//
// Курганы (DESIGN_Brewing_Situations_And_Lore.md §4.3 "Гнёздово",
// DESIGN_Community_And_Homestead.md §2.3, 2026-09-06) — единственный
// источник Костяного ножа/Серебряного оберега. Тестирует AGridWorldManager::
// SeedKurganSites/LootKurgan напрямую — тот же класс пробела на резолв по
// имени, что уже у ActivateWard/TradeWithCommunity/PlantSeed (см. ROADMAP.md):
// AHerbalistPlayerController::LootKurgan (State через IngredientRegistrySubsystem,
// недоступный в Editor-мире автотестов) здесь не покрыт, только сам эффект.

#include "Core/World/GridWorldManager.h"
#include "Misc/AutomationTest.h"
#include "Editor.h"
#include "Engine/World.h"

#if WITH_AUTOMATION_TESTS && WITH_EDITOR

#include "TestWorldHelpers.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistKurgan_SeedingProducesExactlyTwoNonWaterSites,
    "Herbalist.Kurgan.SeedingProducesExactlyTwoNonWaterSites",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistKurgan_SeedingProducesExactlyTwoNonWaterSites::RunTest(const FString& Parameters)
{
    // SpawnAndBeginPlay -> InitializeCells уже вызвал SeedKurganSites один раз.
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;

    const TMap<FIntPoint, FName>& Sites = Manager->GetKurganSites();
    TestEqual(TEXT("Exactly two kurgan sites seeded (Костяной нож + Серебряный оберег)"), Sites.Num(), 2);

    bool bHasBoneKnife = false, bHasSilverWard = false;
    for (const auto& Pair : Sites)
    {
        const FGridCell* Cell = Manager->GetCellConst(Pair.Key.X, Pair.Key.Y);
        TestTrue(TEXT("Kurgan site is not on water"), Cell && !Cell->bIsWater);
        if (Pair.Value == FName(TEXT("Костяной нож"))) bHasBoneKnife = true;
        if (Pair.Value == FName(TEXT("Серебряный оберег"))) bHasSilverWard = true;
    }
    TestTrue(TEXT("One site grants Костяной нож"), bHasBoneKnife);
    TestTrue(TEXT("One site grants Серебряный оберег"), bHasSilverWard);

    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistKurgan_LootingConsumesTheSiteAndCanOnlyHappenOnce,
    "Herbalist.Kurgan.LootingConsumesTheSiteAndCanOnlyHappenOnce",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistKurgan_LootingConsumesTheSiteAndCanOnlyHappenOnce::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;

    const TMap<FIntPoint, FName>& Sites = Manager->GetKurganSites();
    if (!TestTrue(TEXT("At least one kurgan seeded"), Sites.Num() > 0)) { Manager->Destroy(); return false; }
    const FIntPoint SiteCell = Sites.CreateConstIterator()->Key;
    const FName ExpectedLoot = Sites.CreateConstIterator()->Value;

    FName Granted;
    TestTrue(TEXT("First loot succeeds"), Manager->LootKurgan(SiteCell, Granted));
    TestEqual(TEXT("Grants the site's own loot"), Granted, ExpectedLoot);
    TestEqual(TEXT("Site removed from the map after looting"), Manager->GetKurganSites().Num(), 1);

    FName SecondAttempt;
    TestFalse(TEXT("Second loot at the same cell fails -- already looted"), Manager->LootKurgan(SiteCell, SecondAttempt));

    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistKurgan_LootingAnEmptyCellFails,
    "Herbalist.Kurgan.LootingAnEmptyCellFails",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistKurgan_LootingAnEmptyCellFails::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;

    // Явно снимаем всё, что засеяла SeedKurganSites -- проверяем ровно
    // "нет кургана здесь", не полагаясь на удачную координату.
    Manager->SetKurganSites({});

    FName Granted;
    TestFalse(TEXT("Looting with no kurgan sites at all fails"), Manager->LootKurgan(FIntPoint(0, 0), Granted));

    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistKurgan_SilverWardSuppressesAmbientManifestation,
    "Herbalist.Kurgan.SilverWardSuppressesAmbientManifestation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistKurgan_SilverWardSuppressesAmbientManifestation::RunTest(const FString& Parameters)
{
    // Серебряный оберег (Ось Б §2.3) -- общий на всю сетку источник
    // подавления, действует в той же OR-цепочке, что и Шапка-невидимка/
    // обереги-с-таймером (см. довод у IsSilverWardActive, GridWorldManager.h).
    // Та же фикстура (Гнильники на испорченном Болоте), что уже
    // AmbientEntityTest.cpp::GnilnikiStillManifestsAfterRefactor использует
    // для базового случая.
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;

    FGridCell* Cell = Manager->GetCell(0, 0);
    if (!TestNotNull(TEXT("Cell (0,0) exists"), Cell)) { Manager->Destroy(); return false; }
    Cell->Biome = EBiomeType::Bog;
    Cell->bIsWater = false;
    Cell->State.Meta.Corruption = 0.8f;
    Cell->State.Meta.Purity = 0.5f;
    Cell->TargetState.Meta.Purity = 0.5f;
    Manager->SetGameClockSeconds(10.0f * 60.0f);   // середина Дня, изоляция от Рассвета

    TestFalse(TEXT("Sanity: silver ward starts inactive"), Manager->IsSilverWardActive());
    Manager->SetSilverWardActive(true);
    TestTrue(TEXT("Silver ward now active"), Manager->IsSilverWardActive());

    Manager->UpdateEntityManifestations(1.0f);
    TestNotEqual(TEXT("Гнильники do NOT manifest while the silver ward is active"),
        Cell->ManifestedEntityID, FName(TEXT("Гнильники")));

    Manager->SetSilverWardActive(false);
    Manager->UpdateEntityManifestations(1.0f);
    TestEqual(TEXT("Deactivating the ward lets Гнильники manifest normally"),
        Cell->ManifestedEntityID, FName(TEXT("Гнильники")));

    Manager->Destroy();
    return true;
}

#endif // WITH_AUTOMATION_TESTS && WITH_EDITOR
