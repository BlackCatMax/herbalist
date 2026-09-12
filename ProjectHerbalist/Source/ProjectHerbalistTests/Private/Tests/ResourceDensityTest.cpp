// Source/ProjectHerbalistTests/Private/Tests/ResourceDensityTest.cpp
//
// Разметка мира, этап 2 (2026-09-12) -- плотность ресурсов на площадь
// (решение пользователя 7, DESIGN_World_Layout.md §5). Число ресурсов в клетке
// -- плотность на 100 м², пересчитанная на площадь клетки и затухание к краю
// региона, с розыгрышем дробной части. Проверяется без мира:
// AGridWorldManager::RollResourceCount статическая.

#include "Core/World/GridWorldManager.h"
#include "Core/World/BiomeRegionVolume.h"
#include "Misc/AutomationTest.h"
#include "Math/RandomStream.h"
#include "UObject/UnrealType.h"

#if WITH_AUTOMATION_TESTS

namespace
{
    // Среднее число ресурсов на 100 м² за Rolls бросков на клетке CellSizeCm.
    double MeanPer100SquareMeters(float MinPer100, float MaxPer100, double CellSizeCm, int32 Rolls, int32 Seed,
        float DensityScale = 1.0f)
    {
        FRandomStream Rng(Seed);
        int64 Total = 0;
        for (int32 Roll = 0; Roll < Rolls; ++Roll)
        {
            Total += AGridWorldManager::RollResourceCount(MinPer100, MaxPer100, CellSizeCm, Rng, DensityScale);
        }
        const double CellAreaIn100SquareMeters = FMath::Square(CellSizeCm / 100.0) / 100.0;
        return static_cast<double>(Total) / Rolls / CellAreaIn100SquareMeters;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistResourceDensity_DensityPerAreaDoesNotDependOnCellSize,
    "Herbalist.ResourceDensity.DensityPerAreaDoesNotDependOnCellSize",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistResourceDensity_DensityPerAreaDoesNotDependOnCellSize::RunTest(const FString& Parameters)
{
    // 1..3 на 100 м² -- в среднем 2. Число бросков выбрано так, чтобы допуск
    // 0.05 был не меньше 7 стандартных отклонений среднего: на клетке 2 м в
    // клетке 0 или 1 ресурс (p ~ 0.08), разброс велик -- там миллион бросков
    // (σ ~ 0.007); на клетках 9, 10 и 50 м хватает 40 000 (σ ~ 0.003-0.004).
    struct FCase { double CellSizeCm; int32 Rolls; };
    const FCase Cases[] = { { 900.0, 40000 }, { 1000.0, 40000 }, { 5000.0, 40000 }, { 200.0, 1000000 } };
    for (const FCase& Case : Cases)
    {
        const double Mean = MeanPer100SquareMeters(1.0f, 3.0f, Case.CellSizeCm, Case.Rolls, 12345);
        TestEqual(FString::Printf(TEXT("Клетка %.0f см: в среднем 2 ресурса на 100 м²"), Case.CellSizeCm), Mean, 2.0, 0.05);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistResourceDensity_FalloffScalesDensityOnAnyCell,
    "Herbalist.ResourceDensity.FalloffScalesDensityOnAnyCell",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistResourceDensity_FalloffScalesDensityOnAnyCell::RunTest(const FString& Parameters)
{
    // Найдено ревью: затухание округляло уже целое число ресурсов, и на мелкой
    // клетке 0.4 от одного ресурса давало ноль. Теперь множитель внутри
    // броска: 1..3 на 100 м² x 0.4 = в среднем 0.8 на любой клетке.
    TestEqual(TEXT("Клетка 10 м, затухание 0.4 -- в среднем 0.8 на 100 м²"),
        MeanPer100SquareMeters(1.0f, 3.0f, 1000.0, 40000, 99, 0.4f), 0.8, 0.05);
    TestEqual(TEXT("Клетка 2 м, затухание 0.4 -- те же 0.8"),
        MeanPer100SquareMeters(1.0f, 3.0f, 200.0, 1000000, 99, 0.4f), 0.8, 0.05);
    TestEqual(TEXT("Затухание 0 -- ни одного ресурса"),
        MeanPer100SquareMeters(1.0f, 3.0f, 900.0, 1000, 99, 0.0f), 0.0, 1e-9);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistResourceDensity_ZeroDensityRollsNothing,
    "Herbalist.ResourceDensity.ZeroDensityRollsNothing",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistResourceDensity_ZeroDensityRollsNothing::RunTest(const FString& Parameters)
{
    FRandomStream Rng(7);
    int32 Total = 0;
    for (int32 Roll = 0; Roll < 1000; ++Roll)
    {
        Total += AGridWorldManager::RollResourceCount(0.0f, 0.0f, 900.0, Rng);
    }
    TestEqual(TEXT("Нулевая плотность -- ни одного ресурса"), Total, 0);
    TestEqual(TEXT("Отрицательные значения не дают отрицательного числа"),
        AGridWorldManager::RollResourceCount(-5.0f, -1.0f, 900.0, Rng), 0);
    TestEqual(TEXT("NaN -- ноль, а не -2^30"),
        AGridWorldManager::RollResourceCount(1.0f, NAN, 900.0, Rng), 0);
    const int32 Huge = AGridWorldManager::RollResourceCount(1.0e12f, 1.0e12f, 5000.0, Rng);
    TestTrue(TEXT("Огромная плотность упирается в предел, а не переполняется"),
        Huge >= 0 && Huge <= AGridWorldManager::MaxResourcesPerCell);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistResourceDensity_TenMeterCellKeepsAuthoredRange,
    "Herbalist.ResourceDensity.TenMeterCellKeepsAuthoredRange",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistResourceDensity_TenMeterCellKeepsAuthoredRange::RunTest(const FString& Parameters)
{
    // Регионы L_TestDev задавали 100..200 на клетку 10 м = 100 м². После
    // перехода на площадь клетка 10 м обязана давать тот же диапазон.
    FRandomStream Rng(2026);
    int32 Lowest = TNumericLimits<int32>::Max();
    int32 Highest = 0;
    for (int32 Roll = 0; Roll < 5000; ++Roll)
    {
        const int32 Count = AGridWorldManager::RollResourceCount(100.0f, 200.0f, 1000.0, Rng);
        Lowest = FMath::Min(Lowest, Count);
        Highest = FMath::Max(Highest, Count);
    }
    TestTrue(TEXT("Не меньше 100"), Lowest >= 100);
    TestTrue(TEXT("Не больше 200"), Highest <= 200);

    // Перепутанные Min и Max -- тот же поток бросков, та же сумма.
    FRandomStream Straight(555);
    FRandomStream Swapped(555);
    int32 StraightTotal = 0;
    int32 SwappedTotal = 0;
    for (int32 Roll = 0; Roll < 1000; ++Roll)
    {
        StraightTotal += AGridWorldManager::RollResourceCount(1.0f, 3.0f, 900.0, Straight);
        SwappedTotal += AGridWorldManager::RollResourceCount(3.0f, 1.0f, 900.0, Swapped);
    }
    TestEqual(TEXT("Min и Max перепутаны местами -- ровно то же самое"), SwappedTotal, StraightTotal);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistResourceDensity_OldPerCellFieldsRedirect,
    "Herbalist.ResourceDensity.OldPerCellFieldsRedirect",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistResourceDensity_OldPerCellFieldsRedirect::RunTest(const FString& Parameters)
{
    // Уже расставленные регионы сохранены с MinResourcesPerCell /
    // MaxResourcesPerCell. Проверяется тем же путём, каким имя ищет загрузчик
    // тегированных свойств (FProperty::FindRedirectedPropertyName по классу), а
    // не разбором строки из ini (найдено ревью: тот тест проверял сам себя).
    // Целое в float движок переводит при загрузке сам
    // (TProperty_Numeric::ConvertFromType).
    const UClass* RegionClass = ABiomeRegionVolume::StaticClass();
    const FName NewMin = FProperty::FindRedirectedPropertyName(RegionClass, FName(TEXT("MinResourcesPerCell")));
    const FName NewMax = FProperty::FindRedirectedPropertyName(RegionClass, FName(TEXT("MaxResourcesPerCell")));
    TestEqual(TEXT("MinResourcesPerCell -> MinResourcesPer100SquareMeters"), NewMin, FName(TEXT("MinResourcesPer100SquareMeters")));
    TestEqual(TEXT("MaxResourcesPerCell -> MaxResourcesPer100SquareMeters"), NewMax, FName(TEXT("MaxResourcesPer100SquareMeters")));
    TestNotNull(TEXT("Новое поле Min существует и дробное"), FindFProperty<FFloatProperty>(RegionClass, NewMin));
    TestNotNull(TEXT("Новое поле Max существует и дробное"), FindFProperty<FFloatProperty>(RegionClass, NewMax));
    return true;
}

#endif // WITH_AUTOMATION_TESTS
