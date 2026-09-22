// Source/ProjectHerbalistTests/Private/Tests/CycleEventsTest.cpp
//
// События кругов времени и погоды менеджера сетки (2026-09-22): смена суток,
// фазы суток, луны, сезона и погодных флагов. Источник -- часы и погода
// симуляции (15_Cycles_And_Shrines_Tech §15.7).

#include "Core/World/GridWorldManager.h"
#include "Core/Config/HerbalistSettings.h"
#include "Misc/AutomationTest.h"
#include "Editor.h"
#include "Engine/World.h"
#include "CycleEventsTestListener.h"

#if WITH_AUTOMATION_TESTS && WITH_EDITOR

#include "TestWorldHelpers.h"

namespace
{
    double CycleTestDaySeconds()
    {
        const UHerbalistSettings* Settings = GetDefault<UHerbalistSettings>();
        return FMath::Max(1.0f, (Settings ? Settings->GameDayMinutes : 32.0f) * 60.0f);
    }

    UCycleEventsTestListener* BindCycleListener(AGridWorldManager* Manager)
    {
        UCycleEventsTestListener* Listener = NewObject<UCycleEventsTestListener>();
        Manager->OnGameDayStarted.AddDynamic(Listener, &UCycleEventsTestListener::HandleGameDayStarted);
        Manager->OnDayPhaseChanged.AddDynamic(Listener, &UCycleEventsTestListener::HandleDayPhaseChanged);
        Manager->OnMoonPhaseChanged.AddDynamic(Listener, &UCycleEventsTestListener::HandleMoonPhaseChanged);
        Manager->OnSeasonChanged.AddDynamic(Listener, &UCycleEventsTestListener::HandleSeasonChanged);
        Manager->OnWeatherChanged.AddDynamic(Listener, &UCycleEventsTestListener::HandleWeatherChanged);
        return Listener;
    }

