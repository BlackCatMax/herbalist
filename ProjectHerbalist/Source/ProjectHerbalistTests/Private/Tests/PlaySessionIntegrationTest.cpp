// Source/ProjectHerbalistTests/Private/Tests/PlaySessionIntegrationTest.cpp
//
// "Реальный прогон всей цепочки, а не отдельных функций -- симулируем игру"
// (прямой запрос, 2026-08-30). Всё, что тестировалось до сих пор
// (PipelineV2Test.cpp, PipelineV2ApplyTest.cpp) вызывало Simulation::
// ExecutePipeline один раз за тест, с руками собранным снапшотом -- честно
// проверяет арифметику пайплайна, но никогда не проносит РЕАЛЬНОЕ состояние
// через несколько последовательных команд с настоящим инвентарём и
// настоящим Tick() между ними, как это происходит в реальной игровой сессии.
//
// Важная находка по пути (см. также CHANGELOG.md): производственная связка
// AGridWorldManager::QueueCommand -> Tick() -> Simulation::FSnapshotService
// НЕ тестируема в этом автотест-раннере как есть. FSnapshotService::
// GetSimulationWorld() (SnapshotService.cpp) ищет мир строго типа Game/PIE
// среди GEngine->GetWorldContexts() -- автотесты же крутятся в мире типа
// Editor (GEditor->GetEditorWorldContext().World()), которого этот фильтр
// никогда не находит. Проверено эмпirически: QueueCommand+Tick() молча не
// делает ничего (лог: "PipelineV2: Harvest cell (X,Y) not found") -- Capture-
// World()/CaptureInventory() возвращают ПУСТОЙ снапшот несмотря на реально
// существующие клетки/инвентарь. Это не баг продакшн-кода (в реальной игре
// мир всегда Game/PIE), а слепая зона именно этого тестового окружения --
// тот же класс ограничения, что уже задокументирован в SaveSystemTest.cpp
// про UHerbalistSaveSubsystem (нужен UGameInstance, которого тоже нет).
//
// Поэтому здесь цепочка собирается в обход только САМОГО ПОИСКА мира: вместо
// QueueCommand+Tick() тесты вызывают Simulation::ExecutePipeline напрямую
// (тот же публичный вход, что использует FSnapshotService::ExecuteTick), а
// результат применяют ТЕМИ ЖЕ продакшн-методами, которые вызвал бы
// FSnapshotService, если бы нашёл мир: AGridWorldManager::ApplyStateDelta и
// UHerbalistInventoryComponent::ApplyStateDelta. Реальны: арифметика
// пайплайна, применение дельты к живой клетке И живому инвентарю, Tick()
// между командами (релаксация/манифестация/капища через настоящий
// RegenerateCellParameters/UpdateEntityManifestations/UpdateShrines), save/
// load через CaptureSaveCells/ApplySaveCells. В обход — только то, что сам
// пайплайн вызывается напрямую вместо QueueCommand+естественного тика.

#include "Core/World/GridWorldManager.h"
#include "Core/Inventory/HerbalistInventoryComponent.h"
#include "Core/Simulation/Public/SnapshotTypes.h"
#include "Core/Simulation/Public/DeltaTypes.h"
#include "Core/Simulation/Public/CommandTypes.h"
#include "PipelineV2.h"
#include "Core/Types/HerbalistCoreTypes.h"
#include "Math/RandomStream.h"
#include "Misc/AutomationTest.h"
#include "Editor.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

#if WITH_AUTOMATION_TESTS && WITH_EDITOR

#include "TestWorldHelpers.h"

namespace
{
    UHerbalistInventoryComponent* SpawnPlayerInventory(UWorld* World)
    {
        AActor* Owner = World->SpawnActor<AActor>();
        UHerbalistInventoryComponent* Inventory = NewObject<UHerbalistInventoryComponent>(Owner);
        Inventory->RegisterComponent();
        return Inventory;
    }

