// Source/ProjectHerbalistTests/Private/Tests/NightHorrorFacesTest.cpp
//
// Пять ликов Ночной нечисти (§16.5, решение пользователя 2026-09-20,
// вариант Б: «пять ликов вместо одного»). Раньше ночь давила одинаково по
// всей сетке: +Искажение и +Порча каждой клетке каждую ночь. Теперь у
// каждого лика своё условие и своя ось:
//
//   Вурдалаки  -- ночь в Степи/Лесостепи на искажённой земле -> Порча
//   Лихоманки  -- осенняя ночь в Болоте/Пойме                -> Тело вниз
//   Навьи      -- ночь новолуния, вся сетка                  -> Искажение
//   Оборотни   -- ночь полнолуния в лесах                    -> Стабильность вниз
//   Черти      -- ночь на воде Болота/Поймы                  -> Порча
//
// Ночной подъём Духа (§15.2) к нечисти не относится и остаётся свойством
// самой ночи -- он проверяется отдельно, в DayCycleTest.

#include "Core/World/GridWorldManager.h"
#include "Core/Types/BiomeTypes.h"
#include "Core/Types/HerbalistCalendar.h"
#include "Misc/AutomationTest.h"
#include "Editor.h"
#include "Engine/World.h"

#if WITH_AUTOMATION_TESTS && WITH_EDITOR

#include "TestWorldHelpers.h"

namespace
{
    constexpr float FaceDayLengthSeconds = 32.0f * 60.0f;

    // Ночь выбранного дня года. Фазу луны подбирает вызывающий, перебирая
    // сутки: лунный месяц 28 суток.
    float NightOf(int32 Month, int32 Day)
    {
        return HerbalistCore::Calendar::DayOfYearFromDate(Month, Day) * FaceDayLengthSeconds + 29.0f * 60.0f;
    }

    // Первая ночь нужной фазы начиная с этой даты (в пределах лунного месяца).
    float FindNightWithPhase(AGridWorldManager* Manager, int32 Month, int32 Day, EMoonPhase Phase, bool& bOutFound)
    {
        for (int32 Offset = 0; Offset < 28; ++Offset)
        {
            const float Seconds = NightOf(Month, Day) + Offset * FaceDayLengthSeconds;
            Manager->SetGameClockSeconds(Seconds);
            if (Manager->GetMoonPhase() == Phase && Manager->IsNight())
            {
                bOutFound = true;
                return Seconds;
            }
        }
        bOutFound = false;
        return NightOf(Month, Day);
    }

