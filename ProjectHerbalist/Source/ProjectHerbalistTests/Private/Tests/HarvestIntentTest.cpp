// Source/ProjectHerbalistTests/Private/Tests/HarvestIntentTest.cpp
//
// Намерение сбора (DESIGN_Community_And_Homestead.md §2.4, PlantSeed,
// 2026-09-04, прямой запрос пользователя): "механика использования садов
// должна различать сбор растения ДЛЯ ПОСАДКИ в грядку и сбор ТОГО ЖЕ
// растения для варки". Тот же приём полного пайплайна, что уже
// GatheringToolTest.cpp/MoonPhaseTest.cpp применяют к соседним модификаторам
// сбора (Tool/MoonPhase) — здесь проверяется единственная новая переменная
// в цепочке: FHarvestCommand::bForPlanting -> FInventoryItem::bIsPlantingStock
// (PipelineV2.cpp::ProcessHarvestCommand). Резолв намерения из
// AHerbalistPlayerController::CurrentHarvestIntent (SetHarvestIntent) сюда
// не входит — та же граница, что уже у Tool (AGridWorldManager::
// OnResourceCollected читает контроллер ДО постановки команды, Pipeline
// самого контроллера не видит).

#include "Core/Simulation/Public/SnapshotTypes.h"
#include "Core/Simulation/Public/DeltaTypes.h"
#include "Core/Simulation/Public/CommandTypes.h"
#include "PipelineV2.h"
#include "Core/Types/HerbalistCoreTypes.h"
#include "Math/RandomStream.h"
#include "Misc/AutomationTest.h"

#if WITH_AUTOMATION_TESTS

namespace
{
    // Тот же приём "нейтральной клетки" и структура вызова, что уже
    // GatheringToolTest.cpp::HarvestWithTool — сравниваем результат не на
    // магические числа, а на то, что реально проверяется здесь: флаг
    // предмета, не его State.
    FInventoryItem HarvestWithIntent(bool bForPlanting)
    {
        FGridCell Cell;
        Cell.X = 5; Cell.Y = 5;
        Cell.Biome = EBiomeType::MixedForest;
        Cell.bIsWater = false;
        Cell.State.Magnitude = 0.5f;
        Cell.State.Direction.Body = 0.25f; Cell.State.Direction.Mind = 0.25f;
        Cell.State.Direction.Spirit = 0.25f; Cell.State.Direction.Nature = 0.25f;
        Cell.State.Meta.Purity = 0.5f; Cell.State.Meta.Corruption = 0.2f;
        Cell.State.Meta.Distortion = 0.2f; Cell.State.Meta.Stability = 0.5f;
        Cell.State.Meta.Potency = 0.5f; Cell.State.Meta.Resonance = 0.5f;

        FWorldSnapshot WorldSnap;
        WorldSnap.GridState.Add(FIntPoint(Cell.X, Cell.Y), Cell);
        WorldSnap.WorldSeed = 9001;

        FInventorySnapshot InvSnap;
        FBiomeSnapshot BiomeSnap;

        FCommandBatch CmdBatch;
        FCommandEntry CmdEntry;
        CmdEntry.Primitive = ECommandPrimitive::Harvest;
        CmdEntry.Harvest.TargetCell = FIntPoint(Cell.X, Cell.Y);
        CmdEntry.Harvest.IngredientID = TEXT("IntentProbe");
        CmdEntry.Harvest.Amount = 1;
        CmdEntry.Harvest.BaseState = Cell.State;
        CmdEntry.Harvest.Resilience = 1.0f;
        CmdEntry.Harvest.MoonPhase = EMoonPhase::NewMoon;
        CmdEntry.Harvest.Tool = EGatheringTool::BareHands;
        CmdEntry.Harvest.bForPlanting = bForPlanting;
        CmdBatch.AddCommand(CmdEntry);

        FRandomStream Rng(9001);
        FStateDelta Delta = Simulation::ExecutePipeline(WorldSnap, InvSnap, BiomeSnap, CmdBatch, Rng);
        check(Delta.InventoryOps.Num() == 1);
        return Delta.InventoryOps[0].Ingredient;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistHarvestIntent_BrewGivesOrdinaryIngredient,
    "Herbalist.HarvestIntent.BrewGivesOrdinaryIngredient",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistHarvestIntent_BrewGivesOrdinaryIngredient::RunTest(const FString& Parameters)
{
    const FInventoryItem Item = HarvestWithIntent(/*bForPlanting=*/false);
    TestFalse(TEXT("bForPlanting=false (default, SetHarvestIntent brew) yields an ordinary ingredient, not planting stock"),
        Item.bIsPlantingStock);
    TestEqual(TEXT("Same IngredientID as any ordinary harvest -- not a separate row/species"), Item.IngredientID, FName(TEXT("IntentProbe")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistHarvestIntent_SeedGivesPlantingStockOfTheSameSpecies,
    "Herbalist.HarvestIntent.SeedGivesPlantingStockOfTheSameSpecies",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistHarvestIntent_SeedGivesPlantingStockOfTheSameSpecies::RunTest(const FString& Parameters)
{
    // Прямой запрос: "сбор ДЛЯ ПОСАДКИ ... отличается от сбора ТОГО ЖЕ
    // растения для варки" -- тот же куст (тот же IngredientID), другой
    // предмет в инвентаре (вариант А: bool-поле, не отдельный ряд таблицы).
    const FInventoryItem Item = HarvestWithIntent(/*bForPlanting=*/true);
    TestTrue(TEXT("bForPlanting=true (SetHarvestIntent seed) yields planting stock"), Item.bIsPlantingStock);
    TestEqual(TEXT("Still the same species/IngredientID as the ordinary harvest of the same plant"),
        Item.IngredientID, FName(TEXT("IntentProbe")));
    return true;
}

#endif
