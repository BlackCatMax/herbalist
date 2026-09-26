// Source/ProjectHerbalistTests/Private/Tests/TextCapitalizeTest.cpp
//
// Аудит кода 2026-09-26, Б5: строка ощущения при осмотре начиналась со
// строчной -- FChar::ToUpper меняет только латиницу. Общий помощник
// HerbalistCore::Text::CapitalizeFirst (Core/Types/HerbalistText.h) и сама
// строка ощущения.

#include "Core/Types/HerbalistText.h"
#include "Core/Types/HerbalistSensation.h"
#include "Misc/AutomationTest.h"

#if WITH_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistText_CapitalizeFirstHandlesCyrillic,
    "Herbalist.Text.CapitalizeFirstHandlesCyrillic",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistText_CapitalizeFirstHandlesCyrillic::RunTest(const FString& Parameters)
{
    using HerbalistCore::Text::CapitalizeFirst;
    TestEqual(TEXT("т -> Т"), CapitalizeFirst(TEXT("тёплый")), FString(TEXT("Тёплый")));
    TestEqual(TEXT("а -> А"), CapitalizeFirst(TEXT("аир")), FString(TEXT("Аир")));
    TestEqual(TEXT("я -> Я"), CapitalizeFirst(TEXT("ясный")), FString(TEXT("Ясный")));
    TestEqual(TEXT("ё -> Ё"), CapitalizeFirst(TEXT("ёмкий")), FString(TEXT("Ёмкий")));
    TestEqual(TEXT("Латиница"), CapitalizeFirst(TEXT("water")), FString(TEXT("Water")));
    TestEqual(TEXT("Уже заглавная"), CapitalizeFirst(TEXT("Зола")), FString(TEXT("Зола")));
    TestEqual(TEXT("Пустая"), CapitalizeFirst(FString()), FString());
    TestEqual(TEXT("Не буква"), CapitalizeFirst(TEXT("«тихо»")), FString(TEXT("«тихо»")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistText_SensationLineStartsCapitalized,
    "Herbalist.Text.SensationLineStartsCapitalized",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistText_SensationLineStartsCapitalized::RunTest(const FString& Parameters)
{
    // Все четыре ведущие оси и ровное направление -- первая буква заглавная.
    const float Axes[5][4] = {
        { 0.7f, 0.1f, 0.1f, 0.1f },
        { 0.1f, 0.7f, 0.1f, 0.1f },
        { 0.1f, 0.1f, 0.7f, 0.1f },
        { 0.1f, 0.1f, 0.1f, 0.7f },
        { 0.25f, 0.25f, 0.25f, 0.25f },
    };
    for (const float* A : Axes)
    {
        FRealState State;
        State.Magnitude = 0.5f;
        State.Direction.Body = A[0];
        State.Direction.Mind = A[1];
        State.Direction.Spirit = A[2];
        State.Direction.Nature = A[3];
        const FString Line = HerbalistSensation::Describe(State);
        if (!TestFalse(TEXT("Строка не пуста"), Line.IsEmpty())) continue;
        TestEqual(FString::Printf(TEXT("С заглавной: %s"), *Line), Line[0], HerbalistCore::Text::ToUpperRu(Line[0]));
    }
    return true;
}

#endif
