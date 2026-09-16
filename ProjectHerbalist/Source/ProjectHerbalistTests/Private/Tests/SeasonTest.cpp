// Source/ProjectHerbalistTests/Private/Tests/SeasonTest.cpp
//
// Годовой круг (15_Cycles_And_Shrines.md §15.4). С 2026-09-16 -- календарь
// 365 суток, четыре метеорологических сезона по месяцам, сутки 0 -- 1 марта
// (Core/Types/HerbalistCalendar.h); по лору сезонов три, осень -- часть Лета.
// Эффекты: Весна ускоряет спад HarvestStress (StressRecoveryMultiplier), Зима
// замедляет его и поднимает Purity по всей сетке ("снег как чистота"); Лето и
// Осень нейтральны.
// DispatchBeginPlay-паттерн — тот же, что BistabilityTest.cpp/MoonPhaseTest.cpp.

#include "Core/World/GridWorldManager.h"
#include "Core/Types/HerbalistCalendar.h"
#include "Core/Types/BiomeTypes.h"
#include "Misc/AutomationTest.h"
#include "Editor.h"
#include "Engine/World.h"

#if WITH_AUTOMATION_TESTS && WITH_EDITOR

#include "TestWorldHelpers.h"

namespace
{
    const float SeasonTestDayLengthSeconds = 32.0f * 60.0f;

