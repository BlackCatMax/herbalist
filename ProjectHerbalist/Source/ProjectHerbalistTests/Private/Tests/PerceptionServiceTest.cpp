// Source/ProjectHerbalistTests/Private/Tests/PerceptionServiceTest.cpp
//
// Полный аудит проекта (2026-08-31, продолжение): PerceptionService был
// назван в ROADMAP.md как "реально используется, нулевое покрытие" —
// шум PerceiveRealState лежит в основе всей эпистемики игры (S_Perceived
// = Distort(S_real, Morok), 01_Introduction.md), но ни разу не
// протестирован напрямую. Как и TraceReplay рядом, оказался чистой
// функцией без зависимости от мира — FPerceptionService::PerceiveRealState
// берёт FRealState+FRandomStream+Clarity, возвращает FRealState, ничего
// больше не трогает.

#include "PerceptionService.h"
#include "Core/Simulation/Public/SnapshotTypes.h"
#include "Core/Types/HerbalistCoreTypes.h"
#include "Math/RandomStream.h"
#include "Misc/AutomationTest.h"

#if WITH_AUTOMATION_TESTS

namespace
{
    // Значения намеренно в середине [0,1] -- не у 0/1 -- чтобы клампинг в
    // PerceiveRealState никогда не вступал в игру и не ломал линейность
    // шума по Clarity, которую проверяет тест ниже.
    FRealState PerceptionTestMidRangeState(float Distortion)
    {
        FRealState State;
        State.Direction.Body = 0.3f; State.Direction.Mind = 0.2f;
        State.Direction.Spirit = 0.3f; State.Direction.Nature = 0.2f;
        State.Magnitude = 0.5f;
        State.Meta.Distortion = Distortion;
        State.Meta.Purity = 0.5f;
        State.Meta.Stability = 0.5f;
        State.Meta.Potency = 0.5f;
        State.Meta.Resonance = 0.5f;
        return State;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistPerception_ZeroDistortionMeansNoNoise,
    "Herbalist.Perception.ZeroDistortionMeansNoNoise",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistPerception_ZeroDistortionMeansNoNoise::RunTest(const FString& Parameters)
{
    // NoiseScale = Distortion * (1-Clarity) -- при Distortion=0 множитель
    // ровно 0 независимо от Clarity, значит Rng вообще не должен влиять на
    // результат (не "почти не искажает", а буквально не искажает).
    const FRealState Real = PerceptionTestMidRangeState(0.0f);
    FRandomStream Rng(123);
    const FRealState Perceived = Simulation::FPerceptionService::PerceiveRealState(Real, Rng, /*Clarity=*/0.0f);

    TestEqual(TEXT("Magnitude exactly unchanged at zero Distortion"), Perceived.Magnitude, Real.Magnitude);
    TestEqual(TEXT("Purity exactly unchanged at zero Distortion"), Perceived.Meta.Purity, Real.Meta.Purity);
    TestEqual(TEXT("Stability exactly unchanged at zero Distortion"), Perceived.Meta.Stability, Real.Meta.Stability);
    TestEqual(TEXT("Distortion itself exactly unchanged (still zero)"), Perceived.Meta.Distortion, Real.Meta.Distortion);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistPerception_FullClarityCancelsNoiseEvenAtMaxDistortion,
    "Herbalist.Perception.FullClarityCancelsNoiseEvenAtMaxDistortion",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistPerception_FullClarityCancelsNoiseEvenAtMaxDistortion::RunTest(const FString& Parameters)
{
    // Прямая проверка заявления 06_Progression/17_Hero_And_Community:
    // GlobalPerceptionClarity, накопленная через Заряну, гасит шум даже у
    // максимально искажённого объекта -- Clarity=1 значит NoiseScale=0
    // независимо от Distortion=1.
    const FRealState Real = PerceptionTestMidRangeState(1.0f);
    FRandomStream Rng(456);
    const FRealState Perceived = Simulation::FPerceptionService::PerceiveRealState(Real, Rng, /*Clarity=*/1.0f);

    TestEqual(TEXT("Magnitude exactly unchanged despite Distortion=1, because Clarity=1"), Perceived.Magnitude, Real.Magnitude);
    TestEqual(TEXT("Purity exactly unchanged"), Perceived.Meta.Purity, Real.Meta.Purity);
    TestEqual(TEXT("Stability exactly unchanged"), Perceived.Meta.Stability, Real.Meta.Stability);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistPerception_HighDistortionWithZeroClarityActuallyAddsNoise,
    "Herbalist.Perception.HighDistortionWithZeroClarityActuallyAddsNoise",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistPerception_HighDistortionWithZeroClarityActuallyAddsNoise::RunTest(const FString& Parameters)
{
    // Симметрия к двум тестам выше -- при максимальном множителе шум
    // реально появляется, не только теоретически возможен. Фиксированный
    // сид держит тест детерминированным, не полагается на удачу.
    const FRealState Real = PerceptionTestMidRangeState(1.0f);
    FRandomStream Rng(789);
    const FRealState Perceived = Simulation::FPerceptionService::PerceiveRealState(Real, Rng, /*Clarity=*/0.0f);

    TestFalse(TEXT("At max Distortion and zero Clarity, perceived Magnitude visibly differs from real"),
        FMath::IsNearlyEqual(Perceived.Magnitude, Real.Magnitude, 0.0001f));

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistPerception_NoiseScalesLinearlyWithClarity,
    "Herbalist.Perception.NoiseScalesLinearlyWithClarity",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistPerception_NoiseScalesLinearlyWithClarity::RunTest(const FString& Parameters)
{
    // FRandRange вызывается в том же порядке независимо от Clarity --
    // Clarity домножает уже вытянутый шум, не меняет саму последовательность
    // Rng. Значит на одном сиде отклонение при Clarity=0.5 обязано быть
    // РОВНО половиной отклонения при Clarity=0 -- не "поменьше", а точная
    // линейная зависимость, раз середина диапазона исключает клампинг.
    const FRealState Real = PerceptionTestMidRangeState(1.0f);
    const int32 Seed = 321;

    FRandomStream RngNoClarity(Seed);
    const FRealState NoClarity = Simulation::FPerceptionService::PerceiveRealState(Real, RngNoClarity, 0.0f);

    FRandomStream RngHalfClarity(Seed);
    const FRealState HalfClarity = Simulation::FPerceptionService::PerceiveRealState(Real, RngHalfClarity, 0.5f);

    const float FullNoise = NoClarity.Magnitude - Real.Magnitude;
    const float HalfNoise = HalfClarity.Magnitude - Real.Magnitude;

    TestTrue(TEXT("There is measurable noise at Clarity=0 to compare against"), FMath::Abs(FullNoise) > 0.001f);
    TestTrue(TEXT("Noise at Clarity=0.5 is exactly half the noise at Clarity=0, same seed"),
        FMath::IsNearlyEqual(HalfNoise, FullNoise * 0.5f, 0.001f));

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistPerception_ComputePerceivedWorldPreservesCoordinatesAndSeed,
    "Herbalist.Perception.ComputePerceivedWorldPreservesCoordinatesAndSeed",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistPerception_ComputePerceivedWorldPreservesCoordinatesAndSeed::RunTest(const FString& Parameters)
{
    FWorldSnapshot WorldSnap;
    WorldSnap.WorldSeed = 42;
    FGridCell CellA; CellA.X = 1; CellA.Y = 1; CellA.State = PerceptionTestMidRangeState(0.3f);
    FGridCell CellB; CellB.X = 2; CellB.Y = 5; CellB.State = PerceptionTestMidRangeState(0.6f);
    WorldSnap.GridState.Add(FIntPoint(1, 1), CellA);
    WorldSnap.GridState.Add(FIntPoint(2, 5), CellB);

    FRandomStream Rng(1);
    const FPerceivedWorld Perceived = Simulation::FPerceptionService::ComputePerceivedWorld(WorldSnap, Rng, 0.0f);

    TestEqual(TEXT("WorldSeed carried through unchanged"), Perceived.WorldSeed, 42);
    TestEqual(TEXT("Both cells present in the perceived world"), Perceived.Cells.Num(), 2);
    const FPerceivedCell* PA = Perceived.Cells.Find(FIntPoint(1, 1));
    const FPerceivedCell* PB = Perceived.Cells.Find(FIntPoint(2, 5));
    if (TestNotNull(TEXT("Cell (1,1) present"), PA))
    {
        TestTrue(TEXT("Cell (1,1) marked visible"), PA->bIsVisible);
    }
    if (TestNotNull(TEXT("Cell (2,5) present"), PB))
    {
        TestTrue(TEXT("Cell (2,5) marked visible"), PB->bIsVisible);
    }

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistPerception_ComputePerceivedInventoryPreservesStructure,
    "Herbalist.Perception.ComputePerceivedInventoryPreservesStructure",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistPerception_ComputePerceivedInventoryPreservesStructure::RunTest(const FString& Parameters)
{
    FInventorySnapshot InvSnap;
    FInventoryItem ItemA; ItemA.IngredientID = FName(TEXT("Probe1")); ItemA.State = PerceptionTestMidRangeState(0.2f);
    FInventoryItem ItemB; ItemB.IngredientID = FName(TEXT("Probe2")); ItemB.State = PerceptionTestMidRangeState(0.4f);
    InvSnap.ContainerContents.Add(0, { ItemA, ItemB });

    const FPerceivedInventory Perceived = Simulation::FPerceptionService::ComputePerceivedInventory(InvSnap, 0.0f);

    const TArray<FInventoryItem>* Container = Perceived.ContainerContents.Find(0);
    if (!TestNotNull(TEXT("Container 0 present in perceived inventory"), Container)) return false;
    TestEqual(TEXT("Both items present, same order"), Container->Num(), 2);
    if (Container->Num() == 2)
    {
        TestEqual(TEXT("First item's IngredientID preserved (identity is a fact, not perception)"),
            (*Container)[0].IngredientID, FName(TEXT("Probe1")));
        TestEqual(TEXT("Second item's IngredientID preserved"), (*Container)[1].IngredientID, FName(TEXT("Probe2")));
    }

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistPerception_InventoryNoiseIsStableForSameIdentityAndState,
    "Herbalist.Perception.InventoryNoiseIsStableForSameIdentityAndState",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistPerception_InventoryNoiseIsStableForSameIdentityAndState::RunTest(const FString& Parameters)
{
    // Аудит 2026-09-05 + решение пользователя: шум предмета в инвентаре —
    // функция identity+State, не тикового сида. Держать тултип открытым
    // (то есть звать ComputePerceivedInventory повторно на НЕИЗМЕНИВШЕМСЯ
    // предмете) больше не должно давать разные значения, которые можно
    // усреднить до честного S_real.
    FInventoryItem Item;
    Item.IngredientID = FName(TEXT("Probe1"));
    Item.CreationTime = 123.5f;
    Item.State = PerceptionTestMidRangeState(0.7f);   // высокая Distortion -- шум заметен, есть что перепутать

    FInventorySnapshot InvSnap;
    InvSnap.ContainerContents.Add(0, { Item });

    const FPerceivedInventory First = Simulation::FPerceptionService::ComputePerceivedInventory(InvSnap, 0.0f);
    const FPerceivedInventory Second = Simulation::FPerceptionService::ComputePerceivedInventory(InvSnap, 0.0f);

    const FInventoryItem& A = (*First.ContainerContents.Find(0))[0];
    const FInventoryItem& B = (*Second.ContainerContents.Find(0))[0];
    TestEqual(TEXT("Same item, same call twice -- identical noisy Magnitude (nothing to average)"), A.State.Magnitude, B.State.Magnitude);
    TestEqual(TEXT("Same item, same call twice -- identical noisy Distortion"), A.State.Meta.Distortion, B.State.Meta.Distortion);

    // Меняем реальное состояние (как порча/сушка/отстой сдвинули бы его) --
    // шум обязан сдвинуться сам, а не остаться приклеенным к старому значению.
    FInventoryItem ChangedItem = Item;
    ChangedItem.State.Meta.Distortion = 0.71f;
    FInventorySnapshot ChangedSnap;
    ChangedSnap.ContainerContents.Add(0, { ChangedItem });
    const FPerceivedInventory AfterChange = Simulation::FPerceptionService::ComputePerceivedInventory(ChangedSnap, 0.0f);
    const FInventoryItem& C = (*AfterChange.ContainerContents.Find(0))[0];
    TestNotEqual(TEXT("Real state changed -- perceived Magnitude noise draw is now a different one"), A.State.Magnitude, C.State.Magnitude);

    return true;
}

#endif // WITH_AUTOMATION_TESTS
