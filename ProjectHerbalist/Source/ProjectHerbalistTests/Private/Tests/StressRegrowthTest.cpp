// Source/ProjectHerbalistTests/Private/Tests/StressRegrowthTest.cpp
//
// Истощённая клетка отращивает дольше (2026-09-12, прямой запрос: "на клетках
// с высоким стрессом растения должны восстанавливаться медленнее").
//
//     T = T_база + HarvestStress x (HarvestStressIncrement x ПолноеЗарастание)
//
// Проверяется через AGridWorldManager::GetRegrowthDelaySeconds -- ту самую
// функцию, из которой StartRegeneration берёт длительность таймера. Сам
// таймер на автотесте не дождаться (минуты игрового времени), см. тот же
// довод в ResourceRegrowthTest.cpp.
//
// Ожидания выводятся из тех же публичных функций и настроек, а не
// захардкожены: тесты фиксируют ФОРМУ зависимости (ноль при нулевом
// стрессе, линейность, наследование биома/сезона/капища), а конкретные
// минуты остаются игровой настройкой.

#include "Core/World/GridWorldManager.h"
#include "Core/Config/HerbalistSettings.h"
#include "Core/Types/BiomeTypes.h"
#include "Core/Types/BiomeRow.h"
#include "Misc/AutomationTest.h"
#include "Editor.h"
#include "Engine/World.h"

#if WITH_AUTOMATION_TESTS && WITH_EDITOR

#include "TestWorldHelpers.h"

namespace
{
    float StressRegrowthDaySeconds()
    {
        return GetDefault<UHerbalistSettings>()->GameDayMinutes * 60.0f;
    }

    // Лето -- единственный сезон с множителем 1.0 (см. GetSeason()), так
    // биомный множитель виден изолированно. +1 с -- твёрдо внутри сезона.
    float SummerClockSeconds()
    {
        return GetDefault<UHerbalistSettings>()->SeasonDurationDays * StressRegrowthDaySeconds() + 1.0f;
    }

    float WinterClockSeconds()
    {
        return GetDefault<UHerbalistSettings>()->SeasonDurationDays * StressRegrowthDaySeconds() * 2.0f + 1.0f;
    }

    // Предсказуемая клетка: известный биом, суша, заданный стресс, без
    // капищ. Капища посевом BeginPlay могут оказаться в радиусе -- их
    // убираем, иначе Лесное рядом тихо укоротило бы ожидание.
    FGridCell* PrepareStressCell(AGridWorldManager* Manager, int32 X, int32 Y, float Stress)
    {
        FGridCell* Cell = Manager->GetCell(X, Y);
        if (!Cell) return nullptr;
        Cell->Biome = EBiomeType::MixedForest;
        Cell->bIsWater = false;
        Cell->HarvestStress = Stress;
        Manager->SetShrines(TArray<FShrine>());
        Manager->SetGameClockSeconds(SummerClockSeconds());
        return Cell;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistStressRegrowth_UntouchedCellRegrowsAtBaseTime,
    "Herbalist.StressRegrowth.UntouchedCellRegrowsAtBaseTime",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistStressRegrowth_UntouchedCellRegrowsAtBaseTime::RunTest(const FString& Parameters)
{
    // Главный контракт варианта A: нетронутая клетка не платит ничего.
    // Базовые 7 минут, выбранные 2026-09-04, для неё не меняются.
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("Manager spawned"), Manager)) return false;

    FGridCell* Cell = PrepareStressCell(Manager, 5, 5, 0.0f);
    if (!TestNotNull(TEXT("Cell exists"), Cell)) { Manager->Destroy(); return false; }

