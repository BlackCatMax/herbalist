// Source/ProjectHerbalistTests/Private/Tests/InventoryHarvestFixesTest.cpp
//
// Обход систем после варки (2026-09-14, запрос пользователя «последовательно
// иди по остальным системам, выявляя баги»), раздел «сбор и сумка»:
// - применение зелья в клетку снимало из сумки два зелья: UsePotion сам и
//   Pipeline ещё раз;
// - перенос в пайплайне в неизвестный ему контейнер уничтожал предмет.

#include "Core/Simulation/Public/SnapshotTypes.h"
#include "Core/Simulation/Public/DeltaTypes.h"
#include "Core/Simulation/Public/CommandTypes.h"
#include "PipelineV2.h"
#include "Core/Types/HerbalistCoreTypes.h"
#include "Math/RandomStream.h"
#include "Misc/AutomationTest.h"
#include "Editor.h"
#include "Engine/World.h"

#if WITH_AUTOMATION_TESTS && WITH_EDITOR

#include "TestWorldHelpers.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistPipeline_TransferRefusesContainersUnknownToPipeline,
    "Herbalist.Pipeline.TransferRefusesContainersUnknownToPipeline",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistPipeline_TransferRefusesContainersUnknownToPipeline::RunTest(const FString& Parameters)
{
    FWorldSnapshot WorldSnap;
    FBiomeSnapshot BiomeSnap;

    FInventoryItem Herb;
    Herb.IngredientID = FName(TEXT("Ромашка"));
    Herb.Count = 3;
    FInventorySnapshot InvSnap;
    InvSnap.ContainerContents.Add(0, TArray<FInventoryItem>{ Herb });

    auto Transfer = [&](int32 Source, int32 Target, int32 Amount)
    {
        FCommandEntry Entry;
        Entry.Primitive = ECommandPrimitive::Transfer;
        Entry.Transfer.SourceContainerID = Source;
        Entry.Transfer.TargetContainerID = Target;
        Entry.Transfer.IngredientID = Herb.IngredientID;
        Entry.Transfer.Amount = Amount;
        FCommandBatch Batch;
        Batch.AddCommand(Entry);
        FRandomStream Rng(3);
        return Simulation::ExecutePipeline(WorldSnap, InvSnap, BiomeSnap, Batch, Rng);
    };

    // TestNewTransfer: 0 -> 1. Раньше -- Remove из сумки и Add в контейнер,
    // который никто не применяет.
    const FStateDelta ToUnknown = Transfer(0, 1, 1);
    TestEqual(TEXT("Перенос в неизвестный контейнер -- ни одной операции"), ToUnknown.InventoryOps.Num(), 0);

    const FStateDelta FromUnknown = Transfer(1, 0, 1);
    TestEqual(TEXT("Перенос из неизвестного контейнера -- ни одной операции"), FromUnknown.InventoryOps.Num(), 0);

    // Больше, чем в стопке (3): снятие применяется в пределах стопки, и
    // добавление полных 5 размножало бы предметы.
    const FStateDelta TooMany = Transfer(0, 0, 5);
    if (TestEqual(TEXT("Внутри сумки -- снять и положить"), TooMany.InventoryOps.Num(), 2))
    {
        TestEqual(TEXT("Снято не больше, чем в стопке"), TooMany.InventoryOps[0].Amount, 3);
        TestEqual(TEXT("Положено столько же, сколько снято"), TooMany.InventoryOps[1].Amount, 3);
        TestEqual(TEXT("Число в добавляемом предмете совпадает"), TooMany.InventoryOps[1].Ingredient.Count, 3);
    }
    return true;
}

// Проверяет флаг в команде зелья. Что флаг гасит Remove-операции Pipeline --
// Herbalist.Alchemy.Cauldron.WithdrawnIngredientsAreNotRemovedAgain; что
// UsePotion снимает зелье сам -- автотестом не покрыто (нужен луч из камеры).
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistAlchemy_PotionCommandIsMarkedWithdrawn,
    "Herbalist.Alchemy.PotionCommandIsMarkedWithdrawn",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistAlchemy_PotionCommandIsMarkedWithdrawn::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;

    const int32 QueuedBefore = Manager->GetPendingCommandsForTests().Num();

    FRealState PotionState;
    PotionState.Magnitude = 0.5f;
    Manager->ApplyPotionToCell(2, 3, PotionState);

    const TArray<FCommandEntry>& Pending = Manager->GetPendingCommandsForTests();
    if (TestEqual(TEXT("Одна команда в очереди"), Pending.Num(), QueuedBefore + 1))
    {
        const FCommandEntry& Cmd = Pending.Last();
        TestTrue(TEXT("Команда -- Apply на клетку"), Cmd.Primitive == ECommandPrimitive::Apply && !Cmd.Apply.bIsCrafting);
        TestEqual(TEXT("Клетка цели"), Cmd.Apply.TargetCell, FIntPoint(2, 3));
        // UsePotion уже снял зелье из сумки -- Pipeline не снимает второе.
        TestTrue(TEXT("Зелье помечено снятым"), Cmd.Apply.bIngredientsAlreadyWithdrawn);
    }

    // Применение предметов сумки без предварительного снятия -- как было.
    FInventoryItem Herb;
    Herb.IngredientID = FName(TEXT("Ромашка"));
    Herb.Count = 1;
    Manager->ApplyAlchemyResult(4, 5, { Herb }, FIntent());
    TestFalse(TEXT("ApplyAlchemyResult по умолчанию списывает из сумки"),
        Manager->GetPendingCommandsForTests().Last().Apply.bIngredientsAlreadyWithdrawn);

    Manager->Destroy();
    return true;
}

#endif // WITH_AUTOMATION_TESTS && WITH_EDITOR
