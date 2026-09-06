// Source/ProjectHerbalistTests/Private/Tests/WorldDistanceMetricTest.cpp
//
// Метрика мира с историей (15_Cycles_And_Shrines.md §15.5.1, реализовано
// 2026-09-06, прямой запрос пользователя "реализуем"). HerbalistCore::Math::
// DistanceWithHistory — чистая функция, тестируется без мира/пайплайна, тем
// же принципом, что уже ApplyOverdosePenalty (OverdoseTest.cpp): проверить
// ровно новую формулу, не гонять весь ComputeApplyResult.

#include "Core/Types/HerbalistCoreMath.h"
#include "Misc/AutomationTest.h"

#if WITH_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistWorldDistanceMetric_FullCoherenceMatchesRawDistance,
    "Herbalist.WorldDistanceMetric.FullCoherenceMatchesRawDistance",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistWorldDistanceMetric_FullCoherenceMatchesRawDistance::RunTest(const FString& Parameters)
{
    // AverageCoherence=1 -- "дошло до этих чисел через сплошь согласованные
    // варки" -- множитель (2-1)=1, никакого штрафа, ровно голый Distance.
    FRealState State;
    State.Magnitude = 0.5f;
    State.Meta.Purity = 0.3f;

    const float Raw = HerbalistCore::Math::Distance(State, FAlatyr::S0);
    const float WithHistory = HerbalistCore::Math::DistanceWithHistory(State, 1.0f);

    TestTrue(TEXT("AverageCoherence=1 leaves Distance unpenalized"), FMath::IsNearlyEqual(WithHistory, Raw, 0.0001f));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistWorldDistanceMetric_ZeroCoherenceDoublesDistance,
    "Herbalist.WorldDistanceMetric.ZeroCoherenceDoublesDistance",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistWorldDistanceMetric_ZeroCoherenceDoublesDistance::RunTest(const FString& Parameters)
{
    // AverageCoherence=0 -- "те же числа, но через хаос" -- множитель
    // (2-0)=2, расстояние ровно удваивается (15_Cycles_And_Shrines.md §15.5.1).
    FRealState State;
    State.Magnitude = 0.4f;
    State.Meta.Corruption = 0.6f;

    const float Raw = HerbalistCore::Math::Distance(State, FAlatyr::S0);
    const float WithHistory = HerbalistCore::Math::DistanceWithHistory(State, 0.0f);

    TestTrue(TEXT("AverageCoherence=0 exactly doubles the raw distance"), FMath::IsNearlyEqual(WithHistory, Raw * 2.0f, 0.0001f));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistWorldDistanceMetric_MultiplierClampedToOneTwoRange,
    "Herbalist.WorldDistanceMetric.MultiplierClampedToOneTwoRange",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistWorldDistanceMetric_MultiplierClampedToOneTwoRange::RunTest(const FString& Parameters)
{
    // Спецификация требует множитель строго в [1,2] -- даже если
    // AverageCoherence когда-нибудь выйдет за [0,1] (не гарантировано местом
    // вычисления EMA), формула не должна улучшать расстояние ниже голого
    // Distance или ухудшать его больше чем вдвое.
    FRealState State;
    State.Magnitude = 0.5f;
    const float Raw = HerbalistCore::Math::Distance(State, FAlatyr::S0);

    const float AboveOne = HerbalistCore::Math::DistanceWithHistory(State, 1.5f);   // (2-1.5)=0.5 -> кламп к 1.0
    TestTrue(TEXT("AverageCoherence above 1 does not reduce distance below raw"), FMath::IsNearlyEqual(AboveOne, Raw, 0.0001f));

    const float BelowZero = HerbalistCore::Math::DistanceWithHistory(State, -1.0f);  // (2-(-1))=3 -> кламп к 2.0
    TestTrue(TEXT("AverageCoherence below 0 does not exceed the double-distance cap"), FMath::IsNearlyEqual(BelowZero, Raw * 2.0f, 0.0001f));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistWorldDistanceMetric_LowerCoherenceMeansFartherFromAlatyr,
    "Herbalist.WorldDistanceMetric.LowerCoherenceMeansFartherFromAlatyr",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistWorldDistanceMetric_LowerCoherenceMeansFartherFromAlatyr::RunTest(const FString& Parameters)
{
    // Два места с ОДИНАКОВЫМ State -- путь-зависимость: то, что пришло через
    // менее согласованную историю, должно читаться как более далёкое от S0.
    FRealState State;
    State.Magnitude = 0.6f;
    State.Meta.Distortion = 0.4f;

    const float Coherent = HerbalistCore::Math::DistanceWithHistory(State, 0.8f);
    const float Chaotic = HerbalistCore::Math::DistanceWithHistory(State, 0.2f);

    TestTrue(TEXT("Same raw State, lower AverageCoherence reads as farther from S0"), Chaotic > Coherent + KINDA_SMALL_NUMBER);
    return true;
}

#endif