    // SpawnAndBeginPlay убирает регионы уровня -- базой служит глобальное
    // ResourceRegrowthTime.
    TestEqual(TEXT("Стресс 0 -- ровно базовое время, без надбавки"),
        Manager->GetRegrowthDelaySeconds(*Cell), Manager->ResourceRegrowthTime, KINDA_SMALL_NUMBER);

    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistStressRegrowth_ExhaustedCellWaitsForLandToShedOneHarvest,
    "Herbalist.StressRegrowth.ExhaustedCellWaitsForLandToShedOneHarvest",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistStressRegrowth_ExhaustedCellWaitsForLandToShedOneHarvest::RunTest(const FString& Parameters)
{
    // Смысл надбавки одной фразой: полностью истощённая клетка ждёт, пока
    // земля отпустит ровно один сбор. Время на это -- шаг сбора, умноженный
    // на полное зарастание клетки.
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("Manager spawned"), Manager)) return false;

    FGridCell* Cell = PrepareStressCell(Manager, 5, 5, 1.0f);
    if (!TestNotNull(TEXT("Cell exists"), Cell)) { Manager->Destroy(); return false; }

    const float StressStep = GetDefault<UHerbalistSettings>()->HarvestStressIncrement;
    const float TimeToShedOneHarvest = StressStep * Manager->GetStressRecoverySecondsForCell(*Cell);
    const float Delay = Manager->GetRegrowthDelaySeconds(*Cell);

    TestTrue(TEXT("Истощённая клетка отращивает дольше нетронутой"), Delay > Manager->ResourceRegrowthTime);
    TestEqual(TEXT("Надбавка при стрессе 1.0 равна времени, за которое земля отпускает один сбор"),
        Delay - Manager->ResourceRegrowthTime, TimeToShedOneHarvest, 0.01f);

    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistStressRegrowth_SlowdownIsLinearInStress,
    "Herbalist.StressRegrowth.SlowdownIsLinearInStress",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistStressRegrowth_SlowdownIsLinearInStress::RunTest(const FString& Parameters)
{
    // Форма аддитивная и линейная, а не T/(1-стресс): надбавка при 0.5 --
    // ровно половина надбавки при 1.0, и значение при 1.0 конечно. Если
    // форму когда-нибудь поменяют на расходящуюся, этот тест обязан упасть
    // и заставить подтвердить решение осознанно.
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("Manager spawned"), Manager)) return false;

    FGridCell* Cell = PrepareStressCell(Manager, 5, 5, 0.0f);
    if (!TestNotNull(TEXT("Cell exists"), Cell)) { Manager->Destroy(); return false; }

    const float Base = Manager->GetRegrowthDelaySeconds(*Cell);
    Cell->HarvestStress = 0.5f;
    const float Half = Manager->GetRegrowthDelaySeconds(*Cell);
    Cell->HarvestStress = 1.0f;
    const float Full = Manager->GetRegrowthDelaySeconds(*Cell);

    TestTrue(TEXT("Ожидание растёт со стрессом: 0 < 0.5 < 1.0"), Base < Half && Half < Full);
    TestEqual(TEXT("Надбавка при 0.5 -- ровно половина надбавки при 1.0"),
        Half - Base, (Full - Base) * 0.5f, 0.01f);
    TestTrue(TEXT("Ожидание при стрессе 1.0 конечно"), FMath::IsFinite(Full));

    // Стресс вне [0,1] (не бывает штатно, но поле публичное) не разгоняет
    // надбавку за потолок.
    Cell->HarvestStress = 3.0f;
    TestEqual(TEXT("Стресс сверх 1.0 клампится к потолку"),
        Manager->GetRegrowthDelaySeconds(*Cell), Full, 0.01f);

    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistStressRegrowth_WinterWaitsLongerThanSummer,
    "Herbalist.StressRegrowth.WinterWaitsLongerThanSummer",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistStressRegrowth_WinterWaitsLongerThanSummer::RunTest(const FString& Parameters)
{
    // Надбавка читает то же полное зарастание, что спад HarvestStress, --
    // значит "Зима: клетки заживают медленнее" (§15.4) доходит и до
    // отрастания без отдельной настройки. Базу сезон при этом не трогает:
    // нетронутая клетка зимой отращивает за те же базовые минуты.
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("Manager spawned"), Manager)) return false;

    FGridCell* Cell = PrepareStressCell(Manager, 5, 5, 1.0f);
    if (!TestNotNull(TEXT("Cell exists"), Cell)) { Manager->Destroy(); return false; }

    const float SummerDelay = Manager->GetRegrowthDelaySeconds(*Cell);
    Manager->SetGameClockSeconds(WinterClockSeconds());
    const float WinterDelay = Manager->GetRegrowthDelaySeconds(*Cell);

    TestTrue(TEXT("Истощённая клетка зимой ждёт дольше, чем летом"), WinterDelay > SummerDelay);

    Cell->HarvestStress = 0.0f;
    TestEqual(TEXT("Нетронутая клетка зимой -- то же базовое время"),
        Manager->GetRegrowthDelaySeconds(*Cell), Manager->ResourceRegrowthTime, KINDA_SMALL_NUMBER);

    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistStressRegrowth_ForestShrineShortensTheWait,
    "Herbalist.StressRegrowth.ForestShrineShortensTheWait",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistStressRegrowth_ForestShrineShortensTheWait::RunTest(const FString& Parameters)
{
    // Лесное капище "ускоряет заживление клеток" (§15.5) -- через общую
    // функцию оно ускоряет и отрастание, делением полного зарастания на
    // (1 + ShrineForestHealBonus x Restoration).
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("Manager spawned"), Manager)) return false;

    FGridCell* Cell = PrepareStressCell(Manager, 5, 5, 1.0f);
    if (!TestNotNull(TEXT("Cell exists"), Cell)) { Manager->Destroy(); return false; }

    const float WithoutShrine = Manager->GetRegrowthDelaySeconds(*Cell);

    Manager->RegisterShrine(FIntPoint(5, 5), EShrineType::Forest, 1.0f);
    const float WithShrine = Manager->GetRegrowthDelaySeconds(*Cell);

    TestTrue(TEXT("Восстановленное Лесное капище укорачивает ожидание"), WithShrine < WithoutShrine);

    const UHerbalistSettings* Settings = GetDefault<UHerbalistSettings>();
    const float ExpectedRecovery = Manager->GetStressRecoverySecondsForBiome(Cell->Biome)
        / (1.0f + Settings->ShrineForestHealBonus * 1.0f);
    TestEqual(TEXT("Надбавка считается от зарастания, делённого на (1 + HealBonus x Restoration)"),
        WithShrine - Manager->ResourceRegrowthTime, Settings->HarvestStressIncrement * ExpectedRecovery, 0.01f);

    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistStressRegrowth_BiomeRecoveryMatchesDocumentedFormula,
    "Herbalist.StressRegrowth.BiomeRecoveryMatchesDocumentedFormula",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistStressRegrowth_BiomeRecoveryMatchesDocumentedFormula::RunTest(const FString& Parameters)
{
    // Страховка выноса лямбды из RegenerateCellParameters в
    // GetStressRecoverySecondsForBiome: летом (сезон 1.0) функция обязана
    // давать ровно StressRecoveryGameDays x сутки x множитель биома -- ту
    // формулу, что стояла в лямбде до 2026-09-12.
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("Manager spawned"), Manager)) return false;

    Manager->SetGameClockSeconds(SummerClockSeconds());

    const FBiomeRow* Row = FBiomeDefaults::GetBiomeRow(EBiomeType::MixedForest);
    if (!TestNotNull(TEXT("Строка биома MixedForest есть"), Row)) { Manager->Destroy(); return false; }

    const float Expected = GetDefault<UHerbalistSettings>()->StressRecoveryGameDays * StressRegrowthDaySeconds()
        * FMath::Max(Row->StressRecoveryMultiplier, 0.05f);
    TestEqual(TEXT("Летом зарастание = дни x сутки x множитель биома"),
        Manager->GetStressRecoverySecondsForBiome(EBiomeType::MixedForest), Expected, 0.5f);

    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistStressRegrowth_DecayAndRegrowthReadTheSameClock,
    "Herbalist.StressRegrowth.DecayAndRegrowthReadTheSameClock",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistStressRegrowth_DecayAndRegrowthReadTheSameClock::RunTest(const FString& Parameters)
{
    // Ради этого функция и вынесена: спад HarvestStress в
    // RegenerateCellParameters и надбавка к отрастанию обязаны читать ОДНО
    // число. Проверяем спад против GetStressRecoverySecondsForCell -- если
    // кто-то снова заведёт в релаксации собственное выражение и поправит
    // только его, тест упадёт.
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("Manager spawned"), Manager)) return false;

    // (0,0) -- та же клетка, на которой SeasonTest уже проверяет спад через
    // RegenerateCellParameters.
    FGridCell* Cell = PrepareStressCell(Manager, 0, 0, 1.0f);
    if (!TestNotNull(TEXT("Cell exists"), Cell)) { Manager->Destroy(); return false; }

    const float RecoverySeconds = Manager->GetStressRecoverySecondsForCell(*Cell);
    const float DeltaTime = 600.0f;   // заведомо меньше полного зарастания -- спад не упрётся в ноль

    Manager->RegenerateCellParameters(DeltaTime);

    TestEqual(TEXT("Спад за шаг = шаг / полное зарастание той же клетки"),
        1.0f - Cell->HarvestStress, DeltaTime / RecoverySeconds, 0.0001f);

    Manager->Destroy();
    return true;
}

#endif // WITH_AUTOMATION_TESTS && WITH_EDITOR
