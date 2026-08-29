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
    // AllowedSeasons=[Spring, Summer] + bAutumnOnly=true. bAutumnOnly должен
    // сузить только ветку Лета (до его позднего окна) и не трогать Весну.
    UDataTable* Table = MakeWindowTestTable();

    FIngredientTableRow Row;
    Row.AllowedBiomes = { EBiomeType::Bog };
    Row.AllowedSeasons = { ESeason::Spring, ESeason::Summer };
    Row.bAutumnOnly = true;
    Table->AddRow(FName(TEXT("SpringOrLateAutumnRoot")), Row);

    FIngredientTableRow OtherRow;
    OtherRow.AllowedBiomes = { EBiomeType::Bog };
    OtherRow.AllowedSeasons = { ESeason::Winter };
    Table->AddRow(FName(TEXT("WinterOnlyOther")), OtherRow);

    UIngredientRegistrySubsystem* Registry = MakeWindowTestRegistry(Table);

    FGridCell Cell;
    Cell.Biome = EBiomeType::Bog;

    FHarvestContext SpringContext;
    SpringContext.Season = ESeason::Spring;
    SpringContext.bLateSummer = false;

    int32 SpringPick = 0, WinterPick = 0;
    CountPicks(Registry, Cell, SpringContext, FName(TEXT("SpringOrLateAutumnRoot")), FName(TEXT("WinterOnlyOther")), SpringPick, WinterPick);
    TestTrue(TEXT("Spring is unaffected by bAutumnOnly -- picked far more than the Winter-only row"), SpringPick > WinterPick * 3);

    FHarvestContext EarlySummerContext;
    EarlySummerContext.Season = ESeason::Summer;
    EarlySummerContext.bLateSummer = false;

    int32 EarlySummerPick = 0, WinterPick2 = 0;
    CountPicks(Registry, Cell, EarlySummerContext, FName(TEXT("SpringOrLateAutumnRoot")), FName(TEXT("WinterOnlyOther")), EarlySummerPick, WinterPick2);
    TestTrue(TEXT("Early (non-late) Summer is gated by bAutumnOnly -- close to the Winter-only row"), EarlySummerPick < WinterPick2 * 3);

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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistRegistry_ExhaustedCellStillYieldsSomethingNotNothing,
    "Herbalist.Registry.ExhaustedCellStillYieldsSomethingNotNothing",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistRegistry_ExhaustedCellStillYieldsSomethingNotNothing::RunTest(const FString& Parameters)
{
    // (1 - HarvestStress), DESIGN_World_State.md §15 звено 4: клетка на пике
    // истощения (HarvestStress=1) гасит TotalWeight до нуля -- проверяем
    // безопасный откат (первый кандидат), не NAME_None, тот же принцип, что
    // уже покрыт для TotalWeight<=KINDA_SMALL_NUMBER выше по другой причине.
    // Два ряда, не один -- с одним GetRandomResourceForBiome вообще не считает
    // веса (ранний выход "Candidates->Num() == 1"), это не проверило бы StressFactor.
    UDataTable* Table = MakeWindowTestTable();

    FIngredientTableRow RowA;
    RowA.AllowedBiomes = { EBiomeType::Bog };
    Table->AddRow(FName(TEXT("HerbA")), RowA);

    FIngredientTableRow RowB;
    RowB.AllowedBiomes = { EBiomeType::Bog };
    Table->AddRow(FName(TEXT("HerbB")), RowB);

    UIngredientRegistrySubsystem* Registry = MakeWindowTestRegistry(Table);

    FGridCell Cell;
    Cell.Biome = EBiomeType::Bog;
    Cell.HarvestStress = 1.0f;

    FHarvestContext Context;
    FRandomStream Rng(1);
    const FName Picked = Registry->GetRandomResourceForBiome(Cell, Context, Rng);
    TestEqual(TEXT("Fully exhausted cell still returns the first candidate, not NAME_None"), Picked, FName(TEXT("HerbA")));

    Registry->Reset();
    return true;
}

#endif // WITH_AUTOMATION_TESTS
