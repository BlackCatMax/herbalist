// Source/ProjectHerbalistTests/Private/Tests/PipelineSameCellTest.cpp
//
// Аудит кода 2026-09-26, Б4 (решение пользователя -- вариант (а)): команда
// шага видит изменения своей клетки, уже набранные в этом шаге. Раньше каждая
// считалась от снимка мира на начало шага, и вторая команда на ту же клетку
// затирала первую в FStateDelta::WorldChanges: два сбора истощали клетку
// как один, у двух зелий на капище в подношение шло только второе.

#include "Core/Simulation/Public/SnapshotTypes.h"
#include "Core/Simulation/Public/DeltaTypes.h"
#include "Core/Simulation/Public/CommandTypes.h"
#include "PipelineV2.h"
#include "Core/Config/HerbalistSettings.h"
#include "Core/Types/HerbalistCoreTypes.h"
#include "Math/RandomStream.h"
#include "Misc/AutomationTest.h"

#if WITH_AUTOMATION_TESTS

namespace
{
    FGridCell MakeSameCellTarget()
    {
        FGridCell Cell;
        Cell.X = 4;
        Cell.Y = 4;
        Cell.Biome = EBiomeType::MixedForest;
        Cell.bIsWater = false;
        Cell.HarvestStress = 0.1f;
        Cell.State.Magnitude = 0.5f;
        Cell.State.Direction.Body = Cell.State.Direction.Mind = Cell.State.Direction.Spirit = Cell.State.Direction.Nature = 0.25f;
        Cell.State.Meta.Distortion = 0.3f;
        Cell.State.Meta.Purity = 0.5f;
        Cell.State.Meta.Stability = 0.5f;
        return Cell;
    }

    FCommandEntry MakeSameCellHarvest(const FIntPoint& Cell)
    {
        FCommandEntry Entry;
        Entry.Primitive = ECommandPrimitive::Harvest;
        Entry.Harvest.TargetCell = Cell;
        Entry.Harvest.IngredientID = FName(TEXT("bol_01"));
        Entry.Harvest.BaseState.Magnitude = 0.5f;
        Entry.Harvest.BaseState.Direction.Body = 1.0f;
        return Entry;
    }

    FCommandEntry MakeSameCellApply(const FIntPoint& Cell, float HerbPurity, float HerbCorruption)
    {
        FInventoryItem Herb;
        Herb.IngredientID = FName(TEXT("Herb"));
        Herb.Count = 1;
        Herb.State.Magnitude = 0.5f;
        Herb.State.Direction.Body = 1.0f;
        Herb.State.Meta.Distortion = 0.2f;
        Herb.State.Meta.Stability = 0.6f;
        Herb.State.Meta.Purity = HerbPurity;
        Herb.State.Meta.Corruption = HerbCorruption;

        FInventoryItem Water;
        Water.IngredientID = FName(TEXT("Water"));
        Water.Count = 1;
        Water.bIsWater = true;
        Water.State.Magnitude = 0.5f;
        Water.State.Direction.Body = Water.State.Direction.Mind = Water.State.Direction.Spirit = Water.State.Direction.Nature = 0.25f;
        Water.State.Meta.Purity = 0.5f;

        FCommandEntry Entry;
        Entry.Primitive = ECommandPrimitive::Apply;
        Entry.Apply.TargetCell = Cell;
        Entry.Apply.Ingredients = { Herb, Water };
        Entry.Apply.bIsCrafting = false;
        return Entry;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistPipeline_TwoHarvestsOnOneCellBothCount,
    "Herbalist.Pipeline.SameCell.TwoHarvestsOnOneCellBothCount",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistPipeline_TwoHarvestsOnOneCellBothCount::RunTest(const FString& Parameters)
{
    const FGridCell Base = MakeSameCellTarget();
    const FIntPoint Coord(Base.X, Base.Y);
    FWorldSnapshot WorldSnap;
    WorldSnap.GridState.Add(Coord, Base);
    FInventorySnapshot InvSnap;
    FBiomeSnapshot BiomeSnap;

    FCommandBatch Batch;
    Batch.AddCommand(MakeSameCellHarvest(Coord));
    Batch.AddCommand(MakeSameCellHarvest(Coord));
    FRandomStream Rng(7);
    const FStateDelta Delta = Simulation::ExecutePipeline(WorldSnap, InvSnap, BiomeSnap, Batch, Rng);

    const UHerbalistSettings* Settings = GetHerbalistSettings();
    const float StressStep = Settings ? Settings->HarvestStressIncrement : 0.1f;

    TestEqual(TEXT("Обе добычи в сумку"), Delta.InventoryOps.Num(), 2);
    const FGridCell* Changed = Delta.WorldChanges.Find(Coord);
    if (TestNotNull(TEXT("Клетка изменена"), Changed))
    {
        TestEqual(TEXT("Стресс -- от обоих сборов"), Changed->HarvestStress,
            FMath::Clamp(Base.HarvestStress + 2.0f * StressStep, 0.0f, 1.0f), KINDA_SMALL_NUMBER);
        TestEqual(TEXT("Искажение -- от обоих сборов"), Changed->State.Meta.Distortion,
            Base.State.Meta.Distortion + 2.0f * 0.002f, KINDA_SMALL_NUMBER);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistPipeline_TwoPotionsOnOneCellBothCount,
    "Herbalist.Pipeline.SameCell.TwoPotionsOnOneCellBothCount",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistPipeline_TwoPotionsOnOneCellBothCount::RunTest(const FString& Parameters)
{
    const FGridCell Base = MakeSameCellTarget();
    const FIntPoint Coord(Base.X, Base.Y);
    FWorldSnapshot WorldSnap;
    WorldSnap.GridState.Add(Coord, Base);
    FInventorySnapshot InvSnap;
    FBiomeSnapshot BiomeSnap;

    // Чистое и скверное зелье на одну клетку в одном шаге.
    FCommandBatch Batch;
    Batch.AddCommand(MakeSameCellApply(Coord, 0.9f, 0.0f));
    Batch.AddCommand(MakeSameCellApply(Coord, 0.1f, 0.8f));
    FRandomStream Rng(11);
    const FStateDelta Delta = Simulation::ExecutePipeline(WorldSnap, InvSnap, BiomeSnap, Batch, Rng);

    TestEqual(TEXT("Две варки на клетку -- две записи для подношений"), Delta.CellApplications.Num(), 2);
    const FGridCell* Changed = Delta.WorldChanges.Find(Coord);
    if (TestNotNull(TEXT("Клетка изменена"), Changed) && Delta.CellApplications.Num() == 2)
    {
        TestEqual(TEXT("Стресс -- от обеих варок"), Changed->HarvestStress,
            FMath::Clamp(Base.HarvestStress + 0.4f, 0.0f, 1.0f), KINDA_SMALL_NUMBER);
        TestTrue(TEXT("Каждое зелье со своим качеством: первое чище второго"),
            Delta.CellApplications[0].State.Meta.Purity - Delta.CellApplications[0].State.Meta.Corruption
            > Delta.CellApplications[1].State.Meta.Purity - Delta.CellApplications[1].State.Meta.Corruption);
        TestEqual(TEXT("Состояние клетки -- от последнего зелья"),
            Changed->State.Meta.Purity, Delta.CellApplications[1].State.Meta.Purity, KINDA_SMALL_NUMBER);
    }
    return true;
}

#endif
