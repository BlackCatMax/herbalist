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

    // Метель: ветер И снег И Зима одновременно -- ищем внутри окна Зимы
    // (3-й сезон, [2/3, 1) года; SeasonDurationDays=117 по умолчанию).
    const float DayLength = 32.0f * 60.0f;
    const float YearLength = 117.0f * 3.0f * DayLength;
    const float WinterStart = YearLength * 2.0f / 3.0f;

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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistAmbientEntity_ListovikiOnlyManifestLateSummerNotEarly,
    "Herbalist.AmbientEntity.ListovikiOnlyManifestLateSummerNotEarly",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistAmbientEntity_ListovikiOnlyManifestLateSummerNotEarly::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("Manager spawned"), Manager)) return false;

    FGridCell* Cell = Manager->GetCell(0, 0);
    Cell->Biome = EBiomeType::MixedForest;
    Cell->bIsWater = false;

    const float DayLength = 32.0f * 60.0f;
    const float YearLength = 117.0f * 3.0f * DayLength;
    const float SummerStart = YearLength / 3.0f;
    const float SummerEnd = YearLength * 2.0f / 3.0f;

    // Начало Лета -- НЕ поздний конец, Листовики не должны проявиться.
    Manager->SetGameClockSeconds(SummerStart + DayLength * 5.0f);
    TestFalse(TEXT("Early summer is not late summer"), Manager->IsLateSummer());
    Manager->UpdateEntityManifestations(1.0f);
    TestNotEqual(TEXT("Листовики do not manifest in early summer"), Cell->ManifestedEntityID, FName(TEXT("Листовики")));

    // Самый конец Лета -- должны проявиться.
    Manager->SetGameClockSeconds(SummerEnd - DayLength * 2.0f);
    TestTrue(TEXT("Just before season end is late summer"), Manager->IsLateSummer());
    const float NatureBefore = Cell->TargetState.Direction.Nature;
    Manager->UpdateEntityManifestations(1.0f);
    TestEqual(TEXT("Листовики manifest in late summer"), Cell->ManifestedEntityID, FName(TEXT("Листовики")));
    TestTrue(TEXT("Листовики nudge Direction.Nature up"), Cell->TargetState.Direction.Nature > NatureBefore);

    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistAmbientEntity_KupalskyeOnlyManifestOnKupalaNight,
    "Herbalist.AmbientEntity.KupalskyeOnlyManifestOnKupalaNight",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistAmbientEntity_KupalskyeOnlyManifestOnKupalaNight::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("Manager spawned"), Manager)) return false;

    FGridCell* Cell = Manager->GetCell(0, 0);
    Cell->Biome = EBiomeType::MixedForest;
    Cell->bIsWater = false;

    const float DayLength = 32.0f * 60.0f;
    const float YearLength = 117.0f * 3.0f * DayLength;
    const float SummerStart = YearLength / 3.0f;
    const float SummerEnd = YearLength * 2.0f / 3.0f;

    // Ночь в разгар Лета, но не в купальском окне (середина Лета) -- НЕ Купала.
    Manager->SetGameClockSeconds(SummerStart + (SummerEnd - SummerStart) * 0.5f + DayLength * 0.9f);
    TestFalse(TEXT("Midsummer night outside the Kupala window is not Kupala night"), Manager->IsKupalaNight());

    // Ищем настоящую купальскую ночь внутри окна [SummerStart+0.15*SeasonLen,
    // SummerStart+0.18*SeasonLen) -- перебором по времени суток, а не
    // угадыванием конкретного часа.
    const float SeasonLen = SummerEnd - SummerStart;
    const float WindowStart = SummerStart + SeasonLen * 0.15f;
    const float WindowEnd = SummerStart + SeasonLen * 0.18f;

    const float KupalaTime = FindMoment(Manager, WindowStart, WindowEnd, 60.0f, [Manager]() { return Manager->IsKupalaNight(); });
    if (!TestTrue(TEXT("Found an actual Kupala night within the window"), KupalaTime >= 0.0f))
    {
        Manager->Destroy();
        return false;
    }

    Manager->SetGameClockSeconds(KupalaTime);
    const float ResonanceBefore = Cell->TargetState.Meta.Resonance;
    Manager->UpdateEntityManifestations(1.0f);
    TestEqual(TEXT("Купальские manifest on Kupala night"), Cell->ManifestedEntityID, FName(TEXT("Купальские")));
    TestTrue(TEXT("Купальские nudge Resonance up"), Cell->TargetState.Meta.Resonance > ResonanceBefore);

    Manager->Destroy();
    return true;
}

#endif // WITH_AUTOMATION_TESTS && WITH_EDITOR
