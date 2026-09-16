// Source/ProjectHerbalistTests/Private/Tests/IngredientHarvestWindowTest.cpp
//
// Сезон/время суток/луна/погода -> ингредиенты (DESIGN_World_State.md §15/§16,
// звено 8: "Сезон/погода → ингредиенты"), 2026-08-29, по прямому запросу
// пользователя ("прорабатываем сбор, связку ингредиентов с сезонами, погодой
// и прочими факторами"). GetRandomResourceForBiome уже смещает выбор по
// дистанции Cell.State/BaseState (Herbalist.Registry.SuitabilityBiasesToward-
// CloserBaseState, IngredientRegistryTest.cpp) -- эти тесты проверяют новый,
// независимый слой множителей поверх неё: чтобы изолировать эффект окна от
// эффекта дистанции, у обоих кандидатов в каждом тесте BaseState совпадает с
// Cell.State (Suitability = 1 у обоих), разница только в гейте.

#include "Core/Subsystems/IngredientRegistrySubsystem.h"
#include "Core/Data/IngredientTableRow.h"
#include "Misc/AutomationTest.h"
#include "Engine/DataTable.h"
#include "Engine/GameInstance.h"
#include "Engine/Engine.h"

#if WITH_AUTOMATION_TESTS

namespace
{
    UIngredientRegistrySubsystem* MakeWindowTestRegistry(UDataTable* Table)
    {
        UGameInstance* OwnerGameInstance = NewObject<UGameInstance>(GEngine);
        UIngredientRegistrySubsystem* Registry = NewObject<UIngredientRegistrySubsystem>(OwnerGameInstance);
        Registry->LoadFromDataTable(Table);
        return Registry;
    }

    UDataTable* MakeWindowTestTable()
    {
        UDataTable* Table = NewObject<UDataTable>();
        Table->RowStruct = FIngredientTableRow::StaticStruct();
        return Table;
    }

