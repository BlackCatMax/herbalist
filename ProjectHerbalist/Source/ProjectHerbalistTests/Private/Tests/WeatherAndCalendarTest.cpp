// Source/ProjectHerbalistTests/Private/Tests/WeatherAndCalendarTest.cpp
//
// Собственный C++-сигнал погоды (§15.7) и окна внутри сезона (Листовики/
// Купальские) — 2026-08-29, по прямому решению пользователя ("возьмёмся
// за погоду/календарь как отдельные дизайн-решения"). Погода детерминирована
// (value-noise от GameClockSeconds+RngBaseSeed, без сохраняемого состояния),
// поэтому тесты ищут конкретный момент перебором, а не подгадывают магическое
// число заранее -- надёжнее и честнее показывает, что механизм работает как
// систему, а не что один волшебный таймкод случайно попадает в порог.

#include "Core/World/GridWorldManager.h"
#include "Core/Entities/AmbientEntityTypes.h"
#include "Core/Types/HerbalistCalendar.h"
#include "Core/Types/BiomeTypes.h"
#include "Misc/AutomationTest.h"
#include "Editor.h"
#include "Engine/World.h"

#if WITH_AUTOMATION_TESTS && WITH_EDITOR

#include "TestWorldHelpers.h"

namespace
{
    // Возвращает первый GameClockSeconds из [Start, End) с шагом Step, где
    // Predicate(Manager) истинен, либо -1 если не нашлось.
    template<typename TPredicate>
    float FindMoment(AGridWorldManager* Manager, float Start, float End, float Step, TPredicate Predicate)
    {
        for (float T = Start; T < End; T += Step)
        {
            Manager->SetGameClockSeconds(T);
            if (Predicate())
            {
                return T;
            }
        }
        return -1.0f;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistWeather_WindIntensityIsDeterministicAndSeedDependent,
    "Herbalist.Weather.WindIntensityIsDeterministicAndSeedDependent",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistWeather_WindIntensityIsDeterministicAndSeedDependent::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("Manager spawned"), Manager)) return false;

    Manager->SetGameClockSeconds(12345.0f);
    const float FirstRead = Manager->GetWindIntensity();
    const float SecondRead = Manager->GetWindIntensity();
    TestEqual(TEXT("Same GameClockSeconds -- same wind intensity (no hidden state)"), FirstRead, SecondRead);
    TestTrue(TEXT("Wind intensity is in [0,1]"), FirstRead >= 0.0f && FirstRead <= 1.0f);

    Manager->RngBaseSeed = 999;
    const float DifferentSeedRead = Manager->GetWindIntensity();
    // Не гарантированно различны на 100% (два хэша теоретически могут
    // совпасть), но на практике для разных сидов почти всегда различны --
    // проверяем, что смена сида вообще что-то меняет, не заявляем точных чисел.
    TestNotEqual(TEXT("Different RngBaseSeed changes wind intensity"), DifferentSeedRead, FirstRead);

    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistWeather_SnowOnlyPossibleInWinter,
    "Herbalist.Weather.SnowOnlyPossibleInWinter",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistWeather_SnowOnlyPossibleInWinter::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("Manager spawned"), Manager)) return false;

    // День 10 (Spring, начало года) -- снега быть не может вовсе.
    Manager->SetGameClockSeconds(10.0f * 60.0f);
    TestEqual(TEXT("Season is Spring"), Manager->GetSeason(), ESeason::Spring);
    TestEqual(TEXT("Snow intensity is exactly 0 outside Winter"), Manager->GetSnowIntensity(), 0.0f);
    TestFalse(TEXT("Blizzard impossible outside Winter"), Manager->IsBlizzard());

    Manager->Destroy();
    return true;
}

