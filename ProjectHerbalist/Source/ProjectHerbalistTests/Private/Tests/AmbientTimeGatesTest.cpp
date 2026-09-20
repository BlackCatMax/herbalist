// Source/ProjectHerbalistTests/Private/Tests/AmbientTimeGatesTest.cpp
//
// Время для Низших, у которых его не было (решение пользователя 2026-09-20:
// «у 17 из 33 Низших нет ни суток, ни сезона»). Двенадцать карточек получили
// сутки, сезон или погоду; пять остались круглогодичным фоном своего биома.
//
// Проверяем не каждую из двенадцати поимённо (это была бы копия таблицы
// данных), а то, что механизм реально работает в обе стороны: сезонная
// карточка молчит вне своего сезона и проявляется в своём, а фоновая стоит
// круглый год. Разбор самих гейтов -- AmbientEntityTest.cpp и пачки 4/5.

#include "Core/World/GridWorldManager.h"
#include "Core/Entities/AmbientEntityTypes.h"
#include "Core/Types/BiomeTypes.h"
#include "Core/Types/HerbalistCalendar.h"
#include "Misc/AutomationTest.h"
#include "Editor.h"
#include "Engine/World.h"

#if WITH_AUTOMATION_TESTS && WITH_EDITOR

#include "TestWorldHelpers.h"

namespace
{
    constexpr float TimeGateDayLengthSeconds = 32.0f * 60.0f;

    float DayNoon(int32 Month, int32 Day)
    {
        return HerbalistCore::Calendar::DayOfYearFromDate(Month, Day) * TimeGateDayLengthSeconds + 10.0f * 60.0f;
    }

    // Болотная клетка с высокой Порчей: подходит Гнильникам (лето) и, по
    // Природе, Трясинным духам (круглый год). Остальные болотные карточки
    // отсечены -- Стабильность выше порога Ржавых духов, Искажение ниже
    // порога Болотных огней.
    void MakeCorruptedBogCell(FGridCell& Cell, float Nature)
    {
        Cell.Biome = EBiomeType::Bog;
        Cell.bIsWater = false;
        Cell.Memory.bDegrading = false;
        for (FRealState* State : { &Cell.State, &Cell.TargetState })
        {
            State->Direction.Body = 0.1f;
            State->Direction.Mind = 0.1f;
            State->Direction.Spirit = 0.1f;
            State->Direction.Nature = Nature;
            State->Meta.Corruption = 0.9f;
            State->Meta.Purity = 0.4f;
            State->Meta.Stability = 0.9f;
            State->Meta.Distortion = 0.2f;
        }
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistTimeGates_SeasonalCardIsSilentOutOfSeason,
    "Herbalist.AmbientEntity.SeasonalCardIsSilentOutOfSeason",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistTimeGates_SeasonalCardIsSilentOutOfSeason::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;
    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("Manager spawned"), Manager)) return false;

    FGridCell* Cell = Manager->GetCell(0, 0);
    if (!TestNotNull(TEXT("Клетка (0,0) есть"), Cell)) { Manager->Destroy(); return false; }
    // Природа ниже порога Трясинных духов: на этой клетке спорит только
    // Гнильники против пустоты.
    MakeCorruptedBogCell(*Cell, 0.2f);

    Manager->SetGameClockSeconds(DayNoon(1, 15));   // зима
    Manager->UpdateEntityManifestations(1.0f);
    TestNotEqual(TEXT("Зимой Гнильники молчат"), Cell->ManifestedEntityID, FName(TEXT("Гнильники")));

    MakeCorruptedBogCell(*Cell, 0.2f);
    Manager->SetGameClockSeconds(DayNoon(7, 15));   // лето
    Manager->UpdateEntityManifestations(1.0f);
    TestEqual(TEXT("Летом Гнильники проявляются"), Cell->ManifestedEntityID, FName(TEXT("Гнильники")));

    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistTimeGates_YearRoundCardHoldsItsBiomeAllYear,
    "Herbalist.AmbientEntity.YearRoundCardHoldsItsBiomeAllYear",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistTimeGates_YearRoundCardHoldsItsBiomeAllYear::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;
    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("Manager spawned"), Manager)) return false;

    FGridCell* Cell = Manager->GetCell(0, 0);
    if (!TestNotNull(TEXT("Клетка (0,0) есть"), Cell)) { Manager->Destroy(); return false; }

    // Пять карточек намеренно остались без времени -- фон своего биома
    // (Трясинные духи, Межевые, Степные духи, Моховые духи, Жердяи). Берём
    // Трясинных: Порча ниже порога Гнильников, Природа выше их порога.
    const int32 Months[4] = { 1, 4, 7, 10 };   // зима, весна, лето, осень
    for (int32 Month : Months)
    {
        MakeCorruptedBogCell(*Cell, 0.7f);
        Cell->State.Meta.Corruption = 0.2f;
        Cell->TargetState.Meta.Corruption = 0.2f;
        Manager->SetGameClockSeconds(DayNoon(Month, 15));
        Manager->UpdateEntityManifestations(1.0f);
        TestEqual(FString::Printf(TEXT("Месяц %d -- Трясинные духи на месте"), Month),
            Cell->ManifestedEntityID, FName(TEXT("Трясинные духи")));
    }

    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistTimeGates_TaigaAndBroadleafHaveTemporalCardsNow,
    "Herbalist.AmbientEntity.TaigaAndBroadleafHaveTemporalCardsNow",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistTimeGates_TaigaAndBroadleafHaveTemporalCardsNow::RunTest(const FString& Parameters)
{
    // До 2026-09-20 в Тайге и Широколиственном лесу не было НИ ОДНОГО
    // Низшего с условием по времени: у обоих биомов не было ни суточного,
    // ни годового ритма. Проверяем по реестру, а не по проявлению: это
    // свойство данных, и оно должно пережить любую перетасовку порогов.
    int32 TaigaTemporal = 0;
    int32 BroadleafTemporal = 0;
    int32 YearRound = 0;
    for (const FAmbientEntityDefinition& Def : GetAmbientEntityDefinitions())
    {
        const bool bTemporal = Def.bRequiresNight || Def.bRequiresDusk || Def.bRequiresSeason
            || Def.bRequiresMoonPhase || Def.bRequiresWeather || Def.bRequiresLateSummer
            || Def.bRequiresKupalaNight;
        if (!bTemporal) ++YearRound;
        if (Def.Biome == EBiomeType::Taiga && bTemporal) ++TaigaTemporal;
        if (Def.Biome == EBiomeType::BroadleafForest && bTemporal) ++BroadleafTemporal;
    }

    TestTrue(FString::Printf(TEXT("В Тайге есть карточки со временем (%d)"), TaigaTemporal), TaigaTemporal > 0);
    TestTrue(FString::Printf(TEXT("В Широколиственном лесу есть карточки со временем (%d)"), BroadleafTemporal), BroadleafTemporal > 0);
    // Пять фоновых -- осознанное решение пользователя, не недоработка:
    // Трясинные духи, Межевые, Степные духи, Моховые духи, Жердяи.
    TestEqual(TEXT("Круглогодичных осталось ровно пять"), YearRound, 5);
    return true;
}

#endif // WITH_AUTOMATION_TESTS && WITH_EDITOR
