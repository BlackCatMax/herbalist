// Source/ProjectHerbalistTests/Private/Tests/MoonPhaseTest.cpp
//
// Лунный цикл (15_Cycles_And_Shrines.md §15.3), v1: фаза считается из
// GameClockSeconds (тот же принцип, что GetTimeOfDay01()/IsNight()), эффект
// на сбор идёт через GenerateHarvestResult (PipelineV2.cpp). Новолуние и
// Убывающая пока не имеют эффекта (влияют на применённое зелье, не на
// сбор — отдельный, ещё не сделанный кусок), поэтому тестируются только
// как "не Растущая и не Полнолуние, значит без буста".
// DispatchBeginPlay-паттерн для фазы — тот же, что BistabilityTest.cpp.
// Тест на буст урожая — тот же паттерн полного пайплайна, что ResilienceTest.cpp.

#include "Core/World/GridWorldManager.h"
#include "Core/Simulation/Public/SnapshotTypes.h"
#include "Core/Simulation/Public/DeltaTypes.h"
#include "Core/Simulation/Public/CommandTypes.h"
#include "PipelineV2.h"
#include "Core/Types/BiomeTypes.h"
#include "Math/RandomStream.h"
#include "Misc/AutomationTest.h"
#include "Editor.h"
#include "Engine/World.h"

#if WITH_AUTOMATION_TESTS

