// Source/ProjectHerbalistTests/Private/Tests/GatheringToolTest.cpp
//
// Инструмент сбора (DESIGN_Community_And_Homestead.md §2.3, 2026-08-31):
// множитель качества (не шанса спавна) применяется к Magnitude/Potency/
// Resonance собранного предмета в GenerateHarvestResult (PipelineV2.cpp).
// Тот же паттерн полного пайплайна, что MoonPhaseTest.cpp — изолируем
// эффект инструмента нейтральной клеткой и Resilience=1.0 (Base==Cell,
// биом/сопротивляемость не вносят разброса), сравниваем относительно, не
// на магические числа: HarvestBiomeWeight/множители могут отличаться в
// тестовом окружении, где UHerbalistSettings не проинициализирован
// игровым режимом (тот же довод, что уже в ResilienceTest.cpp).

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
    FGridCell GatheringToolMakeNeutralCell()
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
        return Cell;
    }

    FRealState HarvestWithTool(EGatheringTool Tool, bool bIronAverse, bool bDelicate, int32 Seed)
    {
        const FGridCell Cell = GatheringToolMakeNeutralCell();
        FWorldSnapshot WorldSnap;
        WorldSnap.GridState.Add(FIntPoint(Cell.X, Cell.Y), Cell);
        WorldSnap.WorldSeed = Seed;

        FInventorySnapshot InvSnap;
        FBiomeSnapshot BiomeSnap;

        FCommandBatch CmdBatch;
        FCommandEntry CmdEntry;
        CmdEntry.Primitive = ECommandPrimitive::Harvest;
        CmdEntry.Harvest.TargetCell = FIntPoint(Cell.X, Cell.Y);
        CmdEntry.Harvest.IngredientID = TEXT("ToolProbe");
        CmdEntry.Harvest.Amount = 1;
        CmdEntry.Harvest.BaseState = Cell.State;   // Base == Cell -> Resilience/биом не вносят разброса
        CmdEntry.Harvest.Resilience = 1.0f;
        CmdEntry.Harvest.MoonPhase = EMoonPhase::NewMoon; // без лунного буста, тестируем только инструмент
        CmdEntry.Harvest.Tool = Tool;
        CmdEntry.Harvest.bIronAverse = bIronAverse;
        CmdEntry.Harvest.bDelicate = bDelicate;
        CmdBatch.AddCommand(CmdEntry);

        FRandomStream Rng(Seed);
        FStateDelta Delta = Simulation::ExecutePipeline(WorldSnap, InvSnap, BiomeSnap, CmdBatch, Rng);
        check(Delta.InventoryOps.Num() == 1);
        return Delta.InventoryOps[0].Ingredient.State;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistGatheringTool_IronBeatsBareHandsOnOrdinaryHerb,
    "Herbalist.GatheringTool.IronBeatsBareHandsOnOrdinaryHerb",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistGatheringTool_IronBeatsBareHandsOnOrdinaryHerb::RunTest(const FString& Parameters)
{
    const FRealState BareHands = HarvestWithTool(EGatheringTool::BareHands, false, false, 8001);
    const FRealState Iron      = HarvestWithTool(EGatheringTool::IronBlade, false, false, 8001);

    TestTrue(TEXT("Iron gives higher Magnitude than bare hands on an ordinary herb"), Iron.Magnitude > BareHands.Magnitude);
    TestTrue(TEXT("Iron gives higher Potency than bare hands on an ordinary herb"), Iron.Meta.Potency > BareHands.Meta.Potency);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistGatheringTool_CopperAndBoneMatchEachOtherOnOrdinaryHerb,
    "Herbalist.GatheringTool.CopperAndBoneMatchEachOtherOnOrdinaryHerb",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistGatheringTool_CopperAndBoneMatchEachOtherOnOrdinaryHerb::RunTest(const FString& Parameters)
{
    const FRealState Copper = HarvestWithTool(EGatheringTool::CopperBlade, false, false, 8002);
    const FRealState Bone   = HarvestWithTool(EGatheringTool::BoneKnife,   false, false, 8002);
    const FRealState Iron   = HarvestWithTool(EGatheringTool::IronBlade,   false, false, 8002);

    TestTrue(TEXT("Copper and Bone give the same Magnitude on an ordinary herb (both non-iron, no special flag)"),
        FMath::IsNearlyEqual(Copper.Magnitude, Bone.Magnitude, 0.001f));
    TestTrue(TEXT("Copper/Bone are cheaper than Iron on an ordinary herb (price of not being iron)"),
        Copper.Magnitude < Iron.Magnitude);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistGatheringTool_IronRuinsIronAverseHerb,
    "Herbalist.GatheringTool.IronRuinsIronAverseHerb",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistGatheringTool_IronRuinsIronAverseHerb::RunTest(const FString& Parameters)
{
    // Плакун-трава/Чистотел: железо отпугивает силу травы.
    const FRealState BareHands = HarvestWithTool(EGatheringTool::BareHands, /*bIronAverse=*/true, false, 8003);
    const FRealState Iron      = HarvestWithTool(EGatheringTool::IronBlade, /*bIronAverse=*/true, false, 8003);

    TestTrue(TEXT("Iron gives a much worse Magnitude than bare hands on an iron-averse herb"), Iron.Magnitude < BareHands.Magnitude);
    TestTrue(TEXT("Iron gives a much worse Potency than bare hands on an iron-averse herb"), Iron.Meta.Potency < BareHands.Meta.Potency);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistGatheringTool_RespectfulToolsAreEqualOnIronAverseHerb,
    "Herbalist.GatheringTool.RespectfulToolsAreEqualOnIronAverseHerb",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistGatheringTool_RespectfulToolsAreEqualOnIronAverseHerb::RunTest(const FString& Parameters)
{
    // Уважительный сбор (не железо) на такой траве не наказывается вовсе —
    // голые руки/медь/кость должны дать РОВНО одинаковый (полный) результат,
    // не обычный базовый множитель инструмента.
    const FRealState BareHands = HarvestWithTool(EGatheringTool::BareHands,   /*bIronAverse=*/true, false, 8004);
    const FRealState Copper    = HarvestWithTool(EGatheringTool::CopperBlade, /*bIronAverse=*/true, false, 8004);
    const FRealState Bone      = HarvestWithTool(EGatheringTool::BoneKnife,   /*bIronAverse=*/true, false, 8004);

    TestTrue(TEXT("Bare hands and Copper match on an iron-averse herb"), FMath::IsNearlyEqual(BareHands.Magnitude, Copper.Magnitude, 0.001f));
    TestTrue(TEXT("Bare hands and Bone match on an iron-averse herb"), FMath::IsNearlyEqual(BareHands.Magnitude, Bone.Magnitude, 0.001f));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistGatheringTool_BoneKnifeBestOnDelicateHerb,
    "Herbalist.GatheringTool.BoneKnifeBestOnDelicateHerb",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistGatheringTool_BoneKnifeBestOnDelicateHerb::RunTest(const FString& Parameters)
{
    // Медуница: костяной нож сохраняет мощь при срезе лучше любого другого
    // инструмента, включая железо (которое здесь не наказывается — трава
    // не bIronAverse, только bDelicate).
    const FRealState Iron = HarvestWithTool(EGatheringTool::IronBlade, false, /*bDelicate=*/true, 8005);
    const FRealState Bone = HarvestWithTool(EGatheringTool::BoneKnife, false, /*bDelicate=*/true, 8005);

    TestTrue(TEXT("Bone knife gives higher Magnitude than iron on a delicate herb"), Bone.Magnitude > Iron.Magnitude);
    TestTrue(TEXT("Bone knife gives higher Potency than iron on a delicate herb"), Bone.Meta.Potency > Iron.Meta.Potency);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistGatheringTool_BoneKnifeBonusOverridesIronAverseSafety,
    "Herbalist.GatheringTool.BoneKnifeBonusOverridesIronAverseSafety",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistGatheringTool_BoneKnifeBonusOverridesIronAverseSafety::RunTest(const FString& Parameters)
{
    // Трава несёт оба флага сразу (кость и так не железо -- снимает табу
    // bIronAverse и даёт бонус bDelicate одновременно) -- костяной нож
    // должен выйти лучше, чем просто "безопасный" (голыми руками) вариант,
    // не просто сравняться с ним.
    const FRealState BareHands = HarvestWithTool(EGatheringTool::BareHands, /*bIronAverse=*/true, /*bDelicate=*/true, 8006);
    const FRealState Bone      = HarvestWithTool(EGatheringTool::BoneKnife, /*bIronAverse=*/true, /*bDelicate=*/true, 8006);

    TestTrue(TEXT("Bone knife beats bare hands on a herb that is both iron-averse AND delicate"), Bone.Magnitude > BareHands.Magnitude);
    return true;
}

#endif