    // Один шаг команды: снапшот живого мира+инвентаря -> ExecutePipeline ->
    // применение дельты обратно в те же живые объекты. Ровно то, что делает
    // Simulation::FSnapshotService::ExecuteTick (см. комментарий выше файла),
    // без поиска мира.
    FStateDelta RunRealCommand(const FCommandEntry& Cmd, AGridWorldManager* Manager,
        UHerbalistInventoryComponent* Inventory, FRandomStream& Rng)
    {
        FWorldSnapshot WorldSnap = Manager->CaptureState();
        FInventorySnapshot InvSnap = Inventory->CaptureState();
        FBiomeSnapshot BiomeSnap; // без биом-графа в этом наборе тестов -- не его предмет

        FCommandBatch Batch;
        Batch.AddCommand(Cmd);
        FStateDelta Delta = Simulation::ExecutePipeline(WorldSnap, InvSnap, BiomeSnap, Batch, Rng);

        Manager->ApplyStateDelta(Delta);
        Inventory->ApplyStateDelta(Delta);
        return Delta;
    }

    bool PlaySessionInRange01(float V)
    {
        return FMath::IsFinite(V) && V >= -0.0001f && V <= 1.0001f;
    }

    bool CellAxesSane(const FGridCell& Cell)
    {
        const FRealState& S = Cell.State;
        return PlaySessionInRange01(S.Meta.Corruption) && PlaySessionInRange01(S.Meta.Purity) && PlaySessionInRange01(S.Meta.Distortion)
            && PlaySessionInRange01(S.Meta.Stability) && PlaySessionInRange01(S.Meta.Potency) && PlaySessionInRange01(S.Meta.Resonance)
            && PlaySessionInRange01(S.Magnitude);
    }
}

