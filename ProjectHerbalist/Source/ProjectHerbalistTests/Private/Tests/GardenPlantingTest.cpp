// Source/ProjectHerbalistTests/Private/Tests/GardenPlantingTest.cpp
//
// Посадка (PlantSeed, DESIGN_Community_And_Homestead.md §2.4, 2026-09-04,
// прямой запрос пользователя): "механика использования садов должна
// различать сбор растения ДЛЯ ПОСАДКИ в грядку и сбор ТОГО ЖЕ растения для
// варки — сейчас посадка вообще не выбирает вид явно (только косвенно,
// через State→BaseState)". До этой правки грядка сада (RegisterGardenPlot/
// GardenPlots) только подделывала НИШУ (GetRandomResourceForNiche —
// вероятностный выбор среди кандидатов ниши по близости Cell.State к
// Row.BaseState); эта правка добавляет явную посадку КОНКРЕТНОГО вида
// (FGridCell::PlantedSpeciesID, AGridWorldManager::PlantSeedInCell) поверх
// того же механизма — вероятностный путь для непосаженных клеток не тронут
// (см. отдельно ResourceRegrowthTest.cpp/PcgResourcePlacementTest.cpp).
//
// PlantSeedInCell намеренно НЕ трогает GameInstance/IngredientRegistrySubsystem
// (SpeciesNiche резолвится вызывающей стороной, AHerbalistPlayerController::
// PlantSeed) — тот же принцип границы, что уже WardTest.cpp применяет к
// ActivateWardBrewBoost/ActivateWardConcealment: тестируется напрямую здесь,
// резолв инвентаря по имени в PlantSeed — нет (см. ROADMAP.md, тот же класс
// пробела, что уже у ActivateWard/TradeWithCommunity).

#include "Core/World/GridWorldManager.h"
#include "Core/Resources/AHerbalistResourceActor.h"
#include "Core/Subsystems/IngredientRegistrySubsystem.h"
#include "Core/Data/IngredientTableRow.h"
#include "Core/Save/HerbalistSaveTypes.h"
#include "Core/Inventory/HerbalistInventoryComponent.h"
#include "Player/HerbalistPlayerController.h"
#include "Misc/AutomationTest.h"
#include "Editor.h"
#include "Engine/World.h"
#include "Engine/DataTable.h"
#include "Engine/GameInstance.h"
#include "Engine/Engine.h"

#if WITH_AUTOMATION_TESTS && WITH_EDITOR

#include "TestWorldHelpers.h"

namespace
{
    const FName PlantedSpecies(TEXT("GardenPlantA"));
    const FName OtherSpecies(TEXT("GardenPlantB"));

    // Два кандидата одной ниши, тот же приём синтетической таблицы, что уже
    // PcgResourcePlacementTest.cpp/IngredientAltitudeTest.cpp -- в голом
    // editor-мире тестов GameInstance нет, реестр строится и отдаётся явно.
    // OtherSpecies несёт заведомо БОЛЬШИЙ RarityWeight -- если бы посадка не
    // обходила PickWeightedResource целиком, она проигрывала бы ему почти
    // всегда, не "здесь посажено именно это".
    UIngredientRegistrySubsystem* MakeTwoCandidateNicheRegistry()
    {
        UGameInstance* Owner = NewObject<UGameInstance>(GEngine);
        UIngredientRegistrySubsystem* Registry = NewObject<UIngredientRegistrySubsystem>(Owner);

        UDataTable* Table = NewObject<UDataTable>();
        Table->RowStruct = FIngredientTableRow::StaticStruct();

        FIngredientTableRow Planted;
        Planted.DisplayName = FText::FromString(TEXT("Посаженная трава"));
        Planted.GardenNiche = EGardenNiche::SunnyBed;
        Planted.RarityWeight = 1;
        Table->AddRow(PlantedSpecies, Planted);

        FIngredientTableRow Other;
        Other.DisplayName = FText::FromString(TEXT("Другая трава той же ниши"));
        Other.GardenNiche = EGardenNiche::SunnyBed;
        Other.RarityWeight = 100;
        Table->AddRow(OtherSpecies, Other);

        Registry->LoadFromDataTable(Table);
        return Registry;
    }

    // SpawnControllerAndBeginPlay -- вынесено в TestWorldHelpers.h (2026-09-04,
    // ODR-дубликат с ArtifactInventoryTest.cpp, тот же класс проблемы, что уже
    // решён для SpawnAndBeginPlay).
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistGardenPlanting_RequiresGardenPlotAtCell,
    "Herbalist.GardenPlanting.RequiresGardenPlotAtCell",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistGardenPlanting_RequiresGardenPlotAtCell::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;