namespace
{
    // Клетка, нейтральная относительно базового состояния ингредиента —
    // изолирует эффект луны от эффекта Resilience/биома (уже покрытого
    // ResilienceTest.cpp).
    FGridCell MakeNeutralCell()
    {
        FGridCell Cell;
        Cell.X = 4; Cell.Y = 4;
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

    FRealState HarvestWithMoonPhase(EMoonPhase MoonPhase, int32 Seed)
    {
        const FGridCell Cell = MakeNeutralCell();
        FWorldSnapshot WorldSnap;
        WorldSnap.GridState.Add(FIntPoint(Cell.X, Cell.Y), Cell);
        WorldSnap.WorldSeed = Seed;

        FInventorySnapshot InvSnap;
        FBiomeSnapshot BiomeSnap;

        FCommandBatch CmdBatch;
        FCommandEntry CmdEntry;
        CmdEntry.Primitive = ECommandPrimitive::Harvest;
        CmdEntry.Harvest.TargetCell = FIntPoint(Cell.X, Cell.Y);
        CmdEntry.Harvest.IngredientID = TEXT("MoonProbe");
        CmdEntry.Harvest.Amount = 1;
        CmdEntry.Harvest.BaseState = Cell.State;   // Base == Cell -> Resilience/биом не вносят разброса
        CmdEntry.Harvest.Resilience = 1.0f;
        CmdEntry.Harvest.MoonPhase = MoonPhase;
        CmdBatch.AddCommand(CmdEntry);

        FRandomStream Rng(Seed);
        FStateDelta Delta = Simulation::ExecutePipeline(WorldSnap, InvSnap, BiomeSnap, CmdBatch, Rng);
        check(Delta.InventoryOps.Num() == 1);
        return Delta.InventoryOps[0].Ingredient.State;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistMoonPhase_WaxingBoostsBodyNatureMagnitude,
    "Herbalist.MoonPhase.WaxingBoostsBodyNatureMagnitude",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistMoonPhase_WaxingBoostsBodyNatureMagnitude::RunTest(const FString& Parameters)
{
    const FRealState NewMoonResult    = HarvestWithMoonPhase(EMoonPhase::NewMoon,    5001);
    const FRealState WaxingMoonResult = HarvestWithMoonPhase(EMoonPhase::WaxingMoon, 5001);

    TestTrue(TEXT("Direction.Body relatively higher during Waxing Moon"), WaxingMoonResult.Direction.Body > NewMoonResult.Direction.Body);
    TestTrue(TEXT("Direction.Nature relatively higher during Waxing Moon"), WaxingMoonResult.Direction.Nature > NewMoonResult.Direction.Nature);
    TestTrue(TEXT("Magnitude higher during Waxing Moon"), WaxingMoonResult.Magnitude > NewMoonResult.Magnitude);
    // Полнолунный буст (Spirit/Potency/Resonance) не должен просочиться в Растущую.
    TestTrue(TEXT("Meta.Potency untouched during Waxing Moon"), FMath::IsNearlyEqual(WaxingMoonResult.Meta.Potency, NewMoonResult.Meta.Potency, 0.001f));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistMoonPhase_FullMoonBoostsSpiritPotencyResonance,
    "Herbalist.MoonPhase.FullMoonBoostsSpiritPotencyResonance",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistMoonPhase_FullMoonBoostsSpiritPotencyResonance::RunTest(const FString& Parameters)
{
    const FRealState NewMoonResult  = HarvestWithMoonPhase(EMoonPhase::NewMoon,  6002);
    const FRealState FullMoonResult = HarvestWithMoonPhase(EMoonPhase::FullMoon, 6002);

    TestTrue(TEXT("Direction.Spirit relatively higher during Full Moon"), FullMoonResult.Direction.Spirit > NewMoonResult.Direction.Spirit);
    TestTrue(TEXT("Meta.Potency higher during Full Moon"), FullMoonResult.Meta.Potency > NewMoonResult.Meta.Potency);
    TestTrue(TEXT("Meta.Resonance higher during Full Moon"), FullMoonResult.Meta.Resonance > NewMoonResult.Meta.Resonance);
    // Растущий буст (Body/Nature/Magnitude) не должен просочиться в Полнолуние.
    TestTrue(TEXT("Magnitude untouched during Full Moon"), FMath::IsNearlyEqual(FullMoonResult.Magnitude, NewMoonResult.Magnitude, 0.001f));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistMoonPhase_NewMoonAndWaningMoonLeaveHarvestUnboosted,
    "Herbalist.MoonPhase.NewMoonAndWaningMoonLeaveHarvestUnboosted",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistMoonPhase_NewMoonAndWaningMoonLeaveHarvestUnboosted::RunTest(const FString& Parameters)
{
    // Новолуние/Убывающая усиливают применённое зелье (Apply), не сбор
    // (Harvest) — v1 сознательно не трогает эти два случая здесь, см.
    // комментарий у GenerateHarvestResult. Результат должен быть идентичен.
    const FRealState NewMoonResult    = HarvestWithMoonPhase(EMoonPhase::NewMoon,    7003);
    const FRealState WaningMoonResult = HarvestWithMoonPhase(EMoonPhase::WaningMoon, 7003);

    TestTrue(TEXT("Waning Moon harvest matches New Moon harvest (no Apply-side effect implemented yet)"),
        FMath::IsNearlyEqual(NewMoonResult.Magnitude, WaningMoonResult.Magnitude, 0.001f) &&
        FMath::IsNearlyEqual(NewMoonResult.Direction.Spirit, WaningMoonResult.Direction.Spirit, 0.001f));
    return true;
}

#if WITH_EDITOR

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistMoonPhase_CyclesThroughAllFourPhasesOverOneMonth,
    "Herbalist.MoonPhase.CyclesThroughAllFourPhasesOverOneMonth",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistMoonPhase_CyclesThroughAllFourPhasesOverOneMonth::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = World->SpawnActor<AGridWorldManager>();
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;
    Manager->DispatchBeginPlay();

    // Дефолт: GameDayMinutes=32мин, StressRecoveryGameDays=7 -> фаза = 7 суток,
    // весь месяц = 28 суток = 28*32*60 = 53760 секунд игровых часов.
    const float DayLengthSeconds = 32.0f * 60.0f;
    const float PhaseDurationSeconds = 7.0f * DayLengthSeconds;

    Manager->SetGameClockSeconds(0.0f);
    TestEqual(TEXT("Day 0 is New Moon"), Manager->GetMoonPhase(), EMoonPhase::NewMoon);

    Manager->SetGameClockSeconds(PhaseDurationSeconds + 1.0f);
    TestEqual(TEXT("Day 7 is Waxing Moon"), Manager->GetMoonPhase(), EMoonPhase::WaxingMoon);

    Manager->SetGameClockSeconds(PhaseDurationSeconds * 2.0f + 1.0f);
    TestEqual(TEXT("Day 14 is Full Moon"), Manager->GetMoonPhase(), EMoonPhase::FullMoon);

    Manager->SetGameClockSeconds(PhaseDurationSeconds * 3.0f + 1.0f);
    TestEqual(TEXT("Day 21 is Waning Moon"), Manager->GetMoonPhase(), EMoonPhase::WaningMoon);

    Manager->SetGameClockSeconds(PhaseDurationSeconds * 4.0f + 1.0f);
    TestEqual(TEXT("Day 28 wraps back to New Moon"), Manager->GetMoonPhase(), EMoonPhase::NewMoon);

    Manager->Destroy();
    return true;
}

#endif // WITH_EDITOR

#endif // WITH_AUTOMATION_TESTS