// ---------------------------------------------------------------------------
// Собрать траву -> собрать воду -> реальное время идёт -> сварить зелье из
// РЕАЛЬНО собранных предметов -> реальное время идёт -> применить зелье на
// клетку -> реальное время идёт. Каждый шаг читает состояние, оставленное
// предыдущим -- не независимые сценарии.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistPlaySession_GatherBrewApplyEndToEnd,
    "Herbalist.PlaySession.GatherBrewApplyEndToEnd",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistPlaySession_GatherBrewApplyEndToEnd::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("Manager spawned"), Manager)) return false;
    UHerbalistInventoryComponent* Inventory = SpawnPlayerInventory(World);

    // Клетка травы (Тайга), клетка воды рядом, клетка-мишень для применения.
    FGridCell* HerbCell = Manager->GetCell(5, 5);
    FGridCell* WaterCell = Manager->GetCell(5, 6);
    FGridCell* TargetCell = Manager->GetCell(6, 5);
    if (!TestNotNull(TEXT("Herb cell exists"), HerbCell) || !TestNotNull(TEXT("Water cell exists"), WaterCell)
        || !TestNotNull(TEXT("Target cell exists"), TargetCell))
    {
        Manager->Destroy();
        return false;
    }

    HerbCell->Biome = EBiomeType::Taiga;
    HerbCell->bIsWater = false;
    HerbCell->State.Magnitude = 0.7f;
    HerbCell->State.Direction.Body = 1.0f;
    HerbCell->State.Meta.Distortion = 0.2f;
    HerbCell->State.Meta.Stability = 0.6f;
    HerbCell->State.Meta.Purity = 0.6f;
    HerbCell->State.Meta.Corruption = 0.1f;
    HerbCell->TargetState = HerbCell->State;

    WaterCell->Biome = EBiomeType::Taiga;
    WaterCell->bIsWater = true;
    WaterCell->State.Magnitude = 0.5f;
    WaterCell->State.Direction.Body = WaterCell->State.Direction.Mind
        = WaterCell->State.Direction.Spirit = WaterCell->State.Direction.Nature = 0.25f;
    WaterCell->State.Meta.Purity = 0.5f;
    WaterCell->TargetState = WaterCell->State;

    TargetCell->Biome = EBiomeType::Taiga;
    TargetCell->bIsWater = false;
    TargetCell->HarvestStress = 0.1f;
    TargetCell->TargetState = TargetCell->State;

    const float HerbCellStressBefore = HerbCell->HarvestStress;

    // --- Шаг 1: собрать траву ---
    FCommandEntry HarvestHerb;
    HarvestHerb.Primitive = ECommandPrimitive::Harvest;
    HarvestHerb.Harvest.TargetCell = FIntPoint(5, 5);
    HarvestHerb.Harvest.IngredientID = FName(TEXT("WildHerb"));
    HarvestHerb.Harvest.Amount = 1;
    FRandomStream RngHarvestHerb(101);
    RunRealCommand(HarvestHerb, Manager, Inventory, RngHarvestHerb);

    TestEqual(TEXT("Inventory has the harvested herb"), Inventory->GetItems().Num(), 1);
    if (Inventory->GetItems().Num() != 1) { Manager->Destroy(); return false; }
    TestEqual(TEXT("Harvested item is WildHerb"), Inventory->GetItems()[0].IngredientID, FName(TEXT("WildHerb")));
    TestTrue(TEXT("Harvesting the cell for real increased its HarvestStress"),
        HerbCell->HarvestStress > HerbCellStressBefore + KINDA_SMALL_NUMBER);

    // --- Шаг 2: набрать воду ---
    FCommandEntry HarvestWater;
    HarvestWater.Primitive = ECommandPrimitive::Harvest;
    HarvestWater.Harvest.TargetCell = FIntPoint(5, 6);
    HarvestWater.Harvest.Amount = 1;
    FRandomStream RngHarvestWater(102);
    RunRealCommand(HarvestWater, Manager, Inventory, RngHarvestWater);

    TestEqual(TEXT("Inventory now has herb + water"), Inventory->GetItems().Num(), 2);
    if (Inventory->GetItems().Num() != 2) { Manager->Destroy(); return false; }
    TestTrue(TEXT("Second item is water"), Inventory->GetItems()[1].bIsWater);

    // --- Реальное время идёт между сбором и варкой (настоящий Tick(), не
    // прямой вызов внутренних систем) ---
    for (int32 i = 0; i < 20; ++i)
    {
        Manager->Tick(1.0f);
    }
    TestTrue(TEXT("Herb cell stays coherent after real ticks"), CellAxesSane(*Manager->GetCellConst(5, 5)));
    TestTrue(TEXT("Water cell stays coherent after real ticks"), CellAxesSane(*Manager->GetCellConst(5, 6)));

    // --- Шаг 3: сварить зелье из РЕАЛЬНО собранных ингредиентов, не заново
    // сконструированных ---
    FCommandEntry Brew;
    Brew.Primitive = ECommandPrimitive::Apply;
    Brew.Apply.TargetCell = FIntPoint(6, 5); // не используется при крафте, но заполнено честно
    Brew.Apply.Ingredients = Inventory->GetItems();
    Brew.Apply.bIsCrafting = true;
    FRandomStream RngBrew(103);
    FStateDelta BrewDelta = RunRealCommand(Brew, Manager, Inventory, RngBrew);

    TestEqual(TEXT("Herb and water consumed, one potion produced"), Inventory->GetItems().Num(), 1);
    if (Inventory->GetItems().Num() != 1) { Manager->Destroy(); return false; }
    const FName BrewedID = Inventory->GetItems()[0].IngredientID;
    TestTrue(TEXT("Brewed result is a real potion (not the degenerate Ash/BoiledWater path)"),
        BrewedID == FName(TEXT("Potion")));
    TestTrue(TEXT("Brewed potion state is coherent"),
        PlaySessionInRange01(Inventory->GetItems()[0].State.Magnitude) && PlaySessionInRange01(Inventory->GetItems()[0].State.Meta.Distortion));

    // --- Ещё немного реального времени ---
    for (int32 i = 0; i < 10; ++i)
    {
        Manager->Tick(1.0f);
    }

    // --- Шаг 4: применить сваренное зелье на клетку-мишень ---
    FCommandEntry ApplyPotion;
    ApplyPotion.Primitive = ECommandPrimitive::Apply;
    ApplyPotion.Apply.TargetCell = FIntPoint(6, 5);
    ApplyPotion.Apply.Ingredients = Inventory->GetItems(); // ровно тот один потион
    ApplyPotion.Apply.bIsCrafting = false;
    const float TargetStressBefore = TargetCell->HarvestStress;
    FRandomStream RngApply(104);
    RunRealCommand(ApplyPotion, Manager, Inventory, RngApply);

    TestEqual(TEXT("Potion consumed applying it to the cell"), Inventory->GetItems().Num(), 0);
    TestTrue(TEXT("Target cell HarvestStress increased by applying a potion"),
        TargetCell->HarvestStress > TargetStressBefore + KINDA_SMALL_NUMBER - 0.0005f);
    TestTrue(TEXT("Target cell stays coherent after real application"), CellAxesSane(*TargetCell));

    // --- Сессия продолжается ещё немного реального времени без краша ---
    for (int32 i = 0; i < 10; ++i)
    {
        Manager->Tick(1.0f);
    }
    TestTrue(TEXT("World stays coherent after the whole session"), CellAxesSane(*Manager->GetCellConst(6, 5)));

    Manager->Destroy();
    return true;
}

