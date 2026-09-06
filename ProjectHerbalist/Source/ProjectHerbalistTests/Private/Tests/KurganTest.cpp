// Source/ProjectHerbalistTests/Private/Tests/KurganTest.cpp
//
// Курганы (DESIGN_Brewing_Situations_And_Lore.md §4.3 "Гнёздово",
// DESIGN_Community_And_Homestead.md §2.3, 2026-09-06) — единственный
// источник Костяного ножа/Серебряного оберега. Тестирует AGridWorldManager::
// SeedKurganSites/LootKurgan напрямую — тот же класс пробела на резолв по
// имени, что уже у ActivateWard/TradeWithCommunity/PlantSeed (см. ROADMAP.md):
// AKurganActor::OnInteract (State через IngredientRegistrySubsystem,
// недоступный в Editor-мире автотестов) здесь не покрыт полностью, только
// сам эффект и факт спавна/подбора актора.
//
// Физический подбор (DECISIONS_LOG.md решение №5, 2026-09-06) — курганы
// раньше были голым Exec (LootKurgan на контроллере), теперь AKurganActor
// на клетке, подбирается через IInteractable/Interact(), тот же паттерн,
// что AMemoryFragmentActor.

#include "Core/World/GridWorldManager.h"
#include "Core/World/KurganActor.h"
#include "Player/HerbalistPlayerController.h"
#include "Misc/AutomationTest.h"
#include "Editor.h"
#include "Engine/World.h"
#include "EngineUtils.h"

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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistKurgan_SeedingSpawnsOnePhysicalPickupActorPerSite,
    "Herbalist.Kurgan.SeedingSpawnsOnePhysicalPickupActorPerSite",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistKurgan_SeedingSpawnsOnePhysicalPickupActorPerSite::RunTest(const FString& Parameters)
{
    // DECISIONS_LOG.md решение №5, 2026-09-06 -- дельта до/после, не
    // абсолютное число: акторы-заглушки этого же захода (Тотем/Светлояр/
    // Горюч-камень) и предыдущие прогоны этого файла делят один
    // персистентный editor-мир, см. тот же довод у
    // Herbalist.POI.SeedingSpawnsOneVisualActorPerPOI.
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    int32 Before = 0;
    for (TActorIterator<AKurganActor> It(World); It; ++It) ++Before;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;

    int32 After = 0;
    for (TActorIterator<AKurganActor> It(World); It; ++It) ++After;

    TestEqual(TEXT("Exactly as many new pickup actors as kurgan sites"), After - Before, Manager->GetKurganSites().Num());

    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistKurgan_InteractingWithPickupActorGrantsItemAndClearsSite,
    "Herbalist.Kurgan.InteractingWithPickupActorGrantsItemAndClearsSite",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistKurgan_InteractingWithPickupActorGrantsItemAndClearsSite::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    // Kurgan-координаты детерминированы (тот же WorldRNG-сид в каждом
    // тестовом прогоне) -- по этой же клетке в мире могут уже стоять
    // акторы, оставленные ДРУГИМИ тестами этого файла (тот же класс
    // проблемы, что уже решает дельта-подсчёт у
    // Herbalist.Kurgan.SeedingSpawnsOnePhysicalPickupActorPerSite выше, но
    // здесь недостаточно посчитать дельту -- нужен именно СВЕЖИЙ актор,
    // привязанный к менеджеру ЭТОГО теста, не к уже разрушенному менеджеру
    // из более раннего прогона). Снимаем множество уже существующих
    // акторов ДО спавна, ищем среди новых.
    TSet<AKurganActor*> ExistingBefore;
    for (TActorIterator<AKurganActor> It(World); It; ++It) ExistingBefore.Add(*It);

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;
    AHerbalistPlayerController* PC = SpawnControllerAndBeginPlay(World, Manager);
    if (!TestNotNull(TEXT("Controller spawned"), PC)) { Manager->Destroy(); return false; }

    const TMap<FIntPoint, FName>& Sites = Manager->GetKurganSites();
    if (!TestTrue(TEXT("At least one kurgan seeded"), Sites.Num() > 0)) { Manager->Destroy(); PC->Destroy(); return false; }
    const FIntPoint SiteCell = Sites.CreateConstIterator()->Key;

    AKurganActor* Pickup = nullptr;
    for (TActorIterator<AKurganActor> It(World); It; ++It)
    {
        if (ExistingBefore.Contains(*It)) continue;
        if (It->GetGridCell() == SiteCell) { Pickup = *It; break; }
    }
    if (!TestNotNull(TEXT("A freshly spawned pickup actor exists at the seeded site"), Pickup)) { Manager->Destroy(); PC->Destroy(); return false; }

    const int32 ItemsBefore = PC->InventoryComponent ? PC->InventoryComponent->GetItems().Num() : 0;

    // Вызов _Implementation напрямую, не через Execute_OnInteract/
    // Interact() -- тот общий диспетчер (камера-трейс + Implements<
    // UInteractable>()) уже разделяемый, нетронутый код, здесь важно
    // проверить именно собственную логику AKurganActor::OnInteract, не
    // переверять сам механизм интерфейса.
    Pickup->OnInteract_Implementation(PC);

    TestFalse(TEXT("Site removed from KurganSites after pickup"), Manager->GetKurganSites().Contains(SiteCell));
    if (PC->InventoryComponent)
    {
        TestEqual(TEXT("Exactly one new item in inventory"), PC->InventoryComponent->GetItems().Num(), ItemsBefore + 1);
    }
    TestTrue(TEXT("Pickup actor destroys itself after granting the item"), !IsValid(Pickup) || Pickup->IsActorBeingDestroyed());

    Manager->Destroy();
    PC->Destroy();
    return true;
}

#endif // WITH_AUTOMATION_TESTS && WITH_EDITOR
