// Source/ProjectHerbalistTests/Private/Tests/TimeDisplayTest.cpp
//
// Время в материалах (2026-09-16, этап 1б docs/research/
// DESIGN_Living_Vegetation_Research.md §2): веса суток и сезонов в сумме 1,
// без скачков на границах фаз и сезонов, SeasonUDW целый в серединах сезонов,
// листопад и луна в опорных точках, запись в MPC_WorldStateFields и перемотка
// часов. Формулы -- Core/Types/HerbalistTimeDisplay.h.

#include "Core/World/GridWorldManager.h"
#include "Core/Types/HerbalistCalendar.h"
#include "Core/Types/HerbalistTimeDisplay.h"
#include "Core/Config/HerbalistSettings.h"
#include "Materials/MaterialParameterCollection.h"
#include "Materials/MaterialParameterCollectionInstance.h"
#include "Misc/AutomationTest.h"
#include "Editor.h"
#include "Engine/World.h"
#include "Core/World/Sky/LeafFallSubsystem.h"

#if WITH_AUTOMATION_TESTS && WITH_EDITOR

#include "TestWorldHelpers.h"

namespace
{
    // Длина суток из настроек: тесты не должны ломаться от другой GameDayMinutes.
    double TimeDisplayDaySecondsFromSettings() { return GetDefault<UHerbalistSettings>()->GameDayMinutes * 60.0; }

    float WeightSum(const FLinearColor& Weights)
    {
        return Weights.R + Weights.G + Weights.B + Weights.A;
    }