    void MakePlainCell(FGridCell& Cell, EBiomeType Biome, bool bWater = false)
    {
        Cell.Biome = Biome;
        Cell.bIsWater = bWater;
        Cell.bEternallyPure = false;
        Cell.Memory.bDegrading = false;
        for (FRealState* State : { &Cell.State, &Cell.TargetState })
        {
            State->Direction.Body = 0.4f;
            State->Direction.Mind = 0.2f;
            State->Direction.Spirit = 0.2f;
            State->Direction.Nature = 0.2f;
            State->Meta.Corruption = 0.2f;
            State->Meta.Purity = 0.5f;
            State->Meta.Stability = 0.5f;
            State->Meta.Distortion = 0.2f;
        }
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistNightFaces_EachFaceHitsItsOwnPlaceAndAxis,
    "Herbalist.NightHorror.EachFaceHitsItsOwnPlaceAndAxis",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistNightFaces_EachFaceHitsItsOwnPlaceAndAxis::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;
    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("Manager spawned"), Manager)) return false;

    // Вурдалаки: ночь в Степи на искажённой земле -> Порча вверх. Соседняя
    // степная клетка без искажения остаётся нетронутой.
    FGridCell* Kurgan = Manager->GetCell(0, 0);
    FGridCell* CleanSteppe = Manager->GetCell(1, 0);
    if (!TestNotNull(TEXT("Клетки есть"), Kurgan) || !TestNotNull(TEXT("Клетки есть"), CleanSteppe))
    {
        Manager->Destroy();
        return false;
    }
    MakePlainCell(*Kurgan, EBiomeType::Steppe);
    Kurgan->State.Meta.Distortion = 0.7f;
    Kurgan->TargetState.Meta.Distortion = 0.7f;
    MakePlainCell(*CleanSteppe, EBiomeType::Steppe);

    // Лето, чтобы не поймать Лихоманок (осень); фазу луны подбираем не
    // новолунную и не полнолунную -- иначе Навьи и Оборотни вмешались бы.
    bool bFound = false;
    const float WaxingNight = FindNightWithPhase(Manager, 7, 10, EMoonPhase::WaxingMoon, bFound);
    if (!TestTrue(TEXT("Нашли растущую луну летней ночью"), bFound)) { Manager->Destroy(); return false; }

    const float KurganCorruptionBefore = Kurgan->TargetState.Meta.Corruption;
    const float CleanCorruptionBefore = CleanSteppe->TargetState.Meta.Corruption;
    Manager->SetGameClockSeconds(WaxingNight);
    Manager->UpdateEntityManifestations(1.0f);

    TestTrue(TEXT("Вурдалаки: на искажённой степной земле Порча растёт"),
        Kurgan->TargetState.Meta.Corruption > KurganCorruptionBefore);
    TestEqual(TEXT("Вурдалаки: чистую степную клетку не трогают"),
        CleanSteppe->TargetState.Meta.Corruption, CleanCorruptionBefore);

    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistNightFaces_MoonAndSeasonDecideWhoWalks,
    "Herbalist.NightHorror.MoonAndSeasonDecideWhoWalks",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistNightFaces_MoonAndSeasonDecideWhoWalks::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;
    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("Manager spawned"), Manager)) return false;

    FGridCell* Forest = Manager->GetCell(2, 0);
    FGridCell* BogWater = Manager->GetCell(3, 0);
    FGridCell* BogLand = Manager->GetCell(4, 0);
    if (!TestNotNull(TEXT("Клетки есть"), Forest) || !TestNotNull(TEXT("Клетки есть"), BogWater)
        || !TestNotNull(TEXT("Клетки есть"), BogLand))
    {
        Manager->Destroy();
        return false;
    }

    // Оборотни: полнолуние в лесу -> Стабильность вниз.
    MakePlainCell(*Forest, EBiomeType::Taiga);
    bool bFound = false;
    const float FullMoonNight = FindNightWithPhase(Manager, 7, 10, EMoonPhase::FullMoon, bFound);
    if (!TestTrue(TEXT("Нашли полнолунную летнюю ночь"), bFound)) { Manager->Destroy(); return false; }
    MakePlainCell(*Forest, EBiomeType::Taiga);
    const float StabilityBefore = Forest->TargetState.Meta.Stability;
    Manager->SetGameClockSeconds(FullMoonNight);
    Manager->UpdateEntityManifestations(1.0f);
    TestTrue(TEXT("Оборотни: в полнолуние лес теряет Стабильность"),
        Forest->TargetState.Meta.Stability < StabilityBefore);

    // Навьи: новолуние -> Искажение по всей сетке, даже в чистой Тундре.
    FGridCell* Tundra = Manager->GetCell(5, 0);
    if (!TestNotNull(TEXT("Клетка тундры есть"), Tundra)) { Manager->Destroy(); return false; }
    MakePlainCell(*Tundra, EBiomeType::Tundra);
    const float NewMoonNight = FindNightWithPhase(Manager, 7, 10, EMoonPhase::NewMoon, bFound);
    if (!TestTrue(TEXT("Нашли безлунную летнюю ночь"), bFound)) { Manager->Destroy(); return false; }
    MakePlainCell(*Tundra, EBiomeType::Tundra);
    const float DistortionBefore = Tundra->TargetState.Meta.Distortion;
    Manager->SetGameClockSeconds(NewMoonNight);
    Manager->UpdateEntityManifestations(1.0f);
    TestTrue(TEXT("Навьи: в новолуние Морок растёт везде"),
        Tundra->TargetState.Meta.Distortion > DistortionBefore);

    // Черти: ночь на болотной воде -> Порча; болотная суша в ту же ночь
    // (не осень, не новолуние) остаётся нетронутой.
    MakePlainCell(*BogWater, EBiomeType::Bog, /*bWater=*/true);
    MakePlainCell(*BogLand, EBiomeType::Bog);
    const float WaxingNight = FindNightWithPhase(Manager, 7, 10, EMoonPhase::WaxingMoon, bFound);
    if (!TestTrue(TEXT("Нашли растущую луну летней ночью"), bFound)) { Manager->Destroy(); return false; }
    MakePlainCell(*BogWater, EBiomeType::Bog, /*bWater=*/true);
    MakePlainCell(*BogLand, EBiomeType::Bog);
    const float WaterCorruptionBefore = BogWater->TargetState.Meta.Corruption;
    const float LandCorruptionBefore = BogLand->TargetState.Meta.Corruption;
    Manager->SetGameClockSeconds(WaxingNight);
    Manager->UpdateEntityManifestations(1.0f);
    TestTrue(TEXT("Черти: на воде Болота Порча растёт"),
        BogWater->TargetState.Meta.Corruption > WaterCorruptionBefore);
    TestEqual(TEXT("Черти: болотную сушу в ту же ночь не трогают"),
        BogLand->TargetState.Meta.Corruption, LandCorruptionBefore);

    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistNightFaces_LihomankiOnlyInAutumnDampBiomes,
    "Herbalist.NightHorror.LihomankiOnlyInAutumnDampBiomes",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistNightFaces_LihomankiOnlyInAutumnDampBiomes::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;
    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("Manager spawned"), Manager)) return false;

    FGridCell* Floodplain = Manager->GetCell(6, 0);
    FGridCell* Steppe = Manager->GetCell(7, 0);
    if (!TestNotNull(TEXT("Клетки есть"), Floodplain) || !TestNotNull(TEXT("Клетки есть"), Steppe))
    {
        Manager->Destroy();
        return false;
    }

    // Летняя ночь -- Лихоманок нет даже в пойме.
    MakePlainCell(*Floodplain, EBiomeType::Floodplain);
    bool bFound = false;
    const float SummerNight = FindNightWithPhase(Manager, 7, 10, EMoonPhase::WaxingMoon, bFound);
    if (!TestTrue(TEXT("Нашли летнюю ночь растущей луны"), bFound)) { Manager->Destroy(); return false; }
    MakePlainCell(*Floodplain, EBiomeType::Floodplain);
    float BodyBefore = Floodplain->TargetState.Direction.Body;
    Manager->SetGameClockSeconds(SummerNight);
    Manager->UpdateEntityManifestations(1.0f);
    TestEqual(TEXT("Летом Лихоманок нет"), Floodplain->TargetState.Direction.Body, BodyBefore);

    // Осенняя ночь -- в пойме Тело падает, в сухой степи нет.
    const float AutumnNight = FindNightWithPhase(Manager, 10, 10, EMoonPhase::WaxingMoon, bFound);
    if (!TestTrue(TEXT("Нашли осеннюю ночь растущей луны"), bFound)) { Manager->Destroy(); return false; }
    MakePlainCell(*Floodplain, EBiomeType::Floodplain);
    MakePlainCell(*Steppe, EBiomeType::Steppe);
    BodyBefore = Floodplain->TargetState.Direction.Body;
    const float SteppeBodyBefore = Steppe->TargetState.Direction.Body;
    Manager->SetGameClockSeconds(AutumnNight);
    Manager->UpdateEntityManifestations(1.0f);
    TestTrue(TEXT("Лихоманки: осенней ночью в пойме Тело падает"),
        Floodplain->TargetState.Direction.Body < BodyBefore);
    TestEqual(TEXT("Лихоманки: сухую степь не трогают"),
        Steppe->TargetState.Direction.Body, SteppeBodyBefore);

    Manager->Destroy();
    return true;
}

#endif // WITH_AUTOMATION_TESTS && WITH_EDITOR