    // Часы на долю суток Fraction в сутках DayIndex.
    void SetClockAt(AGridWorldManager* Manager, int32 DayIndex, double Fraction)
    {
        Manager->SetGameClockSeconds((DayIndex + Fraction) * CycleTestDaySeconds());
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistCycleEvents_DayPhasesAndNewDay,
    "Herbalist.CycleEvents.DayPhasesAndNewDay",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistCycleEvents_DayPhasesAndNewDay::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;
    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("Manager spawned"), Manager)) return false;
    // Погода не должна вмешиваться: мост держит её постоянной.
    Manager->SetWeatherBridgeIntensities(0.0f, 0.0f, 0.0f);

    UCycleEventsTestListener* Listener = BindCycleListener(Manager);

    SetClockAt(Manager, 0, 0.01);
    TestEqual(TEXT("Clock at dawn"), Manager->GetDayPhase(), EDayPhase::Dawn);
    Manager->UpdateCycleEvents();
    TestTrue(TEXT("First check only remembers the state"), Listener->Order.IsEmpty());

    Manager->UpdateCycleEvents();
    TestTrue(TEXT("Nothing changed -- nothing sent"), Listener->Order.IsEmpty());

    SetClockAt(Manager, 0, 0.5);
    Manager->UpdateCycleEvents();
    TestEqual(TEXT("Dawn -> Day: one phase event"), Listener->Order, FString(TEXT("P")));
    TestTrue(TEXT("Phase is Day"), Listener->DayPhases.Num() == 1 && Listener->DayPhases[0] == EDayPhase::Day);

    SetClockAt(Manager, 0, 0.99);
    TestEqual(TEXT("End of day is night"), Manager->GetDayPhase(), EDayPhase::Night);
    Manager->UpdateCycleEvents();
    TestEqual(TEXT("Day -> Night directly: one event, Dusk skipped"), Listener->Order, FString(TEXT("PP")));
    TestEqual(TEXT("Last phase is Night"), Listener->DayPhases.Last(), EDayPhase::Night);

    SetClockAt(Manager, 1, 0.01);
    Manager->UpdateCycleEvents();
    TestEqual(TEXT("New day, then Dawn"), Listener->Order, FString(TEXT("PPDP")));
    TestTrue(TEXT("Day index 1"), Listener->DayStarts.Num() == 1 && Listener->DayStarts[0] == 1);
    TestEqual(TEXT("Dawn after night"), Listener->DayPhases.Last(), EDayPhase::Dawn);

    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistCycleEvents_ClockJumpSendsOneEventEach,
    "Herbalist.CycleEvents.ClockJumpSendsOneEventEach",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistCycleEvents_ClockJumpSendsOneEventEach::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;
    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("Manager spawned"), Manager)) return false;
    Manager->SetWeatherBridgeIntensities(0.0f, 0.0f, 0.0f);

    UCycleEventsTestListener* Listener = BindCycleListener(Manager);

    SetClockAt(Manager, 0, 0.01);
    const ESeason StartSeason = Manager->GetSeason();
    const EMoonPhase StartMoon = Manager->GetMoonPhase();
    Manager->UpdateCycleEvents();

    // Сутки 100 от 1 марта -- 9 июня, лето (месяцы календаря -- константы).
    // Длина лунной фазы -- настройка, поэтому событие луны ждём, только если
    // фаза за скачок действительно сменилась.
    SetClockAt(Manager, 100, 0.5);
    TestNotEqual(TEXT("Season changed over the jump"), Manager->GetSeason(), StartSeason);
    const bool bMoonChanged = Manager->GetMoonPhase() != StartMoon;
    Manager->UpdateCycleEvents();

    const FString ExpectedOrder = bMoonChanged ? FString(TEXT("DSMP")) : FString(TEXT("DSP"));
    TestEqual(TEXT("One event of each kind, order day-season-moon-phase"), Listener->Order, ExpectedOrder);
    TestTrue(TEXT("Day event carries the final day"), Listener->DayStarts.Num() == 1 && Listener->DayStarts[0] == 100);
    TestTrue(TEXT("Season event carries the final season"), Listener->Seasons.Num() == 1 && Listener->Seasons[0] == Manager->GetSeason());
    TestEqual(TEXT("Moon event only when the phase changed"), Listener->MoonPhases.Num(), bMoonChanged ? 1 : 0);
    if (bMoonChanged)
    {
        TestEqual(TEXT("Moon event carries the final phase"), Listener->MoonPhases[0], Manager->GetMoonPhase());
    }

    // Назад (загрузка старого сейва) -- то же правило: одно событие с итогом.
    SetClockAt(Manager, 0, 0.01);
    Manager->UpdateCycleEvents();
    TestEqual(TEXT("Jump back: day index 0 sent"), Listener->DayStarts.Last(), 0);
    TestEqual(TEXT("Jump back: season restored"), Listener->Seasons.Last(), StartSeason);

    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistCycleEvents_WeatherFlags,
    "Herbalist.CycleEvents.WeatherFlags",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistCycleEvents_WeatherFlags::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;
    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("Manager spawned"), Manager)) return false;

    UCycleEventsTestListener* Listener = BindCycleListener(Manager);
    SetClockAt(Manager, 0, 0.5);
    Manager->SetWeatherBridgeIntensities(0.0f, 0.0f, 0.0f);
    Manager->UpdateCycleEvents();

    // Слабый дождь ниже порога -- флаг не сменился, события нет.
    Manager->SetWeatherBridgeIntensities(0.1f, 0.0f, 0.0f);
    Manager->UpdateCycleEvents();
    TestEqual(TEXT("Below threshold: no weather event"), Listener->WeatherEvents, 0);

    Manager->SetWeatherBridgeIntensities(1.0f, 0.0f, 0.0f);
    Manager->UpdateCycleEvents();
    TestEqual(TEXT("Rain started: one event"), Listener->WeatherEvents, 1);
    TestTrue(TEXT("Event says rainy"), Listener->bLastRainy);

    Manager->UpdateCycleEvents();
    TestEqual(TEXT("Still raining: no repeat"), Listener->WeatherEvents, 1);

    Manager->SetWeatherBridgeIntensities(0.0f, 0.0f, 0.0f);
    Manager->UpdateCycleEvents();
    TestEqual(TEXT("Rain stopped: second event"), Listener->WeatherEvents, 2);
    TestFalse(TEXT("Event says dry"), Listener->bLastRainy);

    Manager->Destroy();
    return true;
}

#endif
