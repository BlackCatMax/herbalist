// Source/ProjectHerbalistTests/Private/Tests/WaterTypeRegistryTest.cpp
//
// Полный аудит проекта (2026-08-31, по прямому запросу пользователя):
// WaterTypeRegistrySubsystem был уже НАЗВАН как известный пробел в
// ROADMAP.md "Известные пробелы в тестировании" -- мал и дёшев закрыть в
// этом же проходе, взят в фазу 2 плана. Тот же приём конструирования
// синтетической DataTable напрямую (не через GameInstance::GetSubsystem,
// недоступный в editor-world automation-тестах), что уже использован для
// IngredientRegistrySubsystem в IngredientHarvestWindowTest.cpp.

#include "Core/Subsystems/WaterTypeRegistrySubsystem.h"
#include "Core/Data/WaterTypeRow.h"
#include "Core/Types/BiomeTypes.h"
#include "Misc/AutomationTest.h"
#include "Engine/DataTable.h"
#include "Engine/GameInstance.h"
#include "Engine/Engine.h"
#include "Math/RandomStream.h"

#if WITH_AUTOMATION_TESTS

namespace
{
    UWaterTypeRegistrySubsystem* MakeWaterRegistry(UDataTable* Table)
    {
        UGameInstance* OwnerGameInstance = NewObject<UGameInstance>(GEngine);
        UWaterTypeRegistrySubsystem* Registry = NewObject<UWaterTypeRegistrySubsystem>(OwnerGameInstance);
        Registry->LoadFromDataTable(Table);
        return Registry;
    }

    UDataTable* MakeWaterTable()
    {
        UDataTable* Table = NewObject<UDataTable>();
        Table->RowStruct = FWaterTypeRow::StaticStruct();
        return Table;
    }

