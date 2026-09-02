// Source/ProjectHerbalistTests/Private/Tests/IngredientRegistryAquaticTest.cpp
//
// Водные растения (2026-09-02, прямой запрос пользователя): "если у биома
// есть водные растения, то они разрешены к размещению на поверхности
// воды, и вода одновременно доступна". FIngredientTableRow::bGrowsOnWater
// -- отдельный, не смешанный с земляным пул (CachedAquaticResourcesByBiome),
// читается UIngredientRegistrySubsystem::GetRandomResourceForAquaticBiome.
// Тот же приём Registry/Table/Cell, что уже IngredientRegistryBiomeWeightTest.cpp.

#include "Core/Subsystems/IngredientRegistrySubsystem.h"
#include "Core/Data/IngredientTableRow.h"
#include "Misc/AutomationTest.h"
#include "Engine/DataTable.h"
#include "Engine/GameInstance.h"
#include "Engine/Engine.h"
#include "Math/RandomStream.h"

#if WITH_AUTOMATION_TESTS

namespace
{
    UIngredientRegistrySubsystem* MakeAquaticTestRegistry(UDataTable* Table)
    {
        UGameInstance* OwnerGameInstance = NewObject<UGameInstance>(GEngine);
        UIngredientRegistrySubsystem* Registry = NewObject<UIngredientRegistrySubsystem>(OwnerGameInstance);
        Registry->LoadFromDataTable(Table);
        return Registry;
    }

    UDataTable* MakeAquaticTestTable()
    {
        UDataTable* Table = NewObject<UDataTable>();
        Table->RowStruct = FIngredientTableRow::StaticStruct();
        return Table;
    }

    FGridCell MakeAquaticTestCell(EBiomeType Biome)
    {
        FGridCell Cell;
        Cell.X = 5; Cell.Y = 5;
        Cell.Biome = Biome;
        Cell.State.Magnitude = 0.5f;
        Cell.State.Direction.Body = 0.25f; Cell.State.Direction.Mind = 0.25f;
        Cell.State.Direction.Spirit = 0.25f; Cell.State.Direction.Nature = 0.25f;
        return Cell;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistIngredientAquatic_LandOnlyIngredientNeverPickedForWater,
    "Herbalist.IngredientAquatic.LandOnlyIngredientNeverPickedForWater",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistIngredientAquatic_LandOnlyIngredientNeverPickedForWater::RunTest(const FString& Parameters)
{
    UDataTable* Table = MakeAquaticTestTable();
    FGridCell Cell = MakeAquaticTestCell(EBiomeType::Floodplain);

    FIngredientTableRow LandRow;
    LandRow.AllowedBiomes = { EBiomeType::Floodplain };
    LandRow.BaseState = Cell.State;
    LandRow.bGrowsOnWater = false;
    Table->AddRow(FName(TEXT("Осока")), LandRow);

    UIngredientRegistrySubsystem* Registry = MakeAquaticTestRegistry(Table);
    if (!TestNotNull(TEXT("Registry constructed"), Registry)) return false;

    FHarvestContext Context;
    FRandomStream Rng(1);
    const FName Picked = Registry->GetRandomResourceForAquaticBiome(Cell, Context, Rng);
    TestEqual(TEXT("No aquatic candidates registered -- aquatic pick returns None, not the land-only row"),
        Picked, NAME_None);

    // Тот же ингредиент по-прежнему собирается обычным (земляным) путём --
    // bGrowsOnWater=false не выкидывает его из AllowedBiomes-пула.
    const FName LandPicked = Registry->GetRandomResourceForBiome(Cell, Context, Rng);
    TestEqual(TEXT("Land pool still finds it"), LandPicked, FName(TEXT("Осока")));

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistIngredientAquatic_GrowsOnWaterIngredientIsPickedForWaterAndStillForLand,
    "Herbalist.IngredientAquatic.GrowsOnWaterIngredientIsPickedForWaterAndStillForLand",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistIngredientAquatic_GrowsOnWaterIngredientIsPickedForWaterAndStillForLand::RunTest(const FString& Parameters)
{
    UDataTable* Table = MakeAquaticTestTable();
    FGridCell Cell = MakeAquaticTestCell(EBiomeType::Floodplain);

    FIngredientTableRow LilyRow;
    LilyRow.AllowedBiomes = { EBiomeType::Floodplain };
    LilyRow.BaseState = Cell.State;
    LilyRow.bGrowsOnWater = true;
    Table->AddRow(FName(TEXT("Кувшинка")), LilyRow);

    UIngredientRegistrySubsystem* Registry = MakeAquaticTestRegistry(Table);
    if (!TestNotNull(TEXT("Registry constructed"), Registry)) return false;

    FHarvestContext Context;
    FRandomStream RngWater(1);
    const FName PickedForWater = Registry->GetRandomResourceForAquaticBiome(Cell, Context, RngWater);
    TestEqual(TEXT("bGrowsOnWater=true ingredient is the only, and thus guaranteed, aquatic pick"),
        PickedForWater, FName(TEXT("Кувшинка")));

    // "Разрешены к размещению на поверхности воды" -- дополнительно, не
    // вместо земляного пула: тот же ингредиент по-прежнему собирается и
    // обычным (земляным) путём через AllowedBiomes.
    FRandomStream RngLand(1);
    const FName PickedForLand = Registry->GetRandomResourceForBiome(Cell, Context, RngLand);
    TestEqual(TEXT("Still picked through the ordinary land pool too (additive, not exclusive)"),
        PickedForLand, FName(TEXT("Кувшинка")));

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistIngredientAquatic_AquaticPoolIsPerBiomeNotGlobal,
    "Herbalist.IngredientAquatic.AquaticPoolIsPerBiomeNotGlobal",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistIngredientAquatic_AquaticPoolIsPerBiomeNotGlobal::RunTest(const FString& Parameters)
{
    // Кувшинка растёт в Речной пойме -- клетка-Болото не должна её получить
    // через аквапул, даже если обе "водные".
    UDataTable* Table = MakeAquaticTestTable();
    FGridCell FloodplainCell = MakeAquaticTestCell(EBiomeType::Floodplain);
    FGridCell BogCell = MakeAquaticTestCell(EBiomeType::Bog);

    FIngredientTableRow LilyRow;
    LilyRow.AllowedBiomes = { EBiomeType::Floodplain };
    LilyRow.BaseState = FloodplainCell.State;
    LilyRow.bGrowsOnWater = true;
    Table->AddRow(FName(TEXT("Кувшинка")), LilyRow);

    UIngredientRegistrySubsystem* Registry = MakeAquaticTestRegistry(Table);
    if (!TestNotNull(TEXT("Registry constructed"), Registry)) return false;

    FHarvestContext Context;
    FRandomStream Rng(1);
    const FName PickedForBog = Registry->GetRandomResourceForAquaticBiome(BogCell, Context, Rng);
    TestEqual(TEXT("Aquatic candidate registered only for Floodplain does not leak into Bog's aquatic pool"),
        PickedForBog, NAME_None);

    return true;
}

#endif // WITH_AUTOMATION_TESTS
