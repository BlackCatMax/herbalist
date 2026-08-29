// Source/ProjectHerbalistTests/Private/Tests/DayCycleTest.cpp
//
// Рассвет/Закат/Полудница (15_Cycles_And_Shrines.md §15.2), 2026-08-29:
// таблица суток раньше давала эффект только Ночи (§16.5, сделано в этой же
// сессии раньше) — Рассвет/Закат/Полдень оставались чисто атмосферными
// (AUDIT_AND_REFACTORING_PLAN.md §7.2, второстепенная находка). Этот файл
// проверяет фазы (`GetTimeOfDay01`/`IsDawn`/`IsDusk`/`GetDuskProgress01`/
// `IsPoludnitsaWindow`) и их эффект на TargetState через
// UpdateEntityManifestations. DispatchBeginPlay-паттерн — тот же, что
// AmbientEntityTest.cpp/SeasonTest.cpp.

#include "Core/World/GridWorldManager.h"
#include "Core/Types/BiomeTypes.h"
#include "Misc/AutomationTest.h"
#include "Editor.h"
#include "Engine/World.h"

#if WITH_AUTOMATION_TESTS && WITH_EDITOR

#include "TestWorldHelpers.h"

namespace
{
    // Дефолт: GameDayMinutes=32мин -> 1920 секунд игровых часов на сутки.
    constexpr float DaySeconds = 32.0f * 60.0f;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistDayCycle_PhaseBoundariesMatchTheSpecTable,
    "Herbalist.DayCycle.PhaseBoundariesMatchTheSpecTable",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistDayCycle_PhaseBoundariesMatchTheSpecTable::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;

    // §15.2: Рассвет 6 / День 14 / Закат 6 / Ночь 6, итого 32 минуты.
    Manager->SetGameClockSeconds(1.0f);   // самое начало Рассвета
    TestTrue(TEXT("t=1s is Dawn"), Manager->IsDawn());
    TestFalse(TEXT("t=1s is not Dusk"), Manager->IsDusk());
    TestFalse(TEXT("t=1s is not Night"), Manager->IsNight());

    Manager->SetGameClockSeconds(10.0f * 60.0f);   // середина Дня (минута 10 из 6-20)
    TestFalse(TEXT("Midday is not Dawn"), Manager->IsDawn());
    TestFalse(TEXT("Midday is not Dusk"), Manager->IsDusk());
    TestFalse(TEXT("Midday is not Night"), Manager->IsNight());

    Manager->SetGameClockSeconds(22.0f * 60.0f);   // минута 22 -- внутри Заката (20-26)
    TestTrue(TEXT("Minute 22 is Dusk"), Manager->IsDusk());
    TestFalse(TEXT("Minute 22 is not Night"), Manager->IsNight());

    Manager->SetGameClockSeconds(29.0f * 60.0f);   // минута 29 -- внутри Ночи (26-32)
    TestTrue(TEXT("Minute 29 is Night"), Manager->IsNight());
    TestFalse(TEXT("Minute 29 is not Dusk"), Manager->IsDusk());

    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistDayCycle_DuskProgressRampsFromZeroToOne,
    "Herbalist.DayCycle.DuskProgressRampsFromZeroToOne",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistDayCycle_DuskProgressRampsFromZeroToOne::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;

    // Закат -- минуты 20 (0.0f) до 26 (=1.0f) при дефолтных 32 мин/сутки.
    Manager->SetGameClockSeconds(20.0f * 60.0f + 1.0f);
    const float ProgressAtStart = Manager->GetDuskProgress01();

    Manager->SetGameClockSeconds(23.0f * 60.0f);
    const float ProgressAtMidpoint = Manager->GetDuskProgress01();

    Manager->SetGameClockSeconds(26.0f * 60.0f - 1.0f);
    const float ProgressNearEnd = Manager->GetDuskProgress01();

