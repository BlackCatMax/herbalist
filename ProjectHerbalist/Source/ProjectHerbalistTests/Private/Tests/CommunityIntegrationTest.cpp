// Source/ProjectHerbalistTests/Private/Tests/CommunityIntegrationTest.cpp
//
// "Реальный прогон всей цепочки, а не отдельных функций -- симулируем игру"
// (тот же принцип, что PlaySessionIntegrationTest.cpp) — но для общинного
// кластера (DESIGN_Community_And_Homestead.md §1, 2026-08-31): CommunityTest
// .cpp проверяет OfferToCommunity/TryTradeWithCommunity на синтетических
// FInventoryItem, собранных вручную; здесь та же механика подключается к
// РЕАЛЬНО собранному предмету, прошедшему настоящий Simulation::
// ExecutePipeline (Harvest), а не к числам, придуманным для теста. Один
// сеанс, оба направления Молвы (растёт от хорошего подношения, падает от
// плохого), не независимые сценарии.

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
    UHerbalistInventoryComponent* SpawnCommunityTestInventory(UWorld* World)
    {
        AActor* Owner = World->SpawnActor<AActor>();
        UHerbalistInventoryComponent* Inventory = NewObject<UHerbalistInventoryComponent>(Owner);
        Inventory->RegisterComponent();
        return Inventory;
    }

    // Тот же приём, что RunRealCommand в PlaySessionIntegrationTest.cpp —
    // не задублирован оттуда напрямую (namespace-локальные хелперы разных
    // файлов не видны друг другу в Unity-сборке без коллизии имён), но
    // структурно идентичен.
    FStateDelta RunRealCommunityCommand(const FCommandEntry& Cmd, AGridWorldManager* Manager,
        UHerbalistInventoryComponent* Inventory, FRandomStream& Rng)
    {
        FWorldSnapshot WorldSnap = Manager->CaptureState();
        FInventorySnapshot InvSnap = Inventory->CaptureState();
        FBiomeSnapshot BiomeSnap;

        FCommandBatch Batch;
        Batch.AddCommand(Cmd);
        FStateDelta Delta = Simulation::ExecutePipeline(WorldSnap, InvSnap, BiomeSnap, Batch, Rng);

        Manager->ApplyStateDelta(Delta);
        Inventory->ApplyStateDelta(Delta);
        return Delta;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistCommunitySession_HarvestOfferTradeEndToEnd,
    "Herbalist.PlaySession.CommunityHarvestOfferEndToEnd",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistCommunitySession_HarvestOfferTradeEndToEnd::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("Manager spawned"), Manager)) return false;
    UHerbalistInventoryComponent* Inventory = SpawnCommunityTestInventory(World);

    TestEqual(TEXT("Molva starts at zero"), Manager->Molva, 0.0f);

    // --- Клетка чистой, некорумпированной травы ---
    FGridCell* PureCell = Manager->GetCell(4, 4);
    if (!TestNotNull(TEXT("Pure cell exists"), PureCell)) { Manager->Destroy(); return false; }
    PureCell->Biome = EBiomeType::MixedForest;
    PureCell->bIsWater = false;
    PureCell->State.Magnitude = 0.6f;
    PureCell->State.Meta.Purity = 0.85f;
    PureCell->State.Meta.Corruption = 0.05f;
    PureCell->State.Meta.Distortion = 0.1f;
    PureCell->State.Meta.Stability = 0.7f;
    PureCell->TargetState = PureCell->State;

    // --- Шаг 1: собрать реальную чистую траву (настоящий пайплайн, не
    // вручную собранный FInventoryItem) ---
    FCommandEntry HarvestPure;
    HarvestPure.Primitive = ECommandPrimitive::Harvest;
    HarvestPure.Harvest.TargetCell = FIntPoint(4, 4);
    HarvestPure.Harvest.IngredientID = FName(TEXT("PureCommunityHerb"));
    HarvestPure.Harvest.Amount = 1;
    FRandomStream RngHarvestPure(201);
    RunRealCommunityCommand(HarvestPure, Manager, Inventory, RngHarvestPure);

    TestEqual(TEXT("Inventory has the harvested herb"), Inventory->GetItems().Num(), 1);
    if (Inventory->GetItems().Num() != 1) { Manager->Destroy(); return false; }

    // --- Шаг 2: реальное время идёт (настоящий Tick, не мгновенный переход) ---
    for (int32 i = 0; i < 5; ++i)
    {
        Manager->Tick(1.0f);
    }

    // --- Шаг 3: поднести общине то, что реально собрано ---
    const float MolvaBeforeGoodOffer = Manager->Molva;
    Manager->OfferToCommunity(Inventory->GetItems());
    // Тем же порядком, что делает AHerbalistPlayerController::OfferToCommunity
    // -- подношение списывает предмет из инвентаря.
    Inventory->RemoveItem(0, 1);

    TestTrue(TEXT("Offering a real, pure harvest raises Molva above where it started"),
        Manager->Molva > MolvaBeforeGoodOffer);
    TestEqual(TEXT("Inventory empty after the offering is consumed"), Inventory->GetItems().Num(), 0);

    const float MolvaAfterGoodOffer = Manager->Molva;

    // --- Клетка испорченной, но всё ещё честно собираемой травы ---
    FGridCell* CorruptCell = Manager->GetCell(3, 3);
    if (!TestNotNull(TEXT("Corrupt cell exists"), CorruptCell)) { Manager->Destroy(); return false; }
    CorruptCell->Biome = EBiomeType::Bog;
    CorruptCell->bIsWater = false;
    CorruptCell->State.Magnitude = 0.5f;
    CorruptCell->State.Meta.Purity = 0.1f;
    CorruptCell->State.Meta.Corruption = 0.85f;
    CorruptCell->State.Meta.Distortion = 0.6f;
    CorruptCell->State.Meta.Stability = 0.3f;
    CorruptCell->TargetState = CorruptCell->State;

    FCommandEntry HarvestCorrupt;
    HarvestCorrupt.Primitive = ECommandPrimitive::Harvest;
    HarvestCorrupt.Harvest.TargetCell = FIntPoint(3, 3);
    HarvestCorrupt.Harvest.IngredientID = FName(TEXT("CorruptCommunityHerb"));
    HarvestCorrupt.Harvest.Amount = 1;
    FRandomStream RngHarvestCorrupt(202);
    RunRealCommunityCommand(HarvestCorrupt, Manager, Inventory, RngHarvestCorrupt);

    TestEqual(TEXT("Inventory has the corrupt herb"), Inventory->GetItems().Num(), 1);
    if (Inventory->GetItems().Num() != 1) { Manager->Destroy(); return false; }

    // --- Поднести испорченный сбор -- Молва должна ПАДАТЬ, не расти дальше,
    // тот же накопитель, оба направления в одном сеансе. ---
    Manager->OfferToCommunity(Inventory->GetItems());
    Inventory->RemoveItem(0, 1);

    TestTrue(TEXT("Offering a real, corrupt harvest lowers Molva from where the good offering left it"),
        Manager->Molva < MolvaAfterGoodOffer);

    // --- Торговля в том же сеансе -- предмета для обмена в инвентаре больше
    // нет (только что подарен общине), запрошенный ингредиент неизвестен
    // реестру этого тестового окружения -- проверяем, что честный отказ не
    // роняет сессию, реальный путь, не изолированная функция. ---
    FInventoryItem NothingLeftToOffer;
    NothingLeftToOffer.IngredientID = FName(TEXT("CorruptCommunityHerb"));
    NothingLeftToOffer.Count = 0;
    FInventoryItem Received;
    const bool bTraded = Manager->TryTradeWithCommunity(NothingLeftToOffer, FName(TEXT("SomeWantedHerb")), Received);
    TestFalse(TEXT("Trading with nothing left to give fails cleanly, session continues"), bTraded);

    Manager->Destroy();
    return true;
}

#endif // WITH_AUTOMATION_TESTS && WITH_EDITOR
