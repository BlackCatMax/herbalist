// Source/ProjectHerbalist/Private/Tests/PipelineV2Test.cpp
#include "Core/Simulation/Public/SnapshotTypes.h"
#include "Core/Simulation/Public/DeltaTypes.h"
#include "Core/Simulation/Public/CommandTypes.h"
#include "PipelineV2.h"
#include "Core/Types/HerbalistCoreTypes.h"
#include "Math/RandomStream.h"
#include "Misc/AutomationTest.h"

#if WITH_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPipelineV2HarvestTest,
    "ProjectHerbalist.PipelineV2.Harvest",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPipelineV2HarvestTest::RunTest(const FString& Parameters)
{
    // 1. Подготавливаем снапшот мира
    FWorldSnapshot WorldSnap;
    FGridCell Cell;
    Cell.X = 5;
    Cell.Y = 7;
    Cell.Biome = EBiomeType::MixedForest;
    Cell.State.Magnitude = 0.7f;
    Cell.State.Direction.Body = 0.5f;
    Cell.State.Direction.Mind = 0.5f;
    Cell.State.Meta.Distortion = 0.2f;
    Cell.State.Meta.Purity = 0.9f;
    Cell.HarvestStress = 0.0f;
    Cell.bIsWater = false;
    WorldSnap.GridState.Add(FIntPoint(5, 7), Cell);
    WorldSnap.WorldSeed = 12345;

    FInventorySnapshot InvSnap;

    // 2. Строим граф команд
    FCommandGraph CmdGraph;
    FCommandEntry CmdEntry;
    CmdEntry.Primitive = ECommandPrimitive::Harvest;
    CmdEntry.Harvest.TargetCell = FIntPoint(5, 7);
    CmdEntry.Harvest.IngredientID = TEXT("TestHerb");
    CmdEntry.Harvest.Amount = 1;
    CmdGraph.AddCommand(CmdEntry);

    // 3. Детерминированный ГПСЧ
    FRandomStream Rng(Cell.X * 123 + Cell.Y * 456);
    int32 SeedBefore = Rng.GetCurrentSeed();

    // 4. Запуск пайплайна
    FStateDelta Delta = Simulation::ExecutePipeline(WorldSnap, InvSnap, CmdGraph, Rng);

    // 5. Проверки
    const FGridCell* Modified = Delta.WorldChanges.Find(FIntPoint(5, 7));
    if (!TestNotNull(TEXT("WorldChanges contains cell (5,7)"), Modified))
        return false;

    TestEqual(TEXT("HarvestStress increased"), Modified->HarvestStress, 0.1f);
    TestTrue(TEXT("Magnitude changed"), Modified->State.Magnitude != Cell.State.Magnitude);
    TestTrue(TEXT("Distortion changed"), Modified->State.Meta.Distortion != Cell.State.Meta.Distortion);

    TestEqual(TEXT("One inventory operation"), Delta.InventoryOps.Num(), 1);
    if (Delta.InventoryOps.Num() >= 1)
    {
        const FInventoryOperation& Op = Delta.InventoryOps[0];
        TestEqual(TEXT("ContainerID = 0"), Op.ContainerID, 0);
        TestEqual(TEXT("OpType = Add"), Op.OpType, EInventoryOpType::Add);
        TestEqual(TEXT("Ingredient ID"), Op.Ingredient.IngredientID, FName(TEXT("TestHerb")));
        TestEqual(TEXT("Amount = 1"), Op.Amount, 1);
        TestEqual(TEXT("Count = 1"), Op.Ingredient.Count, 1);
        TestTrue(TEXT("bSubjectToDecay"), Op.Ingredient.bSubjectToDecay);
    }

    TestTrue(TEXT("RNG changed"), Rng.GetCurrentSeed() != SeedBefore);
    return true;
}

// Тест игнорирования воды
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPipelineV2HarvestWaterTest,
    "ProjectHerbalist.PipelineV2.HarvestWaterIgnored",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPipelineV2HarvestWaterTest::RunTest(const FString& Parameters)
{
    FWorldSnapshot WorldSnap;
    FGridCell WaterCell;
    WaterCell.X = 3; WaterCell.Y = 3;
    WaterCell.bIsWater = true;
    WaterCell.State.Magnitude = 0.5f;
    WorldSnap.GridState.Add(FIntPoint(3,3), WaterCell);

    FInventorySnapshot InvSnap;
    FCommandGraph CmdGraph;
    FCommandEntry CmdEntry;
    CmdEntry.Primitive = ECommandPrimitive::Harvest;
    CmdEntry.Harvest.TargetCell = FIntPoint(3,3);
    CmdEntry.Harvest.IngredientID = TEXT("WaterHerb");
    CmdEntry.Harvest.Amount = 1;
    CmdGraph.AddCommand(CmdEntry);

    FRandomStream Rng(42);
    FStateDelta Delta = Simulation::ExecutePipeline(WorldSnap, InvSnap, CmdGraph, Rng);

    TestEqual(TEXT("No inventory ops"), Delta.InventoryOps.Num(), 0);
    TestEqual(TEXT("No world changes"), Delta.WorldChanges.Num(), 0);
    return true;
}

#endif