    // (3,3) has no garden plot registered at all -- refused with a log, not
    // a silent "sure, why not" (тот же класс валидации, что RegisterGardenPlot).
    TestFalse(TEXT("PlantSeedInCell refuses a cell with no registered garden plot"),
        Manager->PlantSeedInCell(FIntPoint(3, 3), PlantedSpecies, EGardenNiche::SunnyBed));

    const FGridCell* Cell = Manager->GetCellConst(3, 3);
    TestTrue(TEXT("Cell exists"), Cell != nullptr);
    if (Cell)
    {
        TestEqual(TEXT("PlantedSpeciesID stays NAME_None after a refused planting"), Cell->PlantedSpeciesID, NAME_None);
    }

    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistGardenPlanting_RequiresMatchingNiche,
    "Herbalist.GardenPlanting.RequiresMatchingNiche",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistGardenPlanting_RequiresMatchingNiche::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;

    Manager->RegisterGardenPlot(FIntPoint(4, 4), EGardenNiche::SunnyBed);

    // A species whose own GardenNiche is RootCellar cannot be planted into a
    // SunnyBed plot -- mismatch refused with a log (same validation class as
    // RegisterGardenPlot/SetGardenPlot's own unknown-niche refusal).
    TestFalse(TEXT("Niche mismatch is refused"),
        Manager->PlantSeedInCell(FIntPoint(4, 4), FName(TEXT("BogHerb")), EGardenNiche::RootCellar));

    const FGridCell* Cell = Manager->GetCellConst(4, 4);
    if (TestTrue(TEXT("Cell exists"), Cell != nullptr))
    {
        TestEqual(TEXT("Mismatched planting does not touch PlantedSpeciesID"), Cell->PlantedSpeciesID, NAME_None);
    }

