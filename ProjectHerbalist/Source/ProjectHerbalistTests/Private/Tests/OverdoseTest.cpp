// Source/ProjectHerbalistTests/Private/Tests/OverdoseTest.cpp
//
// Передозировка зелий (обсуждение в сессии 2026-08-24) — HerbalistCore::Math::
// ApplyOverdosePenalty — чистая функция, тестируется без мира/пайплайна, тем
// же принципом, что GetInfluenceAt (Herbalist.Shrine.*): не гонять весь
// ComputeApplyResult (сложная, не относящаяся к этой правке цепочка), а
// проверить ровно то, что здесь новое.

#include "Core/Types/HerbalistCoreMath.h"
#include "Misc/AutomationTest.h"

#if WITH_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistOverdose_BelowThresholdLeavesStateUntouched,
    "Herbalist.Overdose.BelowThresholdLeavesStateUntouched",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistOverdose_BelowThresholdLeavesStateUntouched::RunTest(const FString& Parameters)
{
    // Ландыш как сердечное зелье умеренной силы — просто лечит, без обратной стороны.
    FRealState State;
    State.Meta.Potency = 0.6f;
    State.Meta.Stability = 0.8f;
    State.Meta.Purity = 0.9f;
    State.Meta.Distortion = 0.1f;
    State.Meta.Corruption = 0.05f;

    HerbalistCore::Math::ApplyOverdosePenalty(State, 0.75f, 0.5f);

    TestEqual(TEXT("Stability untouched below threshold"), State.Meta.Stability, 0.8f);
    TestEqual(TEXT("Purity untouched below threshold"), State.Meta.Purity, 0.9f);
    TestEqual(TEXT("Distortion untouched below threshold"), State.Meta.Distortion, 0.1f);
    TestEqual(TEXT("Corruption untouched below threshold"), State.Meta.Corruption, 0.05f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistOverdose_AboveThresholdHarmsInsteadOfHealing,
    "Herbalist.Overdose.AboveThresholdHarmsInsteadOfHealing",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistOverdose_AboveThresholdHarmsInsteadOfHealing::RunTest(const FString& Parameters)
{
    // Тот же Ландыш, но передозировка — "яд лютый": Potency на максимуме.
    FRealState State;
    State.Meta.Potency = 1.0f;
    State.Meta.Stability = 0.8f;
    State.Meta.Purity = 0.9f;
    State.Meta.Distortion = 0.1f;
    State.Meta.Corruption = 0.05f;

    HerbalistCore::Math::ApplyOverdosePenalty(State, 0.75f, 0.5f);

    TestTrue(TEXT("Stability drops"), State.Meta.Stability < 0.8f);
    TestTrue(TEXT("Purity drops"), State.Meta.Purity < 0.9f);
    TestTrue(TEXT("Distortion rises"), State.Meta.Distortion > 0.1f);
    TestTrue(TEXT("Corruption rises"), State.Meta.Corruption > 0.05f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistOverdose_StrongerOverdoseHarmsMore,
    "Herbalist.Overdose.StrongerOverdoseHarmsMore",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistOverdose_StrongerOverdoseHarmsMore::RunTest(const FString& Parameters)
{
    // Два зелья одинаковой чистоты, разной силы — то самое "сильнее лечит,
    // вернее убивает": более мощное должно навредить сильнее при передозировке.
    FRealState Mild;
    Mild.Meta.Potency = 0.8f;
    Mild.Meta.Stability = 0.8f;

    FRealState Strong;
    Strong.Meta.Potency = 1.0f;
    Strong.Meta.Stability = 0.8f;

    HerbalistCore::Math::ApplyOverdosePenalty(Mild, 0.75f, 0.5f);
    HerbalistCore::Math::ApplyOverdosePenalty(Strong, 0.75f, 0.5f);

    TestTrue(TEXT("Higher Potency overdose harms Stability more"), Strong.Meta.Stability < Mild.Meta.Stability);
    return true;
}

#endif // WITH_AUTOMATION_TESTS