    TestTrue(TEXT("Dusk progress starts near zero"), ProgressAtStart < 0.05f);
    TestTrue(TEXT("Dusk progress rises through the middle"), ProgressAtMidpoint > ProgressAtStart && ProgressAtMidpoint < ProgressNearEnd);
    TestTrue(TEXT("Dusk progress ends near one"), ProgressNearEnd > 0.9f);

    // Вне Заката прогресс -- 0 (не "нарастающее навсегда", а именно окно Заката).
    Manager->SetGameClockSeconds(10.0f * 60.0f);
    TestEqual(TEXT("Dusk progress is zero outside Dusk"), Manager->GetDuskProgress01(), 0.0f);

    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistDayCycle_DawnRaisesPurityAndStabilityAcrossTheGrid,
    "Herbalist.DayCycle.DawnRaisesPurityAndStabilityAcrossTheGrid",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistDayCycle_DawnRaisesPurityAndStabilityAcrossTheGrid::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;

    // Тундра -- нет ни одного другого определения на этом биоме (AmbientEntityTypes.h),
    // эффект виден изолированно (тот же приём, что и в AmbientEntityTest.cpp/SeasonTest.cpp).
    FGridCell* Cell = Manager->GetCell(0, 0);
    if (!TestNotNull(TEXT("Cell (0,0) exists"), Cell)) { Manager->Destroy(); return false; }
    Cell->Biome = EBiomeType::Tundra;
    Cell->bIsWater = false;

    Manager->SetGameClockSeconds(1.0f);   // Рассвет
    const float PurityBefore = Cell->TargetState.Meta.Purity;
    const float StabilityBefore = Cell->TargetState.Meta.Stability;
    Manager->UpdateEntityManifestations(1.0f);

    TestTrue(TEXT("TargetState.Purity rises at Dawn"), Cell->TargetState.Meta.Purity > PurityBefore);
    TestTrue(TEXT("TargetState.Stability rises at Dawn"), Cell->TargetState.Meta.Stability > StabilityBefore);

    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistDayCycle_PoludnitsaOnlyStrikesOpenBiomesAtMidday,
    "Herbalist.DayCycle.PoludnitsaOnlyStrikesOpenBiomesAtMidday",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistDayCycle_PoludnitsaOnlyStrikesOpenBiomesAtMidday::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;

    FGridCell* SteppeCell = Manager->GetCell(0, 0);
    FGridCell* TaigaCell  = Manager->GetCell(1, 0);
    if (!TestNotNull(TEXT("Cell (0,0) exists"), SteppeCell) || !TestNotNull(TEXT("Cell (1,0) exists"), TaigaCell))
    {
        Manager->Destroy();
        return false;
    }
    SteppeCell->Biome = EBiomeType::Steppe;
    SteppeCell->bIsWater = false;
    TaigaCell->Biome = EBiomeType::Taiga;   // закрытый биом -- Полудница сюда не бьёт
    TaigaCell->bIsWater = false;

    // Середина Дня (минута 13 из 6-20, дефолтная ширина окна Полудницы 2 мин
    // вокруг середины Дня, т.е. центр отрезка Рассвет-конец..Закат-начало).
    Manager->SetGameClockSeconds(13.0f * 60.0f);
    TestTrue(TEXT("Sanity: t=13min falls inside the Poludnitsa window"), Manager->IsPoludnitsaWindow());

    const float SteppeDistortionBefore = SteppeCell->TargetState.Meta.Distortion;
    const float TaigaDistortionBefore  = TaigaCell->TargetState.Meta.Distortion;
    Manager->UpdateEntityManifestations(1.0f);

    TestTrue(TEXT("Distortion rises in open Steppe at midday"), SteppeCell->TargetState.Meta.Distortion > SteppeDistortionBefore);
    TestEqual(TEXT("Distortion untouched in closed Taiga at midday"), TaigaCell->TargetState.Meta.Distortion, TaigaDistortionBefore);

    Manager->Destroy();
    return true;
}

#endif // WITH_AUTOMATION_TESTS && WITH_EDITOR