    // Same plot, matching niche -- succeeds.
    TestTrue(TEXT("Matching niche succeeds"),
        Manager->PlantSeedInCell(FIntPoint(4, 4), PlantedSpecies, EGardenNiche::SunnyBed));
    Cell = Manager->GetCellConst(4, 4);
    if (TestTrue(TEXT("Cell still exists"), Cell != nullptr))
    {
        TestEqual(TEXT("PlantedSpeciesID set to the planted species"), Cell->PlantedSpeciesID, PlantedSpecies);
    }

    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistGardenPlanting_OverridesProbabilisticNichePick,
    "Herbalist.GardenPlanting.OverridesProbabilisticNichePick",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistGardenPlanting_OverridesProbabilisticNichePick::RunTest(const FString& Parameters)
{
    // Ядро запроса пользователя: посадка -- это "здесь растёт вот это", не
    // "склоняется к этому". OtherSpecies несёт RarityWeight=100 против
    // PlantedSpecies=1 -- под обычным вероятностным GetRandomResourceForNiche
    // OtherSpecies выигрывал бы почти всегда; SpawnOneResourceInCell должен
    // тем не менее выбрать ИМЕННО посаженный вид, каждый раз, без исключений.
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    UIngredientRegistrySubsystem* Registry = MakeTwoCandidateNicheRegistry();
    if (!TestNotNull(TEXT("Test registry built"), Registry)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;

    Manager->RegisterGardenPlot(FIntPoint(6, 6), EGardenNiche::SunnyBed);
    if (!TestTrue(TEXT("Planting succeeds"), Manager->PlantSeedInCell(FIntPoint(6, 6), PlantedSpecies, EGardenNiche::SunnyBed)))
    {
        Manager->Destroy();
        return false;
    }

    FGridCell* Cell = Manager->GetCell(6, 6);
    if (!TestNotNull(TEXT("Cell exists"), Cell)) { Manager->Destroy(); return false; }

    const EGardenNiche* PlotNiche = Manager->GardenPlots.Find(FIntPoint(6, 6));
    if (!TestNotNull(TEXT("Plot niche resolvable"), PlotNiche)) { Manager->Destroy(); return false; }

    const FHarvestContext Context = Manager->BuildHarvestContextForCell(*Cell);

    // WorldRNG advances with each call (position jitter, and would advance
    // further still for the weighted pick if the override were bypassed) --
    // repeating several times over that changing internal state is enough to
    // rule out "got lucky once", without needing to touch the RNG directly.
    for (int32 i = 0; i < 8; ++i)
    {
        Cell->ResourceActors.Empty();
        const bool bSpawned = Manager->SpawnOneResourceInCell(*Cell, Context, PlotNiche, nullptr, Registry);
        if (!TestTrue(FString::Printf(TEXT("Iteration %d: spawn succeeds"), i), bSpawned))
        {
            continue;
        }
        if (!TestEqual(FString::Printf(TEXT("Iteration %d: exactly one actor registered"), i), Cell->ResourceActors.Num(), 1))
        {
            continue;
        }
        AHerbalistResourceActor* Spawned = Cell->ResourceActors[0].Get();
        if (TestNotNull(FString::Printf(TEXT("Iteration %d: actor alive"), i), Spawned))
        {
            TestEqual(FString::Printf(TEXT("Iteration %d: planted species chosen, not the heavier-weighted alternative"), i),
                Spawned->GetIngredientID(), PlantedSpecies);
            Spawned->Destroy();
        }
    }

    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistGardenPlanting_SurvivesHarvestAndRegrowthReuse,
    "Herbalist.GardenPlanting.SurvivesHarvestAndRegrowthReuse",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistGardenPlanting_SurvivesHarvestAndRegrowthReuse::RunTest(const FString& Parameters)
{
    // "Посадка НЕ разовая: должна переживать отрастание после сбора"
    // (прямой запрос). StartRegeneration's real timer doesn't fire inside an
    // automation test (5-10 minutes of game time, see ResourceRegrowthTest.cpp's
    // own comment on this) -- but StartRegeneration calls this EXACT SAME
    // SpawnOneResourceInCell on the SAME Cell when its timer does fire, so
    // proving the field survives a second direct call to it (after simulating
    // a harvest by clearing ResourceActors, the way OnResourceCollected does)
    // is the honest equivalent, not a shortcut.
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    UIngredientRegistrySubsystem* Registry = MakeTwoCandidateNicheRegistry();
    if (!TestNotNull(TEXT("Test registry built"), Registry)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;

    // Same cell (6,6) as OverridesProbabilisticNichePick above -- proven
    // clear of any real static geometry on the persistent editor test level
    // (L_TestDev has a real landscape/authored content, unlike a synthetic
    // test world; an arbitrary cell can genuinely be blocked by a rock or
    // prop, which is a property of the level, not of PlantSeedInCell).
    Manager->RegisterGardenPlot(FIntPoint(6, 6), EGardenNiche::SunnyBed);
    Manager->PlantSeedInCell(FIntPoint(6, 6), PlantedSpecies, EGardenNiche::SunnyBed);

    FGridCell* Cell = Manager->GetCell(6, 6);
    if (!TestNotNull(TEXT("Cell exists"), Cell)) { Manager->Destroy(); return false; }
    const EGardenNiche* PlotNiche = Manager->GardenPlots.Find(FIntPoint(6, 6));
    const FHarvestContext Context = Manager->BuildHarvestContextForCell(*Cell);

    // "Initial planting" growth.
    TestTrue(TEXT("Initial spawn succeeds"), Manager->SpawnOneResourceInCell(*Cell, Context, PlotNiche, nullptr, Registry));
    TestEqual(TEXT("Initial spawn is the planted species"), Cell->ResourceActors.Num() == 1 ? Cell->ResourceActors[0]->GetIngredientID() : NAME_None, PlantedSpecies);

    // Simulate a harvest -- the resource is gone, but the planting itself is
    // NOT a property of the resource actor, it's on the cell.
    for (const TWeakObjectPtr<AHerbalistResourceActor>& R : Cell->ResourceActors)
    {
        if (R.IsValid()) R->Destroy();
    }
    Cell->ResourceActors.Empty();
    TestEqual(TEXT("Planting survives the harvest itself (field lives on the cell, not the actor)"), Cell->PlantedSpeciesID, PlantedSpecies);

    // "Regrowth" -- the exact same function StartRegeneration's timer callback
    // calls, on the same Cell, still holding PlantedSpeciesID.
    TestTrue(TEXT("Regrowth-equivalent spawn succeeds"), Manager->SpawnOneResourceInCell(*Cell, Context, PlotNiche, nullptr, Registry));
    if (TestEqual(TEXT("Exactly one actor after regrowth"), Cell->ResourceActors.Num(), 1))
    {
        TestEqual(TEXT("Regrowth reproduces the SAME planted species, not a fresh probabilistic roll"),
            Cell->ResourceActors[0]->GetIngredientID(), PlantedSpecies);
    }

    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistGardenPlanting_PersistsThroughSaveLoad,
    "Herbalist.GardenPlanting.PersistsThroughSaveLoad",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistGardenPlanting_PersistsThroughSaveLoad::RunTest(const FString& Parameters)
{
    // Тот же класс пробела и тот же обход GameInstanceSubsystem, что уже
    // Herbalist.Community.GardenPlotsSurviveSaveLoad (CommunityTest.cpp) --
    // CaptureSaveCells/ApplySaveCells не трогают GameInstance, напрямую
    // воспроизводимы в тесте.
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;

    Manager->RegisterGardenPlot(FIntPoint(8, 8), EGardenNiche::SunnyBed);
    if (!TestTrue(TEXT("Planting succeeds"), Manager->PlantSeedInCell(FIntPoint(8, 8), PlantedSpecies, EGardenNiche::SunnyBed)))
    {
        Manager->Destroy();
        return false;
    }

    const TArray<FSavedCellState> Saved = Manager->CaptureSaveCells();

    // Simulate further play after the save point -- a different planting on
    // the same cell should roll back on load, not stick.
    Manager->PlantSeedInCell(FIntPoint(8, 8), OtherSpecies, EGardenNiche::SunnyBed);
    FGridCell* Cell = Manager->GetCell(8, 8);
    if (TestNotNull(TEXT("Cell exists before load"), Cell))
    {
        TestEqual(TEXT("Sanity: post-save replanting took effect before load"), Cell->PlantedSpeciesID, OtherSpecies);
    }

    Manager->ApplySaveCells(Saved);

    Cell = Manager->GetCell(8, 8);
    if (TestNotNull(TEXT("Cell exists after load"), Cell))
    {
        TestEqual(TEXT("PlantedSpeciesID restored to what it was at save time, not the post-save replanting"),
            Cell->PlantedSpeciesID, PlantedSpecies);
    }

    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistGardenPlanting_PlantingStockDoesNotStackWithOrdinaryIngredient,
    "Herbalist.GardenPlanting.PlantingStockDoesNotStackWithOrdinaryIngredient",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistGardenPlanting_PlantingStockDoesNotStackWithOrdinaryIngredient::RunTest(const FString& Parameters)
{
    // Посадочный материал несёт тот же IngredientID, что обычный собранный
    // ингредиент того же вида (вариант А, FInventoryItem::bIsPlantingStock) --
    // без учёта этого флага в AreItemsStackable они бы молча слились в один
    // стек при первом же AddItem (тот же риск, что комментарий у поля в
    // HerbalistCoreTypes.h предупреждает явно), и PlantSeed/TestNewApply
    // нашли бы в инвентаре не то, что искали.
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;

    AHerbalistPlayerController* PC = SpawnControllerAndBeginPlay(World, Manager);
    if (!TestNotNull(TEXT("PlayerController spawned"), PC) || !TestNotNull(TEXT("InventoryComponent present"), PC->InventoryComponent))
    {
        Manager->Destroy();
        return false;
    }

    FInventoryItem Ordinary;
    Ordinary.IngredientID = PlantedSpecies;
    Ordinary.Count = 1;
    Ordinary.bIsPlantingStock = false;
    PC->InventoryComponent->AddItem(Ordinary, 1);

    FInventoryItem Seed;
    Seed.IngredientID = PlantedSpecies;   // same species/row
    Seed.Count = 1;
    Seed.bIsPlantingStock = true;
    PC->InventoryComponent->AddItem(Seed, 1);

    const TArray<FInventoryItem> Items = PC->InventoryComponent->GetItems();
    if (TestEqual(TEXT("Two distinct slots, not one merged stack"), Items.Num(), 2))
    {
        int32 OrdinaryCount = 0, SeedCount = 0;
        for (const FInventoryItem& Item : Items)
        {
            if (Item.IngredientID != PlantedSpecies) continue;
            if (Item.bIsPlantingStock) { SeedCount += Item.Count; }
            else { OrdinaryCount += Item.Count; }
        }
        TestEqual(TEXT("Ordinary stack keeps its own count"), OrdinaryCount, 1);
        TestEqual(TEXT("Planting stock stack keeps its own count, not merged into the ordinary one"), SeedCount, 1);
    }

    Manager->Destroy();
    return true;
}

#endif // WITH_AUTOMATION_TESTS && WITH_EDITOR