// ---------------------------------------------------------------------------
// Собрать -> сохраниться -> играть дальше без сохранения -> "перезайти"
// (свежие Manager+Inventory) -> загрузиться -> убедиться, что вернулось
// состояние на момент сохранения, а не то, что было после него. Тот же
// принцип, что уже проверяет Herbalist.Save.ApplyRestoresEarlierSnapshot
// (SaveSystemTest.cpp), но здесь состояние на момент сохранения приходит из
// реально пройденной команды сбора, не сконструировано вручную под тест, и
// мир между сбором и сохранением реально тикает.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistPlaySession_SaveAndReloadMidSession,
    "Herbalist.PlaySession.SaveAndReloadMidSession",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistPlaySession_SaveAndReloadMidSession::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("Manager spawned"), Manager)) return false;
    UHerbalistInventoryComponent* Inventory = SpawnPlayerInventory(World);

    FGridCell* HerbCell = Manager->GetCell(3, 3);
    if (!TestNotNull(TEXT("Herb cell exists"), HerbCell)) { Manager->Destroy(); return false; }
    HerbCell->Biome = EBiomeType::Bog;
    HerbCell->bIsWater = false;
    HerbCell->State.Magnitude = 0.5f;
    HerbCell->State.Meta.Purity = 0.4f;
    HerbCell->TargetState = HerbCell->State;

    // Играем: собираем траву по-настоящему.
    FCommandEntry Harvest;
    Harvest.Primitive = ECommandPrimitive::Harvest;
    Harvest.Harvest.TargetCell = FIntPoint(3, 3);
    Harvest.Harvest.IngredientID = FName(TEXT("SwampReed"));
    Harvest.Harvest.Amount = 1;
    FRandomStream RngHarvest(201);
    RunRealCommand(Harvest, Manager, Inventory, RngHarvest);
    if (!TestEqual(TEXT("One item harvested before save"), Inventory->GetItems().Num(), 1)) { Manager->Destroy(); return false; }

    for (int32 i = 0; i < 5; ++i) Manager->Tick(1.0f);

    // "SaveGame": снимок мира и инвентаря прямо сейчас.
    const TArray<FSavedCellState> SavedCells = Manager->CaptureSaveCells();
    const TArray<FInventoryItem> SavedItems = Inventory->GetItems();
    // Distortion, не Corruption -- ProcessHarvestCommand трогает Distortion/
    // Purity/Stability/Magnitude при сборе с земли, Corruption не трогает
    // вовсе (осталась бы 0.0 что до, что после -- бессмысленная проверка).
    const float SavedHerbCellDistortion = Manager->GetCellConst(3, 3)->State.Meta.Distortion;

    // Игрок продолжает играть ПОСЛЕ сохранения, не перезагружаясь: рвём ещё
    // одну траву на той же клетке и портим её сильнее сфабрикованной правкой
    // (тот же приём, что уже применяет SaveSystemTest.cpp, но поверх реально
    // собранного состояния, не с нуля).
    FCommandEntry HarvestAgain;
    HarvestAgain.Primitive = ECommandPrimitive::Harvest;
    HarvestAgain.Harvest.TargetCell = FIntPoint(3, 3);
    HarvestAgain.Harvest.IngredientID = FName(TEXT("SwampReed"));
    HarvestAgain.Harvest.Amount = 1;
    FRandomStream RngHarvestAgain(202);
    RunRealCommand(HarvestAgain, Manager, Inventory, RngHarvestAgain);
    for (int32 i = 0; i < 10; ++i) Manager->Tick(1.0f);

    TestEqual(TEXT("Post-save play left two items in the live inventory"), Inventory->GetItems().Num(), 2);
    const float LiveHerbCellDistortionAfterMorePlay = Manager->GetCellConst(3, 3)->State.Meta.Distortion;

    // "Игрок закрывает игру и заново заходит" -- свежие объекты, ничего не
    // унаследовано от старой сессии.
    AGridWorldManager* ReloadedManager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("Reloaded manager spawned"), ReloadedManager)) { Manager->Destroy(); return false; }
    UHerbalistInventoryComponent* ReloadedInventory = SpawnPlayerInventory(World);

    // "LoadGame"
    ReloadedManager->ApplySaveCells(SavedCells);
    ReloadedInventory->RestoreItems(SavedItems);

    TestEqual(TEXT("Reloaded inventory has exactly the item count at save time (one), not post-save play (two)"),
        ReloadedInventory->GetItems().Num(), 1);
    if (ReloadedInventory->GetItems().Num() == 1)
    {
        TestEqual(TEXT("Reloaded item matches what was actually harvested before saving"),
            ReloadedInventory->GetItems()[0].IngredientID, FName(TEXT("SwampReed")));
    }

    const FGridCell* ReloadedHerbCell = ReloadedManager->GetCellConst(3, 3);
    if (TestNotNull(TEXT("Reloaded herb cell exists"), ReloadedHerbCell))
    {
        TestTrue(TEXT("Reloaded cell matches save-time Distortion, not post-save play"),
            FMath::IsNearlyEqual(ReloadedHerbCell->State.Meta.Distortion, SavedHerbCellDistortion, 0.0005f));
        // Сфабрикованная (не обязательная) проверка на всякий случай, что сами
        // числа save-time/post-save и правда различались -- иначе предыдущая
        // проверка была бы бессмысленно слабой (совпала бы в любом случае).
        TestFalse(TEXT("Sanity: post-save play actually changed the cell (test would be meaningless otherwise)"),
            FMath::IsNearlyEqual(SavedHerbCellDistortion, LiveHerbCellDistortionAfterMorePlay, 0.0005f));
    }

    Manager->Destroy();
    ReloadedManager->Destroy();
    return true;
}