// Мост от Ultra Dynamic Weather (02_GDD/15_Cycles_And_Shrines.md §15.7,
// "Мост в C++") -- 2026-09-04, плагин физически появился в проекте
// (Content/UltraDynamicSky). SetWeatherBridgeIntensities -- единственная
// точка входа для будущего Blueprint-моста; сам мост (Tick/событие UDW,
// читающее реальные Get Wind Intensity()/... и зовущее эту функцию) --
// Blueprint-код, недостижимый из C++-автотеста, поэтому тест звонит сюда
// напрямую с теми же числами, что позвал бы мост -- ровно то, что и
// требуется проверить со стороны C++: активный мост НЕ пересчитывает шум и
// возвращает точно то, что в него положили.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistWeather_BridgeActiveOverridesNoisePlaceholder,
    "Herbalist.Weather.BridgeActiveOverridesNoisePlaceholder",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistWeather_BridgeActiveOverridesNoisePlaceholder::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("Manager spawned"), Manager)) return false;

    TestFalse(TEXT("Мост неактивен по умолчанию (уровень без UDW, как у всех автотестов)"),
        Manager->bWeatherBridgeActive);

    // День 10 (Spring) -- вне моста снег был бы жёстко 0 (см.
    // SnowOnlyPossibleInWinter выше). Мост -- источник истины без такой
    // подмены: реальный UDW может решить иначе (своя дата/полушарие), и
    // C++ ему не перечит.
    Manager->SetGameClockSeconds(10.0f * 60.0f);
    TestEqual(TEXT("Season is Spring"), Manager->GetSeason(), ESeason::Spring);

    Manager->SetWeatherBridgeIntensities(/*Rain=*/0.7f, /*Snow=*/0.9f, /*Wind=*/0.8f, /*Fog=*/0.3f);

    TestTrue(TEXT("Мост теперь активен"), Manager->bWeatherBridgeActive);
    TestEqual(TEXT("GetRainIntensity читает кэш, не шум"), Manager->GetRainIntensity(), 0.7f);
    TestEqual(TEXT("GetSnowIntensity читает кэш даже Весной -- мост, не сезонное правило шума"),
        Manager->GetSnowIntensity(), 0.9f);
    TestEqual(TEXT("GetWindIntensity читает кэш"), Manager->GetWindIntensity(), 0.8f);
    TestEqual(TEXT("CachedFogIntensity сохранён как есть (пока без гейта-потребителя)"),
        Manager->CachedFogIntensity, 0.3f);

    TestTrue(TEXT("IsRainy реагирует на кэш через тот же порог RainyThreshold"), Manager->IsRainy());
    TestTrue(TEXT("IsWindy реагирует на кэш через тот же порог WindyThreshold"), Manager->IsWindy());
    TestTrue(TEXT("IsBlizzard реагирует на кэш (ветер И снег выше порогов)"), Manager->IsBlizzard());

    // Клампинг на входе -- Get*Intensity везде подряд предполагают [0,1].
    Manager->SetWeatherBridgeIntensities(/*Rain=*/1.5f, /*Snow=*/-0.2f, /*Wind=*/0.5f);
    TestEqual(TEXT("Значение выше 1 клампится"), Manager->GetRainIntensity(), 1.0f);
    TestEqual(TEXT("Отрицательное значение клампится к 0"), Manager->GetSnowIntensity(), 0.0f);

    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistAmbientEntity_WeatherGatedEntitiesManifestWhenWindyOrBlizzard,
    "Herbalist.AmbientEntity.WeatherGatedEntitiesManifestWhenWindyOrBlizzard",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistAmbientEntity_WeatherGatedEntitiesManifestWhenWindyOrBlizzard::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("Manager spawned"), Manager)) return false;

    FGridCell* SteppeCell = Manager->GetCell(0, 0);
    SteppeCell->Biome = EBiomeType::Steppe;
    SteppeCell->bIsWater = false;

    const float WindyTime = FindMoment(Manager, 0.0f, 100000.0f, 200.0f, [Manager]() { return Manager->IsWindy(); });
    if (!TestTrue(TEXT("Found a windy moment within the search window"), WindyTime >= 0.0f))
    {
        Manager->Destroy();
        return false;
    }

    Manager->SetGameClockSeconds(WindyTime);
    Manager->UpdateEntityManifestations(1.0f);
    TestEqual(TEXT("Вихри manifest when windy"), SteppeCell->ManifestedEntityID, FName(TEXT("Вихри")));

    // Метель: ветер И снег И Зима одновременно -- ищем внутри Зимы
    // (декабрь–февраль, конец года календаря от 1 марта).
    const float DayLength = 32.0f * 60.0f;
    const float YearLength = HerbalistCore::Calendar::DaysPerYear * DayLength;
    const float WinterStart = HerbalistCore::Calendar::DayOfYearFromDate(12, 1) * DayLength;

    FGridCell* TundraCell = Manager->GetCell(1, 0);
    TundraCell->Biome = EBiomeType::Tundra;
    TundraCell->bIsWater = false;

    const float BlizzardTime = FindMoment(Manager, WinterStart, YearLength, 300.0f, [Manager]() { return Manager->IsBlizzard(); });
    if (!TestTrue(TEXT("Found a blizzard moment within Winter"), BlizzardTime >= 0.0f))
    {
        Manager->Destroy();
        return false;
    }

    Manager->SetGameClockSeconds(BlizzardTime);
    Manager->UpdateEntityManifestations(1.0f);
    TestEqual(TEXT("Метельники manifest during a blizzard"), TundraCell->ManifestedEntityID, FName(TEXT("Метельники")));

    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistAmbientEntity_ListovikiManifestAllAutumnNotSummer,
    "Herbalist.AmbientEntity.ListovikiManifestAllAutumnNotSummer",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistAmbientEntity_ListovikiManifestAllAutumnNotSummer::RunTest(const FString& Parameters)
{
    // Листовики -- вся осень, сентябрь–ноябрь (решение пользователя
    // 2026-09-16); до календаря -- последние 20% трёхсезонного Лета.
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("Manager spawned"), Manager)) return false;

    FGridCell* Cell = Manager->GetCell(0, 0);
    Cell->Biome = EBiomeType::MixedForest;
    Cell->bIsWater = false;

    const float DayLength = 32.0f * 60.0f;
    const float MidDay = 10.0f * 60.0f;
    auto AtDate = [DayLength, MidDay](int32 Month, int32 Day)
    {
        return HerbalistCore::Calendar::DayOfYearFromDate(Month, Day) * DayLength + MidDay;
    };

    // Конец августа -- ещё лето, Листовиков нет.
    Manager->SetGameClockSeconds(AtDate(8, 31));
    TestFalse(TEXT("31 августа -- не осень"), Manager->IsAutumn());
    Manager->UpdateEntityManifestations(1.0f);
    TestNotEqual(TEXT("Листовики не проявляются летом"), Cell->ManifestedEntityID, FName(TEXT("Листовики")));

    // Начало, середина и конец осени -- проявляются.
    for (const FIntPoint& MonthDay : { FIntPoint(9, 1), FIntPoint(10, 15), FIntPoint(11, 30) })
    {
        Manager->SetGameClockSeconds(AtDate(MonthDay.X, MonthDay.Y));
        TestTrue(*FString::Printf(TEXT("%d.%02d -- осень"), MonthDay.Y, MonthDay.X), Manager->IsAutumn());
        const float NatureBefore = Cell->TargetState.Direction.Nature;
        Manager->UpdateEntityManifestations(1.0f);
        TestEqual(*FString::Printf(TEXT("Листовики проявляются %d.%02d"), MonthDay.Y, MonthDay.X), Cell->ManifestedEntityID, FName(TEXT("Листовики")));
        TestTrue(TEXT("Листовики подталкивают Direction.Nature вверх"), Cell->TargetState.Direction.Nature > NatureBefore);
    }

    // Первое декабря -- зима, осень кончилась.
    Manager->SetGameClockSeconds(AtDate(12, 1));
    TestFalse(TEXT("1 декабря -- не осень"), Manager->IsAutumn());

    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistAmbientEntity_KupalskyeOnlyManifestOnKupalaNight,
    "Herbalist.AmbientEntity.KupalskyeOnlyManifestOnKupalaNight",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistAmbientEntity_KupalskyeOnlyManifestOnKupalaNight::RunTest(const FString& Parameters)
{
    // Купальская ночь -- ночь на 24 июня (старый стиль, решение пользователя
    // 2026-09-16): ночная фаза суток 23 июня.
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("Manager spawned"), Manager)) return false;

    FGridCell* Cell = Manager->GetCell(0, 0);
    Cell->Biome = EBiomeType::MixedForest;
    Cell->bIsWater = false;

    // Ночь -- последние 6 минут 32-минутных суток; +3 минуты -- середина ночи.
    const float DayLength = 32.0f * 60.0f;
    const float MidNight = (32.0f - 3.0f) * 60.0f;
    auto NightOf = [DayLength, MidNight](int32 Month, int32 Day)
    {
        return HerbalistCore::Calendar::DayOfYearFromDate(Month, Day) * DayLength + MidNight;
    };

    Manager->SetGameClockSeconds(NightOf(6, 22));
    TestTrue(TEXT("Sanity: ночь 22 июня -- ночь"), Manager->IsNight());
    TestFalse(TEXT("Ночь на 23 июня -- не Купальская"), Manager->IsKupalaNight());

    Manager->SetGameClockSeconds(NightOf(6, 24));
    TestFalse(TEXT("Ночь на 25 июня -- не Купальская"), Manager->IsKupalaNight());

    Manager->SetGameClockSeconds(HerbalistCore::Calendar::DayOfYearFromDate(6, 23) * DayLength + 10.0f * 60.0f);
    TestFalse(TEXT("День 23 июня -- ещё не ночь"), Manager->IsKupalaNight());

    Manager->SetGameClockSeconds(NightOf(6, 23));
    TestTrue(TEXT("Ночь на 24 июня -- Купальская"), Manager->IsKupalaNight());
    const float ResonanceBefore = Cell->TargetState.Meta.Resonance;
    Manager->UpdateEntityManifestations(1.0f);
    TestEqual(TEXT("Купальские manifest on Kupala night"), Cell->ManifestedEntityID, FName(TEXT("Купальские")));
    TestTrue(TEXT("Купальские nudge Resonance up"), Cell->TargetState.Meta.Resonance > ResonanceBefore);

    // Следующий год -- та же ночь.
    Manager->SetGameClockSeconds(NightOf(6, 23) + HerbalistCore::Calendar::DaysPerYear * DayLength);
    TestTrue(TEXT("Через год -- снова Купальская ночь"), Manager->IsKupalaNight());

    Manager->Destroy();
    return true;
}

#endif // WITH_AUTOMATION_TESTS && WITH_EDITOR
