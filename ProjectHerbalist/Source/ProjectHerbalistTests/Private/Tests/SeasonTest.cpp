// Source/ProjectHerbalistTests/Private/Tests/SeasonTest.cpp
//
// Годовой круг (15_Cycles_And_Shrines.md §15.4), v1: сезон считается из
// GameClockSeconds (тот же принцип, что фаза суток/луны), эффекты — Весна
// ускоряет спад HarvestStress (StressRecoveryMultiplier), Зима замедляет
// его и поднимает Purity по всей сетке ("снег как чистота"). Лето
// намеренно нейтрально (см. комментарий у GetSeason()).
// DispatchBeginPlay-паттерн — тот же, что BistabilityTest.cpp/MoonPhaseTest.cpp.

#include "Core/World/GridWorldManager.h"
#include "Core/Types/BiomeTypes.h"
#include "Misc/AutomationTest.h"
#include "Editor.h"
#include "Engine/World.h"

#if WITH_AUTOMATION_TESTS && WITH_EDITOR

namespace
{
    AGridWorldManager* SpawnAndBeginPlay(UWorld* World)
    {
        if (!World) return nullptr;
        AGridWorldManager* Manager = World->SpawnActor<AGridWorldManager>();
        if (Manager)
        {
            Manager->DispatchBeginPlay();
        }
        return Manager;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistSeason_CyclesThroughAllThreeSeasonsOverOneYear,
    "Herbalist.Season.CyclesThroughAllThreeSeasonsOverOneYear",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistSeason_CyclesThroughAllThreeSeasonsOverOneYear::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;

    // Дефолт: GameDayMinutes=32мин, SeasonDurationDays=117 -> сезон = 117 суток.
    const float DayLengthSeconds = 32.0f * 60.0f;
    const float SeasonDurationSeconds = 117.0f * DayLengthSeconds;

    Manager->SetGameClockSeconds(0.0f);
    TestEqual(TEXT("Day 0 is Spring"), Manager->GetSeason(), ESeason::Spring);

    Manager->SetGameClockSeconds(SeasonDurationSeconds + 1.0f);
    TestEqual(TEXT("Day 117 is Summer"), Manager->GetSeason(), ESeason::Summer);

    Manager->SetGameClockSeconds(SeasonDurationSeconds * 2.0f + 1.0f);
    TestEqual(TEXT("Day 234 is Winter"), Manager->GetSeason(), ESeason::Winter);

    Manager->SetGameClockSeconds(SeasonDurationSeconds * 3.0f + 1.0f);
    TestEqual(TEXT("Day 351 wraps back to Spring"), Manager->GetSeason(), ESeason::Spring);

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

    const float DayLengthSeconds = 32.0f * 60.0f;
    const float SeasonDurationSeconds = 117.0f * DayLengthSeconds;

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

    const float SpringStress = MeasureStressAfterOneTick(0.0f);
    const float SummerStress = MeasureStressAfterOneTick(SeasonDurationSeconds + 1.0f);
    const float WinterStress = MeasureStressAfterOneTick(SeasonDurationSeconds * 2.0f + 1.0f);

    TestTrue(TEXT("Spring recovers HarvestStress faster than Summer (lower remaining stress)"), SpringStress < SummerStress);
    TestTrue(TEXT("Winter recovers HarvestStress slower than Summer (higher remaining stress)"), WinterStress > SummerStress);

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

    const float DayLengthSeconds = 32.0f * 60.0f;
    const float SeasonDurationSeconds = 117.0f * DayLengthSeconds;

    // +10 минут игровых суток на оба замера -- твёрдо внутри фазы "День"
    // (6-20 минут при дефолтных 32 мин/сутки), не Рассвет/Закат: с
    // 2026-08-29 у них тоже есть эффект на Purity (§15.2, DayCycleTest.cpp),
    // и t=0/t=SeasonDuration*2+1 совпадали с Рассветом чисто случайно, что
    // и сломало "нет изменения в Весну" ниже, когда Рассвет получил эффект.
    const float MidDaySeconds = 10.0f * 60.0f;

    Manager->SetGameClockSeconds(MidDaySeconds);   // Весна, середина Дня
    const float PurityBeforeSpring = Cell->TargetState.Meta.Purity;
    Manager->UpdateEntityManifestations(1.0f);
    TestEqual(TEXT("No Purity change in Spring on a biome with no other definition"),
        Cell->TargetState.Meta.Purity, PurityBeforeSpring);

    Manager->SetGameClockSeconds(SeasonDurationSeconds * 2.0f + MidDaySeconds);   // Зима, середина Дня
    const float PurityBeforeWinter = Cell->TargetState.Meta.Purity;
    Manager->UpdateEntityManifestations(1.0f);
    TestTrue(TEXT("Purity rises in Winter ('снег как чистота')"), Cell->TargetState.Meta.Purity > PurityBeforeWinter);

    Manager->Destroy();
    return true;
}

#endif // WITH_AUTOMATION_TESTS && WITH_EDITOR