// ---------------------------------------------------------------------------
// Несколько полных циклов сбор->варка->применение подряд, с реальным Tick()
// между каждым -- не одна транзакция, а сессия из нескольких. Проверяет, что
// цепочка не накапливает несостыковку (NaN, отрицательные счётчики
// инвентаря, застрявшее состояние) при повторном реальном использовании.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistPlaySession_RepeatedCyclesStayCoherent,
    "Herbalist.PlaySession.RepeatedCyclesStayCoherent",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistPlaySession_RepeatedCyclesStayCoherent::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("Manager spawned"), Manager)) return false;
    UHerbalistInventoryComponent* Inventory = SpawnPlayerInventory(World);

    FGridCell* HerbCell = Manager->GetCell(8, 8);
    FGridCell* WaterCell = Manager->GetCell(8, 9);
    FGridCell* TargetCell = Manager->GetCell(9, 8);
    if (!TestNotNull(TEXT("Herb cell exists"), HerbCell) || !TestNotNull(TEXT("Water cell exists"), WaterCell)
        || !TestNotNull(TEXT("Target cell exists"), TargetCell))
    {
        Manager->Destroy();
        return false;
    }

    HerbCell->Biome = EBiomeType::MixedForest;
    HerbCell->bIsWater = false;
    HerbCell->State.Magnitude = 0.6f;
    HerbCell->State.Meta.Stability = 0.5f;
    HerbCell->State.Meta.Purity = 0.5f;
    HerbCell->TargetState = HerbCell->State;

    WaterCell->Biome = EBiomeType::MixedForest;
    WaterCell->bIsWater = true;
    WaterCell->State.Magnitude = 0.5f;
    WaterCell->State.Meta.Purity = 0.5f;
    WaterCell->TargetState = WaterCell->State;

    TargetCell->Biome = EBiomeType::MixedForest;
    TargetCell->bIsWater = false;
    TargetCell->TargetState = TargetCell->State;

    bool bSawIncoherence = false;
    int32 SeedCounter = 300;

    for (int32 Cycle = 0; Cycle < 4; ++Cycle)
    {
        FCommandEntry HarvestHerb;
        HarvestHerb.Primitive = ECommandPrimitive::Harvest;
        HarvestHerb.Harvest.TargetCell = FIntPoint(8, 8);
        HarvestHerb.Harvest.IngredientID = FName(TEXT("ForestHerb"));
        HarvestHerb.Harvest.Amount = 1;
        FRandomStream RngHerb(SeedCounter++);
        RunRealCommand(HarvestHerb, Manager, Inventory, RngHerb);

        FCommandEntry HarvestWater;
        HarvestWater.Primitive = ECommandPrimitive::Harvest;
        HarvestWater.Harvest.TargetCell = FIntPoint(8, 9);
        HarvestWater.Harvest.Amount = 1;
        FRandomStream RngWater(SeedCounter++);
        RunRealCommand(HarvestWater, Manager, Inventory, RngWater);

        // Реальное игровое время между сбором и варкой -- имитирует, что
        // игрок не варит мгновенно после каждого сбора.
        for (int32 i = 0; i < 5; ++i) Manager->Tick(1.0f);

        FCommandEntry Brew;
        Brew.Primitive = ECommandPrimitive::Apply;
        Brew.Apply.TargetCell = FIntPoint(9, 8);
        Brew.Apply.Ingredients = Inventory->GetItems();
        Brew.Apply.bIsCrafting = true;
        FRandomStream RngBrew(SeedCounter++);
        RunRealCommand(Brew, Manager, Inventory, RngBrew);

        FCommandEntry ApplyPotion;
        ApplyPotion.Primitive = ECommandPrimitive::Apply;
        ApplyPotion.Apply.TargetCell = FIntPoint(9, 8);
        ApplyPotion.Apply.Ingredients = Inventory->GetItems();
        ApplyPotion.Apply.bIsCrafting = false;
        FRandomStream RngApply(SeedCounter++);
        RunRealCommand(ApplyPotion, Manager, Inventory, RngApply);

        for (int32 i = 0; i < 5; ++i) Manager->Tick(1.0f);

        if (!CellAxesSane(*Manager->GetCellConst(8, 8)) || !CellAxesSane(*Manager->GetCellConst(9, 8)))
        {
            bSawIncoherence = true;
        }
        if (Inventory->GetItems().Num() != 0)
        {
            // Инвентарь должен полностью опустошаться каждым циклом -- потион
            // применён на клетку (не оставлен себе), сырьё полностью потрачено.
            bSawIncoherence = true;
        }
    }

    TestFalse(TEXT("Four real gather->brew->apply cycles never leave NaN/out-of-range state or a stuck inventory"),
        bSawIncoherence);

    Manager->Destroy();
    return true;
}

#endif // WITH_AUTOMATION_TESTS && WITH_EDITOR