    // Считает, сколько раз из Trials выпал каждый из двух рядов -- тот же
    // приём, что уже в SuitabilityBiasesTowardCloserBaseState: не точное
    // соотношение (завязано на IngredientWindowMismatchMultiplier), а
    // направление эффекта.
    void CountPicks(UIngredientRegistrySubsystem* Registry, const FGridCell& Cell, const FHarvestContext& Context,
        FName NameA, FName NameB, int32& OutCountA, int32& OutCountB)
    {
        FRandomStream Rng(777);
        OutCountA = 0;
        OutCountB = 0;
        const int32 Trials = 2000;
        for (int32 i = 0; i < Trials; ++i)
        {
            const FName Picked = Registry->GetRandomResourceForBiome(Cell, Context, Rng);
            if (Picked == NameA) ++OutCountA;
            else if (Picked == NameB) ++OutCountB;
        }
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistRegistry_SeasonWindowBiasesTowardMatchingSeason,
    "Herbalist.Registry.SeasonWindowBiasesTowardMatchingSeason",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistRegistry_SeasonWindowBiasesTowardMatchingSeason::RunTest(const FString& Parameters)
{
    UDataTable* Table = MakeWindowTestTable();

    FIngredientTableRow SummerRow;
    SummerRow.AllowedBiomes = { EBiomeType::Bog };
    SummerRow.AllowedSeasons = { ESeason::Summer };
    Table->AddRow(FName(TEXT("SummerHerb")), SummerRow);

    FIngredientTableRow SpringRow;
    SpringRow.AllowedBiomes = { EBiomeType::Bog };
    SpringRow.AllowedSeasons = { ESeason::Spring };
    Table->AddRow(FName(TEXT("SpringHerb")), SpringRow);

    UIngredientRegistrySubsystem* Registry = MakeWindowTestRegistry(Table);

    FGridCell Cell;
    Cell.Biome = EBiomeType::Bog;

    FHarvestContext Context;
    Context.Season = ESeason::Summer;

    int32 SummerCount = 0, SpringCount = 0;
    CountPicks(Registry, Cell, Context, FName(TEXT("SummerHerb")), FName(TEXT("SpringHerb")), SummerCount, SpringCount);

    TestTrue(TEXT("In-season candidate picked far more often than out-of-season one"), SummerCount > SpringCount * 3);
    TestTrue(TEXT("Out-of-season candidate is suppressed, not impossible"), SpringCount > 0);

    Registry->Reset();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistRegistry_AutumnOnlyDoesNotBlockItsOtherAllowedSeason,
    "Herbalist.Registry.AutumnOnlyDoesNotBlockItsOtherAllowedSeason",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistRegistry_AutumnOnlyDoesNotBlockItsOtherAllowedSeason::RunTest(const FString& Parameters)
{
    // Компендиумный паттерн "корень копают ранней весной ИЛИ поздней осенью":
    // AllowedSeasons=[Spring, Summer] + bAutumnOnly=true. С календарём
    // (2026-09-16) Лето у такой травы означает осень: летом её нет, осенью и
    // весной есть.
    UDataTable* Table = MakeWindowTestTable();

    FIngredientTableRow Row;
    Row.AllowedBiomes = { EBiomeType::Bog };
    Row.AllowedSeasons = { ESeason::Spring, ESeason::Summer };
    Row.bAutumnOnly = true;
    Table->AddRow(FName(TEXT("SpringOrAutumnRoot")), Row);

    FIngredientTableRow OtherRow;
    OtherRow.AllowedBiomes = { EBiomeType::Bog };
    OtherRow.AllowedSeasons = { ESeason::Winter };
    Table->AddRow(FName(TEXT("WinterOnlyOther")), OtherRow);

    UIngredientRegistrySubsystem* Registry = MakeWindowTestRegistry(Table);

    FGridCell Cell;
    Cell.Biome = EBiomeType::Bog;

    auto PicksIn = [&](ESeason Season, int32& OutRoot, int32& OutOther)
    {
        FHarvestContext Context;
        Context.Season = Season;
        CountPicks(Registry, Cell, Context, FName(TEXT("SpringOrAutumnRoot")), FName(TEXT("WinterOnlyOther")), OutRoot, OutOther);
    };

    int32 Root = 0, Other = 0;
    PicksIn(ESeason::Spring, Root, Other);
    TestTrue(TEXT("Весна не затронута bAutumnOnly -- корень выпадает намного чаще зимней травы"), Root > Other * 3);

    PicksIn(ESeason::Autumn, Root, Other);
    TestTrue(TEXT("Осень -- окно осенней травы: корень выпадает намного чаще"), Root > Other * 3);

    PicksIn(ESeason::Summer, Root, Other);
    TestTrue(TEXT("Лето (июнь–август) закрыто для осенней травы -- близко к зимней"), Root < Other * 3);

    Registry->Reset();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistRegistry_SummerHerbIsNotAutumnHerb,
    "Herbalist.Registry.SummerHerbIsNotAutumnHerb",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistRegistry_SummerHerbIsNotAutumnHerb::RunTest(const FString& Parameters)
{
    // Летняя трава -- только июнь–август (решение пользователя 2026-09-16):
    // осень хоть и Лето по лору, но окно летней травы закрыто.
    UDataTable* Table = MakeWindowTestTable();

    FIngredientTableRow SummerRow;
    SummerRow.AllowedBiomes = { EBiomeType::Bog };
    SummerRow.AllowedSeasons = { ESeason::Summer };
    Table->AddRow(FName(TEXT("SummerHerb")), SummerRow);

    FIngredientTableRow AnySeasonRow;
    AnySeasonRow.AllowedBiomes = { EBiomeType::Bog };
    Table->AddRow(FName(TEXT("AnySeasonHerb")), AnySeasonRow);

    UIngredientRegistrySubsystem* Registry = MakeWindowTestRegistry(Table);

    FGridCell Cell;
    Cell.Biome = EBiomeType::Bog;

    FHarvestContext AutumnContext;
    AutumnContext.Season = ESeason::Autumn;
    int32 SummerInAutumn = 0, AnyInAutumn = 0;
    CountPicks(Registry, Cell, AutumnContext, FName(TEXT("SummerHerb")), FName(TEXT("AnySeasonHerb")), SummerInAutumn, AnyInAutumn);
    TestTrue(TEXT("Осенью летняя трава подавлена"), SummerInAutumn * 3 < AnyInAutumn);

    FHarvestContext SummerContext;
    SummerContext.Season = ESeason::Summer;
    int32 SummerInSummer = 0, AnyInSummer = 0;
    CountPicks(Registry, Cell, SummerContext, FName(TEXT("SummerHerb")), FName(TEXT("AnySeasonHerb")), SummerInSummer, AnyInSummer);
    TestTrue(TEXT("Летом летняя трава не подавлена"), SummerInSummer >= AnyInSummer * 0.7f);

    Registry->Reset();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistRegistry_HarvestTimeWindowGatesDawnOnlyIngredient,
    "Herbalist.Registry.HarvestTimeWindowGatesDawnOnlyIngredient",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistRegistry_HarvestTimeWindowGatesDawnOnlyIngredient::RunTest(const FString& Parameters)
{
    UDataTable* Table = MakeWindowTestTable();

    FIngredientTableRow DawnRow;
    DawnRow.AllowedBiomes = { EBiomeType::Bog };
    DawnRow.HarvestTimeWindow = EHarvestTimeWindow::Dawn;
    Table->AddRow(FName(TEXT("DawnHerb")), DawnRow);

    FIngredientTableRow AnyTimeRow;
    AnyTimeRow.AllowedBiomes = { EBiomeType::Bog };
    Table->AddRow(FName(TEXT("AnyTimeHerb")), AnyTimeRow);

    UIngredientRegistrySubsystem* Registry = MakeWindowTestRegistry(Table);

    FGridCell Cell;
    Cell.Biome = EBiomeType::Bog;

    FHarvestContext NightContext;
    NightContext.TimeOfDay = EHarvestTimeWindow::Night;

    int32 DawnAtNight = 0, AnyAtNight = 0;
    CountPicks(Registry, Cell, NightContext, FName(TEXT("DawnHerb")), FName(TEXT("AnyTimeHerb")), DawnAtNight, AnyAtNight);
    TestTrue(TEXT("Dawn-only ingredient suppressed at night"), DawnAtNight < AnyAtNight);

    FHarvestContext DawnContext;
    DawnContext.TimeOfDay = EHarvestTimeWindow::Dawn;

    int32 DawnAtDawn = 0, AnyAtDawn = 0;
    CountPicks(Registry, Cell, DawnContext, FName(TEXT("DawnHerb")), FName(TEXT("AnyTimeHerb")), DawnAtDawn, AnyAtDawn);
    TestTrue(TEXT("Dawn-only ingredient at least as likely as the unrestricted one at dawn"), DawnAtDawn >= AnyAtDawn * 0.7f);

    Registry->Reset();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistRegistry_MoonPhaseGatesRequiredPhase,
    "Herbalist.Registry.MoonPhaseGatesRequiredPhase",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistRegistry_MoonPhaseGatesRequiredPhase::RunTest(const FString& Parameters)
{
    UDataTable* Table = MakeWindowTestTable();

    FIngredientTableRow FullMoonRow;
    FullMoonRow.AllowedBiomes = { EBiomeType::Bog };
    FullMoonRow.bRequiresMoonPhase = true;
    FullMoonRow.RequiredMoonPhase = EMoonPhase::FullMoon;
    Table->AddRow(FName(TEXT("FullMoonHerb")), FullMoonRow);

    FIngredientTableRow AnyMoonRow;
    AnyMoonRow.AllowedBiomes = { EBiomeType::Bog };
    Table->AddRow(FName(TEXT("AnyMoonHerb")), AnyMoonRow);

    UIngredientRegistrySubsystem* Registry = MakeWindowTestRegistry(Table);

    FGridCell Cell;
    Cell.Biome = EBiomeType::Bog;

    FHarvestContext NewMoonContext;
    NewMoonContext.MoonPhase = EMoonPhase::NewMoon;

    int32 FullAtNewMoon = 0, AnyAtNewMoon = 0;
    CountPicks(Registry, Cell, NewMoonContext, FName(TEXT("FullMoonHerb")), FName(TEXT("AnyMoonHerb")), FullAtNewMoon, AnyAtNewMoon);
    TestTrue(TEXT("Full-moon-only ingredient suppressed on a new moon"), FullAtNewMoon < AnyAtNewMoon);

    Registry->Reset();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistRegistry_DryWeatherGatesRequiredIngredient,
    "Herbalist.Registry.DryWeatherGatesRequiredIngredient",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistRegistry_DryWeatherGatesRequiredIngredient::RunTest(const FString& Parameters)
{
    UDataTable* Table = MakeWindowTestTable();

    FIngredientTableRow DryOnlyRow;
    DryOnlyRow.AllowedBiomes = { EBiomeType::Bog };
    DryOnlyRow.bRequiresDryWeather = true;
    Table->AddRow(FName(TEXT("DryOnlyHerb")), DryOnlyRow);

    FIngredientTableRow AnyWeatherRow;
    AnyWeatherRow.AllowedBiomes = { EBiomeType::Bog };
    Table->AddRow(FName(TEXT("AnyWeatherHerb")), AnyWeatherRow);

    UIngredientRegistrySubsystem* Registry = MakeWindowTestRegistry(Table);

    FGridCell Cell;
    Cell.Biome = EBiomeType::Bog;

    FHarvestContext RainyContext;
    RainyContext.bDryWeather = false;

    int32 DryInRain = 0, AnyInRain = 0;
    CountPicks(Registry, Cell, RainyContext, FName(TEXT("DryOnlyHerb")), FName(TEXT("AnyWeatherHerb")), DryInRain, AnyInRain);
    TestTrue(TEXT("Dry-weather-only ingredient suppressed while it's raining"), DryInRain < AnyInRain);

    Registry->Reset();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistRegistry_HarvestStressDoesNotChangeWhichHerbIsPicked,
    "Herbalist.Registry.HarvestStressDoesNotChangeWhichHerbIsPicked",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistRegistry_HarvestStressDoesNotChangeWhichHerbIsPicked::RunTest(const FString& Parameters)
{
    // Штраф истощения -- меньше растений, а не другой набор трав (решение
    // 2026-09-12; сделано 2026-09-14 вместе с повторной попыткой отрастания).
    // Раньше PickWeightedResource домножал вес каждого кандидата на
    // (1 - HarvestStress): на состав это не влияло, а при стрессе 1.0 выбор
    // падал в фолбэк на первого кандидата. Одно зерно -- одна трава при любом
    // стрессе. Два ряда, не один -- с одним рядом веса не считаются вовсе.
    UDataTable* Table = MakeWindowTestTable();

    FIngredientTableRow RowA;
    RowA.AllowedBiomes = { EBiomeType::Bog };
    Table->AddRow(FName(TEXT("HerbA")), RowA);

    FIngredientTableRow RowB;
    RowB.AllowedBiomes = { EBiomeType::Bog };
    Table->AddRow(FName(TEXT("HerbB")), RowB);

    UIngredientRegistrySubsystem* Registry = MakeWindowTestRegistry(Table);

    FGridCell Rested;
    Rested.Biome = EBiomeType::Bog;
    Rested.HarvestStress = 0.0f;
    FGridCell Exhausted = Rested;
    Exhausted.HarvestStress = 1.0f;

    FHarvestContext Context;
    int32 Mismatches = 0;
    bool bSawA = false;
    bool bSawB = false;
    for (int32 Seed = 1; Seed <= 64; ++Seed)
    {
        FRandomStream RestedRng(Seed);
        FRandomStream ExhaustedRng(Seed);
        const FName RestedPick = Registry->GetRandomResourceForBiome(Rested, Context, RestedRng);
        const FName ExhaustedPick = Registry->GetRandomResourceForBiome(Exhausted, Context, ExhaustedRng);
        Mismatches += RestedPick != ExhaustedPick ? 1 : 0;
        bSawA |= RestedPick == FName(TEXT("HerbA"));
        bSawB |= RestedPick == FName(TEXT("HerbB"));
    }
    TestEqual(TEXT("Стресс 0 и 1 при одном зерне -- одна трава"), Mismatches, 0);
    TestTrue(TEXT("Sanity: выбор не вырожден -- выпадают обе травы"), bSawA && bSawB);

    Registry->Reset();
    return true;
}

#endif // WITH_AUTOMATION_TESTS
