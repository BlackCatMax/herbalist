// Source/ProjectHerbalistTests/Private/Tests/GridCorruptionAutoReportTest.cpp
//
// Автоматический периодический ReportGridCorruption (2026-09-06, прямой
// запрос пользователя: "напиши функцию, которая будет автоматом раз в
// полминуты снимать стату со всех клеток... мне останется только
// инициировать пару сборов и ждать"). Сам GetGridCorruptionReport() уже
// покрыт ContagionSpreadTest.cpp -- здесь проверяется только планирование/
// остановка таймера (BeginPlay/EndPlay), не форматирование строки.

#include "Core/World/GridWorldManager.h"
#include "Misc/AutomationTest.h"
#include "Editor.h"
#include "Engine/World.h"

#if WITH_AUTOMATION_TESTS && WITH_EDITOR

#include "TestWorldHelpers.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistGridCorruptionAutoReport_ScheduledByDefaultOnBeginPlay,
    "Herbalist.GridCorruptionAutoReport.ScheduledByDefaultOnBeginPlay",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistGridCorruptionAutoReport_ScheduledByDefaultOnBeginPlay::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;

    TestEqual(TEXT("Default interval is 30 seconds"), Manager->GridCorruptionReportIntervalSeconds, 30.0f);
    TestTrue(TEXT("Auto-report timer is scheduled after BeginPlay with a positive interval"),
        Manager->IsGridCorruptionAutoReportScheduled());

    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistGridCorruptionAutoReport_ZeroIntervalDisablesIt,
    "Herbalist.GridCorruptionAutoReport.ZeroIntervalDisablesIt",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistGridCorruptionAutoReport_ZeroIntervalDisablesIt::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    // Ровно тот же приём очистки предыдущих менеджеров, что и в
    // SpawnAndBeginPlay -- здесь нужен ручной спавн (без BeginPlay), чтобы
    // выставить интервал ДО его прочтения в BeginPlay.
    for (TActorIterator<AGridWorldManager> It(World); It; ++It)
    {
        if (AGridWorldManager* Stale = *It) Stale->Destroy();
    }

    AGridWorldManager* Manager = World->SpawnActor<AGridWorldManager>();
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;

    Manager->GridCorruptionReportIntervalSeconds = 0.0f;
    Manager->DispatchBeginPlay();

    TestFalse(TEXT("Zero interval means no auto-report timer is scheduled"),
        Manager->IsGridCorruptionAutoReportScheduled());

    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistGridCorruptionAutoReport_EndPlayStopsTheTimer,
    "Herbalist.GridCorruptionAutoReport.EndPlayStopsTheTimer",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistGridCorruptionAutoReport_EndPlayStopsTheTimer::RunTest(const FString& Parameters)
{
    // Регрессия ровно на риск, названный в комментарии у
    // GridCorruptionReportTimerHandle (GridWorldManager.h): без остановки в
    // EndPlay таймер на уничтоженном акторе мог бы выстрелить в persistent
    // editor-мире между следующими автотестами того же прогона.
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;

    if (!TestTrue(TEXT("Precondition: timer is scheduled right after BeginPlay"),
        Manager->IsGridCorruptionAutoReportScheduled()))
    {
        Manager->Destroy();
        return false;
    }

    Manager->EndPlay(EEndPlayReason::Destroyed);

    TestFalse(TEXT("EndPlay clears the auto-report timer"),
        Manager->IsGridCorruptionAutoReportScheduled());

    Manager->Destroy();
    return true;
}

#endif // WITH_AUTOMATION_TESTS && WITH_EDITOR