    FWaterTypeRow MakeWaterRow(FName ID, EBiomeType Biome, float Rarity, float Purity = 0.5f)
    {
        FWaterTypeRow Row;
        Row.WaterTypeID = ID;
        Row.AllowedBiomes = { Biome };
        Row.Rarity = Rarity;
        Row.BasePurity = Purity;
        return Row;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistWaterRegistry_LoadFromDataTableParsesRows,
    "Herbalist.WaterRegistry.LoadFromDataTableParsesRows",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistWaterRegistry_LoadFromDataTableParsesRows::RunTest(const FString& Parameters)
{
    UDataTable* Table = MakeWaterTable();
    Table->AddRow(FName(TEXT("BogWater")), MakeWaterRow(FName(TEXT("BogWater")), EBiomeType::Bog, 1.0f));
    Table->AddRow(FName(TEXT("ForestSpring")), MakeWaterRow(FName(TEXT("ForestSpring")), EBiomeType::MixedForest, 1.0f));

    UWaterTypeRegistrySubsystem* Registry = MakeWaterRegistry(Table);
    if (!TestNotNull(TEXT("Registry constructed"), Registry)) return false;

    TestEqual(TEXT("Both rows parsed"), Registry->GetWaterTypeCount(), 2);
    TestTrue(TEXT("BogWater is a valid water type"), Registry->IsValidWaterType(FName(TEXT("BogWater"))));
    TestTrue(TEXT("ForestSpring is a valid water type"), Registry->IsValidWaterType(FName(TEXT("ForestSpring"))));
    TestFalse(TEXT("Unregistered ID is not valid"), Registry->IsValidWaterType(FName(TEXT("НичегоТакогоНет"))));

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistWaterRegistry_GetWaterTypeReturnsMatchingRowData,
    "Herbalist.WaterRegistry.GetWaterTypeReturnsMatchingRowData",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistWaterRegistry_GetWaterTypeReturnsMatchingRowData::RunTest(const FString& Parameters)
{
    UDataTable* Table = MakeWaterTable();
    Table->AddRow(FName(TEXT("SacredSpring")), MakeWaterRow(FName(TEXT("SacredSpring")), EBiomeType::MixedForest, 1.0f, /*Purity=*/0.95f));

    UWaterTypeRegistrySubsystem* Registry = MakeWaterRegistry(Table);
    if (!TestNotNull(TEXT("Registry constructed"), Registry)) return false;

    const FWaterTypeRow* Row = Registry->GetWaterType(FName(TEXT("SacredSpring")));
    if (!TestNotNull(TEXT("Row found"), Row)) return false;
    TestEqual(TEXT("BasePurity survives the round trip"), Row->BasePurity, 0.95f);

    TestNull(TEXT("Unknown water type returns nullptr, not a crash"), Registry->GetWaterType(FName(TEXT("Неизвестное"))));

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistWaterRegistry_GetWaterTypesForBiomeFiltersCorrectly,
    "Herbalist.WaterRegistry.GetWaterTypesForBiomeFiltersCorrectly",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistWaterRegistry_GetWaterTypesForBiomeFiltersCorrectly::RunTest(const FString& Parameters)
{
    UDataTable* Table = MakeWaterTable();
    Table->AddRow(FName(TEXT("BogWater")), MakeWaterRow(FName(TEXT("BogWater")), EBiomeType::Bog, 1.0f));
    Table->AddRow(FName(TEXT("ForestSpring")), MakeWaterRow(FName(TEXT("ForestSpring")), EBiomeType::MixedForest, 1.0f));

    UWaterTypeRegistrySubsystem* Registry = MakeWaterRegistry(Table);
    if (!TestNotNull(TEXT("Registry constructed"), Registry)) return false;

    const TArray<FName> BogTypes = Registry->GetWaterTypesForBiome(EBiomeType::Bog);
    TestEqual(TEXT("Exactly one water type for Bog"), BogTypes.Num(), 1);
    if (BogTypes.Num() == 1)
    {
        TestEqual(TEXT("It's BogWater"), BogTypes[0], FName(TEXT("BogWater")));
    }

    const TArray<FName> SteppeTypes = Registry->GetWaterTypesForBiome(EBiomeType::Steppe);
    TestEqual(TEXT("No water types registered for Steppe -- empty, not a crash"), SteppeTypes.Num(), 0);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistWaterRegistry_GetRandomWaterTypeBiasesTowardHigherRarity,
    "Herbalist.WaterRegistry.GetRandomWaterTypeBiasesTowardHigherRarity",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistWaterRegistry_GetRandomWaterTypeBiasesTowardHigherRarity::RunTest(const FString& Parameters)
{
    UDataTable* Table = MakeWaterTable();
    // "Rarity" здесь используется как вес выбора (GetRandomWaterType), не
    // обратная величина -- см. UWaterTypeRegistrySubsystem::GetRandomWaterType
    // (TotalWeight суммирует Rarity напрямую). Common должен выпадать чаще.
    Table->AddRow(FName(TEXT("CommonPuddle")), MakeWaterRow(FName(TEXT("CommonPuddle")), EBiomeType::Bog, /*Rarity=*/0.9f));
    Table->AddRow(FName(TEXT("RareSpring")), MakeWaterRow(FName(TEXT("RareSpring")), EBiomeType::Bog, /*Rarity=*/0.1f));

    UWaterTypeRegistrySubsystem* Registry = MakeWaterRegistry(Table);
    if (!TestNotNull(TEXT("Registry constructed"), Registry)) return false;

    FRandomStream Rng(4242);
    int32 CommonCount = 0, RareCount = 0;
    const int32 Trials = 2000;
    for (int32 i = 0; i < Trials; ++i)
    {
        const FName Picked = Registry->GetRandomWaterType(EBiomeType::Bog, Rng);
        if (Picked == FName(TEXT("CommonPuddle"))) ++CommonCount;
        else if (Picked == FName(TEXT("RareSpring"))) ++RareCount;
    }

    TestTrue(TEXT("Higher-rarity-weight water type is picked substantially more often"), CommonCount > RareCount * 3);
    TestTrue(TEXT("The rare type still gets picked sometimes, not starved to zero"), RareCount > 0);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistWaterRegistry_GetRandomWaterTypeReturnsNoneForEmptyBiome,
    "Herbalist.WaterRegistry.GetRandomWaterTypeReturnsNoneForEmptyBiome",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistWaterRegistry_GetRandomWaterTypeReturnsNoneForEmptyBiome::RunTest(const FString& Parameters)
{
    UDataTable* Table = MakeWaterTable();
    Table->AddRow(FName(TEXT("BogWater")), MakeWaterRow(FName(TEXT("BogWater")), EBiomeType::Bog, 1.0f));

    UWaterTypeRegistrySubsystem* Registry = MakeWaterRegistry(Table);
    if (!TestNotNull(TEXT("Registry constructed"), Registry)) return false;

    FRandomStream Rng(1);
    const FName Picked = Registry->GetRandomWaterType(EBiomeType::Steppe, Rng);
    TestEqual(TEXT("A biome with no registered water returns NAME_None, not a crash"), Picked, NAME_None);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistWaterRegistry_ResetClearsEverything,
    "Herbalist.WaterRegistry.ResetClearsEverything",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistWaterRegistry_ResetClearsEverything::RunTest(const FString& Parameters)
{
    UDataTable* Table = MakeWaterTable();
    Table->AddRow(FName(TEXT("BogWater")), MakeWaterRow(FName(TEXT("BogWater")), EBiomeType::Bog, 1.0f));

    UWaterTypeRegistrySubsystem* Registry = MakeWaterRegistry(Table);
    if (!TestNotNull(TEXT("Registry constructed"), Registry)) return false;
    TestEqual(TEXT("Precondition: one row loaded"), Registry->GetWaterTypeCount(), 1);

    Registry->Reset();

    TestEqual(TEXT("Count is zero after Reset"), Registry->GetWaterTypeCount(), 0);
    TestFalse(TEXT("Previously-valid ID is no longer valid after Reset"), Registry->IsValidWaterType(FName(TEXT("BogWater"))));
    TestEqual(TEXT("Per-biome cache cleared too"), Registry->GetWaterTypesForBiome(EBiomeType::Bog).Num(), 0);

    return true;
}

#endif // WITH_AUTOMATION_TESTS