    // Середина Дня (6–20 минут суток) -- не Рассвет/Закат, у них свой эффект на Purity.
    float SeasonTestClockAt(int32 Month, int32 Day)
    {
        return HerbalistCore::Calendar::DayOfYearFromDate(Month, Day) * SeasonTestDayLengthSeconds + 10.0f * 60.0f;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistSeason_CalendarDatesStartOnMarchFirst,
    "Herbalist.Season.CalendarDatesStartOnMarchFirst",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistSeason_CalendarDatesStartOnMarchFirst::RunTest(const FString& Parameters)
{
    using namespace HerbalistCore::Calendar;

    // Длины месяцев в сумме -- год.
    int32 Sum = 0;
    for (int32 Length : MonthLengthsFromMarch) Sum += Length;
    TestEqual(TEXT("12 месяцев -- 365 суток"), Sum, DaysPerYear);

    struct FCase { int32 DayOfYear; int32 Month; int32 Day; };
    for (const FCase& Case : { FCase{ 0, 3, 1 }, FCase{ 30, 3, 31 }, FCase{ 31, 4, 1 }, FCase{ 92, 6, 1 },
                               FCase{ 114, 6, 23 }, FCase{ 184, 9, 1 }, FCase{ 275, 12, 1 }, FCase{ 306, 1, 1 },
                               FCase{ 364, 2, 28 }, FCase{ 365, 3, 1 } })
    {
        const FCalendarDate Date = DateFromDayOfYear(Case.DayOfYear);
        TestEqual(*FString::Printf(TEXT("День %d -- месяц"), Case.DayOfYear), Date.Month, Case.Month);
        TestEqual(*FString::Printf(TEXT("День %d -- число"), Case.DayOfYear), Date.Day, Case.Day);
        TestEqual(*FString::Printf(TEXT("%d.%02d -- обратно в день года"), Case.Day, Case.Month),
            DayOfYearFromDate(Case.Month, Case.Day), Case.DayOfYear % DaysPerYear);
    }

    // Сезоны по три месяца: 92 + 92 + 91 + 90.
    TestEqual(TEXT("Весна 92 суток"), SeasonLengthDays(ESeason::Spring), 92);
    TestEqual(TEXT("Лето 92 суток"), SeasonLengthDays(ESeason::Summer), 92);
    TestEqual(TEXT("Осень 91 сутки"), SeasonLengthDays(ESeason::Autumn), 91);
    TestEqual(TEXT("Зима 90 суток"), SeasonLengthDays(ESeason::Winter), 90);

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;
    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;

    Manager->SetGameClockSeconds(0.0f);
    TestEqual(TEXT("Часы 0 -- 1 марта: месяц"), Manager->GetCalendarMonth(), 3);
    TestEqual(TEXT("Часы 0 -- 1 марта: число"), Manager->GetCalendarDay(), 1);

    Manager->SetGameClockSeconds(SeasonTestClockAt(1, 1));
    TestEqual(TEXT("1 января: день года"), Manager->GetDayOfYear(), 306);
    TestEqual(TEXT("1 января: месяц"), Manager->GetCalendarMonth(), 1);

    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistSeason_CyclesThroughFourSeasonsByMonth,
    "Herbalist.Season.CyclesThroughFourSeasonsByMonth",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistSeason_CyclesThroughFourSeasonsByMonth::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;

    struct FCase { int32 Month; int32 Day; ESeason Season; ESeason Lore; };
    for (const FCase& Case : { FCase{ 3, 1, ESeason::Spring, ESeason::Spring }, FCase{ 5, 31, ESeason::Spring, ESeason::Spring },
                               FCase{ 6, 1, ESeason::Summer, ESeason::Summer }, FCase{ 8, 31, ESeason::Summer, ESeason::Summer },
                               FCase{ 9, 1, ESeason::Autumn, ESeason::Summer }, FCase{ 11, 30, ESeason::Autumn, ESeason::Summer },
                               FCase{ 12, 1, ESeason::Winter, ESeason::Winter }, FCase{ 2, 28, ESeason::Winter, ESeason::Winter } })
    {
        Manager->SetGameClockSeconds(SeasonTestClockAt(Case.Month, Case.Day));
        TestEqual(*FString::Printf(TEXT("%d.%02d -- технический сезон"), Case.Day, Case.Month), Manager->GetSeason(), Case.Season);
        TestEqual(*FString::Printf(TEXT("%d.%02d -- сезон по лору"), Case.Day, Case.Month), Manager->GetLoreSeason(), Case.Lore);
    }

    // Год замыкается: 1 марта следующего года -- снова Весна, начало сезона.
    Manager->SetGameClockSeconds(HerbalistCore::Calendar::DaysPerYear * SeasonTestDayLengthSeconds);
    TestEqual(TEXT("Через 365 суток -- снова Весна"), Manager->GetSeason(), ESeason::Spring);
    TestTrue(TEXT("Прогресс сезона в его первый миг -- 0"), Manager->GetSeasonProgress01() < 0.001f);

    // Прогресс -- по суткам внутри сезона: середина осени (15 октября, 44-е
    // сутки из 91 плюс треть суток) около половины.
    Manager->SetGameClockSeconds(SeasonTestClockAt(10, 15));
    TestTrue(TEXT("15 октября -- около середины осени"), FMath::Abs(Manager->GetSeasonProgress01() - (44.0f + 10.0f / 32.0f) / 91.0f) < 0.001f);

    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistSeason_SpringSpeedsUpStressRecoveryWinterSlowsIt,
    "Herbalist.Season.SpringSpeedsUpStressRecoveryWinterSlowsIt",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistSeason_SpringSpeedsUpStressRecoveryWinterSlowsIt::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    auto MeasureStressAfterOneTick = [&](float GameClockSeconds) -> float
    {
        AGridWorldManager* Manager = SpawnAndBeginPlay(World);
        FGridCell* Cell = Manager->GetCell(0, 0);
        Cell->Biome = EBiomeType::MixedForest;
        Cell->bIsWater = false;
        Cell->HarvestStress = 1.0f;
        Manager->SetGameClockSeconds(GameClockSeconds);
        Manager->RegenerateCellParameters(3600.0f);   // крупный DeltaTime -- разница видна сразу
        const float Result = Cell->HarvestStress;
        Manager->Destroy();
        return Result;
    };

    const float SpringStress = MeasureStressAfterOneTick(SeasonTestClockAt(4, 15));
    const float SummerStress = MeasureStressAfterOneTick(SeasonTestClockAt(7, 15));
    const float AutumnStress = MeasureStressAfterOneTick(SeasonTestClockAt(10, 15));
    const float WinterStress = MeasureStressAfterOneTick(SeasonTestClockAt(1, 15));

    TestTrue(TEXT("Spring recovers HarvestStress faster than Summer (lower remaining stress)"), SpringStress < SummerStress);
    TestTrue(TEXT("Winter recovers HarvestStress slower than Summer (higher remaining stress)"), WinterStress > SummerStress);
    TestTrue(TEXT("Осень нейтральна, как Лето"), FMath::IsNearlyEqual(AutumnStress, SummerStress, 1.0e-4f));

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistSeason_WinterRaisesPurityAcrossTheGrid,
    "Herbalist.Season.WinterRaisesPurityAcrossTheGrid",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistSeason_WinterRaisesPurityAcrossTheGrid::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;

    // Тундра -- сегодня нет ни одного другого определения на этом биоме
    // (AmbientEntityTypes.h), эффект виден изолированно (тот же приём, что
    // NightHorrorAffectsEveryBiomeWithoutClaimingTheCell в AmbientEntityTest.cpp).
    FGridCell* Cell = Manager->GetCell(0, 0);
    if (!TestNotNull(TEXT("Cell (0,0) exists"), Cell)) { Manager->Destroy(); return false; }
    Cell->Biome = EBiomeType::Tundra;
    Cell->bIsWater = false;

    Manager->SetGameClockSeconds(SeasonTestClockAt(4, 15));   // Весна, середина Дня
    const float PurityBeforeSpring = Cell->TargetState.Meta.Purity;
    Manager->UpdateEntityManifestations(1.0f);
    TestEqual(TEXT("No Purity change in Spring on a biome with no other definition"),
        Cell->TargetState.Meta.Purity, PurityBeforeSpring);

    Manager->SetGameClockSeconds(SeasonTestClockAt(10, 15));   // Осень, середина Дня
    const float PurityBeforeAutumn = Cell->TargetState.Meta.Purity;
    Manager->UpdateEntityManifestations(1.0f);
    TestEqual(TEXT("Осенью чистота не растёт -- снег только зимой"), Cell->TargetState.Meta.Purity, PurityBeforeAutumn);

    Manager->SetGameClockSeconds(SeasonTestClockAt(1, 15));   // Зима, середина Дня
    const float PurityBeforeWinter = Cell->TargetState.Meta.Purity;
    Manager->UpdateEntityManifestations(1.0f);
    TestTrue(TEXT("Purity rises in Winter ('снег как чистота')"), Cell->TargetState.Meta.Purity > PurityBeforeWinter);

    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistSeason_ClockKeepsRunningThroughWinter,
    "Herbalist.Season.ClockKeepsRunningThroughWinter",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistSeason_ClockKeepsRunningThroughWinter::RunTest(const FString& Parameters)
{
    // Часы во float с 2^19 с (конец ноября) теряли кадр 1/60 с целиком и
    // вставали -- зима в непрерывной игре не наступала (ревью 2026-09-16).
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;
    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;

    const double Start = HerbalistCore::Calendar::DayOfYearFromDate(12, 31) * static_cast<double>(SeasonTestDayLengthSeconds);
    Manager->SetGameClockSeconds(Start);
    const int32 Frames = 60 * 60;   // минута игры при 60 кадрах
    for (int32 Frame = 0; Frame < Frames; ++Frame)
    {
        Manager->AdvanceGameClock(1.0f / 60.0f);
    }
    TestTrue(TEXT("Минута кадров 1/60 с -- ровно минута игрового времени"),
        FMath::IsNearlyEqual(Manager->GetGameClockSeconds() - Start, 60.0, 0.01));

    // Третий год, та же проверка -- часы идут и дальше.
    Manager->SetGameClockSeconds(Start + 2.0 * HerbalistCore::Calendar::DaysPerYear * SeasonTestDayLengthSeconds);
    const double ThirdYear = Manager->GetGameClockSeconds();
    for (int32 Frame = 0; Frame < Frames; ++Frame)
    {
        Manager->AdvanceGameClock(1.0f / 60.0f);
    }
    TestTrue(TEXT("Через два года часы идут так же"), FMath::IsNearlyEqual(Manager->GetGameClockSeconds() - ThirdYear, 60.0, 0.01));
    TestEqual(TEXT("31 декабря -- зима"), Manager->GetSeason(), ESeason::Winter);

    Manager->Destroy();
    return true;
}

#endif // WITH_AUTOMATION_TESTS && WITH_EDITOR