    float MaxComponentDelta(const FLinearColor& A, const FLinearColor& B)
    {
        return FMath::Max(FMath::Max(FMath::Abs(A.R - B.R), FMath::Abs(A.G - B.G)),
            FMath::Max(FMath::Abs(A.B - B.B), FMath::Abs(A.A - B.A)));
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistTimeDisplay_DayPhaseWeightsSumToOneWithoutJumps,
    "Herbalist.TimeDisplay.DayPhaseWeightsSumToOneWithoutJumps",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistTimeDisplay_DayPhaseWeightsSumToOneWithoutJumps::RunTest(const FString& Parameters)
{
    using namespace HerbalistCore::TimeDisplay;
    const float DayMinutes = 32.0f;
    const float BlendMinutes = 1.0f;

    // Середины фаз §15.2 -- чистые.
    TestTrue(TEXT("Минута 3 -- рассвет"), DayPhaseWeights(3.0f / DayMinutes, DayMinutes, BlendMinutes).Equals(FLinearColor(1, 0, 0, 0), 1.0e-4f));
    TestTrue(TEXT("Минута 13 -- день"), DayPhaseWeights(13.0f / DayMinutes, DayMinutes, BlendMinutes).Equals(FLinearColor(0, 1, 0, 0), 1.0e-4f));
    TestTrue(TEXT("Минута 23 -- закат"), DayPhaseWeights(23.0f / DayMinutes, DayMinutes, BlendMinutes).Equals(FLinearColor(0, 0, 1, 0), 1.0e-4f));
    TestTrue(TEXT("Минута 29 -- ночь"), DayPhaseWeights(29.0f / DayMinutes, DayMinutes, BlendMinutes).Equals(FLinearColor(0, 0, 0, 1), 1.0e-4f));

    // Границы -- поровну, включая переход через полночь суток.
    TestTrue(TEXT("Минута 6 -- рассвет и день поровну"), DayPhaseWeights(6.0f / DayMinutes, DayMinutes, BlendMinutes).Equals(FLinearColor(0.5f, 0.5f, 0, 0), 1.0e-3f));
    TestTrue(TEXT("Начало суток -- ночь и рассвет поровну"), DayPhaseWeights(0.0f, DayMinutes, BlendMinutes).Equals(FLinearColor(0.5f, 0, 0, 0.5f), 1.0e-3f));

    // Сутки посекундно: сумма 1, веса в [0,1], шаг не больше наклона
    // smoothstep (1.5 на ширину перехода, за секунду -- 1.5 / 60 / ширина) с
    // запасом 20%; переход через конец суток -- тот же шаг.
    const int32 Samples = 32 * 60;
    FLinearColor Previous = DayPhaseWeights(0.0f, DayMinutes, BlendMinutes);
    float WorstSum = 0.0f;
    float WorstStep = 0.0f;
    for (int32 Index = 1; Index <= Samples; ++Index)
    {
        const float Time01 = static_cast<float>(Index % Samples) / Samples;
        const FLinearColor Current = DayPhaseWeights(Time01, DayMinutes, BlendMinutes);
        WorstSum = FMath::Max(WorstSum, FMath::Abs(WeightSum(Current) - 1.0f));
        WorstStep = FMath::Max(WorstStep, MaxComponentDelta(Current, Previous));
        TestTrue(TEXT("Веса в [0,1]"), Current.GetMin() >= 0.0f && Current.GetMax() <= 1.0f);
        Previous = Current;
    }
    TestTrue(*FString::Printf(TEXT("Сумма весов суток -- 1 (худшее отклонение %.6f)"), WorstSum), WorstSum < 1.0e-4f);
    const float StepLimit = 1.2f * 1.5f / 60.0f / BlendMinutes;
    TestTrue(*FString::Printf(TEXT("Без скачков за секунду (худший шаг %.4f, предел %.4f)"), WorstStep, StepLimit), WorstStep < StepLimit);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistTimeDisplay_SeasonCurveWholeAtMidSeasonsAndContinuous,
    "Herbalist.TimeDisplay.SeasonCurveWholeAtMidSeasonsAndContinuous",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistTimeDisplay_SeasonCurveWholeAtMidSeasonsAndContinuous::RunTest(const FString& Parameters)
{
    using namespace HerbalistCore::Calendar;

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;
    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;

    // Середины сезонов в сутках: SeasonUDW целый (шкала UDS), вес сезона 1,
    // листопад -- 0 весной и летом, smoothstep(1/3) = 7/27 осенью, 1 зимой.
    struct FCase { ESeason Season; float UDW; float LeafDrop; };
    for (const FCase& Case : { FCase{ ESeason::Spring, 0.0f, 0.0f }, FCase{ ESeason::Summer, 1.0f, 0.0f },
                               FCase{ ESeason::Autumn, 2.0f, 7.0f / 27.0f }, FCase{ ESeason::Winter, 3.0f, 1.0f } })
    {
        const double MidDay = SeasonStartDayOfYear(Case.Season) + SeasonLengthDays(Case.Season) * 0.5;
        Manager->SetGameClockSeconds(MidDay * TimeDisplayDaySecondsFromSettings());
        const FString Name = UEnum::GetValueAsString(Case.Season);
        TestTrue(*FString::Printf(TEXT("%s: середина -- SeasonUDW %.1f (есть %.4f)"), *Name, Case.UDW, Manager->GetSeasonUDW()),
            FMath::IsNearlyEqual(Manager->GetSeasonUDW(), Case.UDW, 1.0e-3f));
        TestTrue(*FString::Printf(TEXT("%s: середина -- вес сезона 1"), *Name),
            Manager->GetSeasonWeights().Component(HerbalistCore::TimeDisplay::UDSSeasonIndex(Case.Season)) > 0.999f);
        TestTrue(*FString::Printf(TEXT("%s: листопад %.1f (есть %.4f)"), *Name, Case.LeafDrop, Manager->GetLeafDrop01()),
            FMath::IsNearlyEqual(Manager->GetLeafDrop01(), Case.LeafDrop, 1.0e-3f));
    }

    // Год по 1/8 суток и ещё сутки сверху (замыкание года): сумма весов 1,
    // SeasonUDW и веса без скачков на границах сезонов разной длины.
    const int32 StepsPerDay = 8;
    const int32 Steps = (DaysPerYear + 1) * StepsPerDay;
    Manager->SetGameClockSeconds(0.0);
    float PreviousUDW = Manager->GetSeasonUDW();
    FLinearColor PreviousWeights = Manager->GetSeasonWeights();
    float WorstUDWStep = 0.0f;
    float WorstWeightStep = 0.0f;
    float WorstSum = 0.0f;
    for (int32 Step = 1; Step <= Steps; ++Step)
    {
        Manager->SetGameClockSeconds(Step * TimeDisplayDaySecondsFromSettings() / StepsPerDay);
        const float UDW = Manager->GetSeasonUDW();
        const FLinearColor Weights = Manager->GetSeasonWeights();
        float UDWStep = FMath::Abs(UDW - PreviousUDW);
        UDWStep = FMath::Min(UDWStep, 4.0f - UDWStep);   // 3.99 -> 0.0 -- соседние значения по кругу
        WorstUDWStep = FMath::Max(WorstUDWStep, UDWStep);
        WorstWeightStep = FMath::Max(WorstWeightStep, MaxComponentDelta(Weights, PreviousWeights));
        WorstSum = FMath::Max(WorstSum, FMath::Abs(WeightSum(Weights) - 1.0f));
        PreviousUDW = UDW;
        PreviousWeights = Weights;
    }
    // Самый короткий сезон 90 суток: шаг 1/8 суток -- 1/720 ≈ 0.0014.
    TestTrue(*FString::Printf(TEXT("SeasonUDW без скачков (худший шаг %.5f)"), WorstUDWStep), WorstUDWStep < 0.003f);
    TestTrue(*FString::Printf(TEXT("Веса сезонов без скачков (худший шаг %.5f)"), WorstWeightStep), WorstWeightStep < 0.003f);
    TestTrue(*FString::Printf(TEXT("Сумма весов сезонов -- 1 (худшее отклонение %.6f)"), WorstSum), WorstSum < 1.0e-4f);

    // Листопад летом нулевой до последнего дня, в начале осени только начинается.
    Manager->SetGameClockSeconds((DayOfYearFromDate(8, 31) + 0.99) * TimeDisplayDaySecondsFromSettings());
    TestEqual(TEXT("31 августа листва на месте"), Manager->GetLeafDrop01(), 0.0f);
    Manager->SetGameClockSeconds(DayOfYearFromDate(9, 2) * TimeDisplayDaySecondsFromSettings());
    TestTrue(*FString::Printf(TEXT("2 сентября листопад начался, но едва (%.4f)"), Manager->GetLeafDrop01()),
        Manager->GetLeafDrop01() > 0.0f && Manager->GetLeafDrop01() < 0.01f);

    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistTimeDisplay_MoonFullPeaksInFullMoon,
    "Herbalist.TimeDisplay.MoonFullPeaksInFullMoon",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistTimeDisplay_MoonFullPeaksInFullMoon::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;
    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;

    const double PhaseDays = GetDefault<UHerbalistSettings>()->StressRecoveryGameDays;

    // Середина третьей фазы -- Полнолуние, первой -- Новолуние.
    Manager->SetGameClockSeconds(PhaseDays * 2.5 * TimeDisplayDaySecondsFromSettings());
    TestEqual(TEXT("Середина третьей фазы -- Полнолуние"), Manager->GetMoonPhase(), EMoonPhase::FullMoon);
    TestTrue(*FString::Printf(TEXT("MoonFull01 в середине Полнолуния -- 1 (есть %.4f)"), Manager->GetMoonFull01()), Manager->GetMoonFull01() > 0.999f);

    Manager->SetGameClockSeconds(PhaseDays * 0.5 * TimeDisplayDaySecondsFromSettings());
    TestEqual(TEXT("Середина первой фазы -- Новолуние"), Manager->GetMoonPhase(), EMoonPhase::NewMoon);
    TestTrue(*FString::Printf(TEXT("MoonFull01 в середине Новолуния -- 0 (есть %.4f)"), Manager->GetMoonFull01()), Manager->GetMoonFull01() < 0.001f);

    // Фаза выводится из той же доли цикла, что и MoonFull01.
    Manager->SetGameClockSeconds(PhaseDays * 1.99 * TimeDisplayDaySecondsFromSettings());
    TestEqual(TEXT("Конец второй фазы -- Растущая"), Manager->GetMoonPhase(), EMoonPhase::WaxingMoon);

    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistTimeDisplay_ParametersReachTheMaterialCollection,
    "Herbalist.TimeDisplay.ParametersReachTheMaterialCollection",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistTimeDisplay_ParametersReachTheMaterialCollection::RunTest(const FString& Parameters)
{
    // Материал знает о времени только через MPC: параметры должны быть
    // заведены (-run=TimeDisplaySetup) и получать ровно то, что считает менеджер.
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    UMaterialParameterCollection* Collection = GetDefault<UHerbalistSettings>()->TimeDisplayCollection.LoadSynchronous();
    if (!TestNotNull(TEXT("TimeDisplayCollection назначен в Herbalist Settings"), Collection)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;

    // 15 октября, минута 22 (закат), середина лунного цикла.
    Manager->SetGameClockSeconds(HerbalistCore::Calendar::DayOfYearFromDate(10, 15) * TimeDisplayDaySecondsFromSettings() + 22.0 * 60.0);
    TestTrue(TEXT("Все девять параметров заведены в коллекции"), Manager->WriteTimeDisplayParameters(Collection));

    UMaterialParameterCollectionInstance* Instance = World->GetParameterCollectionInstance(Collection);
    if (!TestNotNull(TEXT("Экземпляр коллекции в мире"), Instance)) { Manager->Destroy(); return false; }

    auto CheckScalar = [&](const TCHAR* Name, float Expected)
    {
        float Value = -1.0f;
        TestTrue(*FString::Printf(TEXT("%s читается"), Name), Instance->GetScalarParameterValue(FName(Name), Value));
        TestTrue(*FString::Printf(TEXT("%s = %.4f (есть %.4f)"), Name, Expected, Value), FMath::IsNearlyEqual(Value, Expected, 1.0e-4f));
    };
    auto CheckVector = [&](const TCHAR* Name, const FLinearColor& Expected)
    {
        FLinearColor Value(-1, -1, -1, -1);
        TestTrue(*FString::Printf(TEXT("%s читается"), Name), Instance->GetVectorParameterValue(FName(Name), Value));
        TestTrue(*FString::Printf(TEXT("%s = %s (есть %s)"), Name, *Expected.ToString(), *Value.ToString()), Value.Equals(Expected, 1.0e-4f));
    };

    CheckScalar(TEXT("TimeOfDay01"), Manager->GetTimeOfDay01());
    CheckScalar(TEXT("SeasonUDW"), Manager->GetSeasonUDW());
    CheckScalar(TEXT("LeafDrop01"), Manager->GetLeafDrop01());
    CheckScalar(TEXT("LeafFall01"), Manager->GetLeafFall01());
    CheckScalar(TEXT("LeafLitter01"), Manager->GetLeafLitter01());
    TestTrue(TEXT("Sanity: 15 октября листья падают"), Manager->GetLeafFall01() > 0.1f);
    CheckScalar(TEXT("MoonFull01"), Manager->GetMoonFull01());
    CheckScalar(TEXT("Morok01"), Manager->GetMorokDisplay01());

    // Сглаживание Морока в кадре: за одну постоянную времени -- ~63% пути,
    // без сглаживания -- сразу цель.
    using HerbalistCore::TimeDisplay::SmoothToward;
    TestTrue(TEXT("Morok01: за Tau -- 63% пути"), FMath::IsNearlyEqual(SmoothToward(0.0f, 1.0f, 2.0f, 2.0f), 1.0f - FMath::Exp(-1.0f), 1.0e-4f));
    TestEqual(TEXT("Morok01: Tau 0 -- сразу цель"), SmoothToward(0.2f, 0.9f, 0.016f, 0.0f), 0.9f);
    TestTrue(TEXT("Morok01: к цели и вниз"), SmoothToward(0.9f, 0.1f, 1.0f, 2.0f) < 0.9f);
    CheckVector(TEXT("DayPhaseWeights"), Manager->GetDayPhaseWeights());
    CheckVector(TEXT("SeasonWeights"), Manager->GetSeasonWeights());
    TestTrue(TEXT("Sanity: минута 22 -- закат"), Manager->GetDayPhaseWeights().B > 0.999f);
    TestTrue(TEXT("Sanity: 15 октября -- осень в весах"), Manager->GetSeasonWeights().B > 0.9f);

    // Вернуть экземпляр редакторного мира к значениям коллекции по умолчанию:
    // иначе вьюпорты редактора держали бы «15 октября, закат» до перезапуска.
    for (const FCollectionScalarParameter& Scalar : Collection->ScalarParameters)
    {
        Instance->SetScalarParameterValue(Scalar.ParameterName, Scalar.DefaultValue);
    }
    for (const FCollectionVectorParameter& Vector : Collection->VectorParameters)
    {
        Instance->SetVectorParameterValue(Vector.ParameterName, Vector.DefaultValue);
    }

    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistTimeDisplay_JumpGameClockMovesDateAndResetsWardsBackward,
    "Herbalist.TimeDisplay.JumpGameClockMovesDateAndResetsWardsBackward",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistTimeDisplay_JumpGameClockMovesDateAndResetsWardsBackward::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;
    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;

    // Вперёд: дата сдвигается, стресс клетки сходит за пропущенные сутки
    // (StressRecoveryGameDays = 7, двое суток -- заметно меньше 1).
    Manager->SetGameClockSeconds(HerbalistCore::Calendar::DayOfYearFromDate(8, 30) * TimeDisplayDaySecondsFromSettings());
    FGridCell* Cell = Manager->GetCell(0, 0);
    if (!TestNotNull(TEXT("Cell (0,0) exists"), Cell)) { Manager->Destroy(); return false; }
    Cell->HarvestStress = 1.0f;
    Manager->ActivateWardBrewBoost();
    Manager->JumpGameClock(Manager->GetGameClockSeconds() + 2.0 * TimeDisplayDaySecondsFromSettings());
    TestEqual(TEXT("+2 суток от 30 августа -- 1 сентября: месяц"), Manager->GetCalendarMonth(), 9);
    TestEqual(TEXT("+2 суток от 30 августа -- 1 сентября: число"), Manager->GetCalendarDay(), 1);
    TestEqual(TEXT("1 сентября -- осень"), Manager->GetSeason(), ESeason::Autumn);
    TestTrue(*FString::Printf(TEXT("Стресс клетки сошёл за перемотанные сутки (%.3f)"), Cell->HarvestStress), Cell->HarvestStress < 0.9f);

    // Назад: оберег, поставленный в более позднее время, не должен ожить.
    Manager->ActivateWardBrewBoost();
    TestTrue(TEXT("Sanity: оберег активен"), Manager->IsWardBrewBoostActive());
    Manager->JumpGameClock(Manager->GetGameClockSeconds() - 5.0 * TimeDisplayDaySecondsFromSettings());
    TestFalse(TEXT("Перемотка назад сбрасывает оберег"), Manager->IsWardBrewBoostActive());

    // Ниже нуля часы не уходят.
    Manager->JumpGameClock(-100.0);
    TestEqual(TEXT("Отрицательное время -- 0"), Manager->GetGameClockSeconds(), 0.0);

    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistTimeDisplay_LeafFallPeaksInAutumnAndLitterLastsIntoWinter,
    "Herbalist.TimeDisplay.LeafFallPeaksInAutumnAndLitterLastsIntoWinter",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistTimeDisplay_LeafFallPeaksInAutumnAndLitterLastsIntoWinter::RunTest(const FString& Parameters)
{
    using namespace HerbalistCore::TimeDisplay;

    // Шкала SeasonUDW: 1.5 начало осени, 2.25 середина спада листвы, 3 середина
    // зимы, 3.5 конец зимы и начало весны, 4 = 0 середина весны.
    TestEqual(TEXT("Лето -- листья не падают"), LeafFall01(1.0f), 0.0f);
    TestEqual(TEXT("Начало осени -- ещё 0"), LeafFall01(1.5f), 0.0f);
    TestTrue(TEXT("Пик листопада в середине спада"), FMath::IsNearlyEqual(LeafFall01(2.25f), 1.0f, 1.0e-4f));
    TestEqual(TEXT("Середина зимы -- падать нечему"), LeafFall01(3.0f), 0.0f);
    TestEqual(TEXT("Весна -- не листопад"), LeafFall01(0.5f), 0.0f);

    TestEqual(TEXT("Подстилки летом нет"), LeafLitter01(1.2f), 0.0f);
    TestTrue(TEXT("Подстилка осенью растёт вместе с опавшей листвой"), FMath::IsNearlyEqual(LeafLitter01(2.25f), LeafDrop01(2.25f), 1.0e-5f));
    TestEqual(TEXT("С середины до конца зимы -- подстилка лежит"), LeafLitter01(3.2f), 1.0f);
    TestTrue(TEXT("Начало весны -- сходит"), FMath::IsNearlyEqual(LeafLitter01(3.75f), 0.5f, 1.0e-4f));
    TestTrue(TEXT("К середине весны сошла"), LeafLitter01(3.999f) < 0.01f && LeafLitter01(0.0f) == 0.0f);

    // Без скачков по году: шаг 1/1000 шкалы.
    float WorstFall = 0.0f;
    float WorstLitter = 0.0f;
    for (int32 Step = 1; Step <= 4000; ++Step)
    {
        const float U = Step / 1000.0f;
        const float Prev = (Step - 1) / 1000.0f;
        WorstFall = FMath::Max(WorstFall, FMath::Abs(LeafFall01(U) - LeafFall01(Prev)));
        WorstLitter = FMath::Max(WorstLitter, FMath::Abs(LeafLitter01(U) - LeafLitter01(Prev)));
    }
    TestTrue(*FString::Printf(TEXT("Темп листопада без скачков (%.4f)"), WorstFall), WorstFall < 0.01f);
    TestTrue(*FString::Printf(TEXT("Подстилка без скачков (%.4f)"), WorstLitter), WorstLitter < 0.01f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistTimeDisplay_LeafFallOnlyInDeciduousBiomesAndFollowsWind,
    "Herbalist.TimeDisplay.LeafFallOnlyInDeciduousBiomesAndFollowsWind",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistTimeDisplay_LeafFallOnlyInDeciduousBiomesAndFollowsWind::RunTest(const FString& Parameters)
{
    TestEqual(TEXT("Не лиственный биом -- 0"), ULeafFallSubsystem::LeafFallRate(1.0f, 1.0f, false, 0.6f), 0.0f);
    TestTrue(TEXT("Штиль -- доля без ветра"), FMath::IsNearlyEqual(ULeafFallSubsystem::LeafFallRate(1.0f, 0.0f, true, 0.6f), 0.4f, 1.0e-4f));
    TestTrue(TEXT("Полный ветер -- полный темп"), FMath::IsNearlyEqual(ULeafFallSubsystem::LeafFallRate(1.0f, 1.0f, true, 0.6f), 1.0f, 1.0e-4f));
    TestEqual(TEXT("Не сезон -- 0 при любом ветре"), ULeafFallSubsystem::LeafFallRate(0.0f, 1.0f, true, 0.6f), 0.0f);

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;
    ULeafFallSubsystem* LeafFall = World->GetSubsystem<ULeafFallSubsystem>();
    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;
    if (!TestNotNull(TEXT("Подсистема листопада"), LeafFall)) { Manager->Destroy(); return false; }

    // Пик листопада, биом клетки под точкой решает.
    const double DayLength = TimeDisplayDaySecondsFromSettings();
    Manager->SetGameClockSeconds((HerbalistCore::Calendar::DayOfYearFromDate(10, 15) + 0.3) * DayLength);
    FGridCell* Cell = Manager->GetCell(0, 0);
    if (!TestNotNull(TEXT("Cell (0,0)"), Cell)) { Manager->Destroy(); return false; }
    // Середина клетки: GetCellWorldPosition -- её угол, ровно на границе.
    const FVector Location = Manager->GetCellWorldPosition(0, 0) + FVector(Manager->CellSize * 0.5, Manager->CellSize * 0.5, 0.0);

    Cell->bIsWater = false;
    Cell->Biome = EBiomeType::BroadleafForest;
    TestTrue(TEXT("Широколиственный лес в октябре -- листья падают"), LeafFall->ComputeLeafFallRate(*Manager, Location) > 0.1f);
    Cell->Biome = EBiomeType::Taiga;
    TestEqual(TEXT("Тайга -- хвойная, не падают"), LeafFall->ComputeLeafFallRate(*Manager, Location), 0.0f);
    Cell->Biome = EBiomeType::BroadleafForest;
    Cell->bIsWater = true;
    TestEqual(TEXT("Над водой -- не падают"), LeafFall->ComputeLeafFallRate(*Manager, Location), 0.0f);
    TestEqual(TEXT("За сеткой -- не падают"), LeafFall->ComputeLeafFallRate(*Manager, FVector(1.0e8, 1.0e8, 0.0)), 0.0f);

    // Без системы частиц компонент не создаётся.
    LeafFall->ApplyRate(Manager, nullptr, 1.0f, 1.0f, 0.1f);
    TestNull(TEXT("Без ассета Niagara -- ничего не спавнится"), LeafFall->GetLeafFallComponent());
    TestFalse(TEXT("Без компонента -- не запрошен"), LeafFall->IsLeafFallRequestedActive());

    Manager->Destroy();
    return true;
}

#endif // WITH_AUTOMATION_TESTS && WITH_EDITOR
