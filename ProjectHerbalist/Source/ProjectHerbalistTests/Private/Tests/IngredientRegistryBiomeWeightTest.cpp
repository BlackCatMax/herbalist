// Source/ProjectHerbalistTests/Private/Tests/IngredientRegistryBiomeWeightTest.cpp
//
// PCG-биомы (2026-08-31), последний шаг: GetRandomResourceForBiome теперь
// сливает кандидатов из всех долей Cell.BiomeWeights, домножая базовый
// вес каждого на долю клетки, вместо одного точного совпадения по
// Cell.Biome. Тот же приём CountBiomeWeightPicks по многим прогонам, что уже в
// IngredientHarvestWindowTest.cpp.
//
// Тест 2 ниже -- прямой регресс-пин на баг, найденный на этапе
// планирования: PickWeightedResource раньше принимала веса как
// TArray<int32>, домножение "1 (дефолтный RarityWeight) * 0.5" усекалось
// бы до 0 -- ингредиент с обычным весом стал бы невыбираемым на любом
// стыке двух регионов, без единой ошибки в логе. Без правки на
// TArray<float> этот тест провалился бы однозначно (B никогда не выпадет).

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
    UIngredientRegistrySubsystem* MakeBiomeWeightTestRegistry(UDataTable* Table)
    {
        UGameInstance* OwnerGameInstance = NewObject<UGameInstance>(GEngine);
        UIngredientRegistrySubsystem* Registry = NewObject<UIngredientRegistrySubsystem>(OwnerGameInstance);
        Registry->LoadFromDataTable(Table);
        return Registry;
    }

    UDataTable* MakeBiomeWeightTestTable()
    {
        UDataTable* Table = NewObject<UDataTable>();
        Table->RowStruct = FIngredientTableRow::StaticStruct();
        return Table;
    }

    FGridCell MakeBiomeWeightTestCell(EBiomeType Biome)
    {
        FGridCell Cell;
        Cell.X = 5; Cell.Y = 5;
        Cell.Biome = Biome;
        // Suitability=1 для любой карточки, чей BaseState совпадает с этим
        // State -- изолирует эффект веса биома от эффекта дистанции
        // (тот же приём, что IngredientHarvestWindowTest.cpp).
        Cell.State.Magnitude = 0.5f;
        Cell.State.Direction.Body = 0.25f; Cell.State.Direction.Mind = 0.25f;
        Cell.State.Direction.Spirit = 0.25f; Cell.State.Direction.Nature = 0.25f;
        return Cell;
    }

    FIngredientTableRow MakeExclusiveRow(EBiomeType Biome, const FGridCell& MatchingState, int32 RarityWeight = 1)
    {
        FIngredientTableRow Row;
        Row.AllowedBiomes = { Biome };
        Row.BaseState = MatchingState.State;
        Row.RarityWeight = RarityWeight;
        return Row;
    }

    void CountBiomeWeightPicks(UIngredientRegistrySubsystem* Registry, const FGridCell& Cell, const FHarvestContext& Context,
        FName NameA, FName NameB, int32& OutCountA, int32& OutCountB)
    {
        FRandomStream Rng(4242);
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistIngredientBiomeWeight_EmptyWeightsMatchesSingleBiomeBehavior,
    "Herbalist.IngredientBiomeWeight.EmptyWeightsMatchesSingleBiomeBehavior",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistIngredientBiomeWeight_EmptyWeightsMatchesSingleBiomeBehavior::RunTest(const FString& Parameters)
{
    // Обратная совместимость -- Cell.BiomeWeights пуст, только Cell.Biome
    // задан (ровно то, что делают IngredientHarvestWindowTest.cpp/
    // IngredientRegistryTest.cpp, 8 существующих тестов).
    UDataTable* Table = MakeBiomeWeightTestTable();
    FGridCell Cell = MakeBiomeWeightTestCell(EBiomeType::Bog);
    // Cell.BiomeWeights не трогаем -- остаётся пустым по умолчанию.

    Table->AddRow(FName(TEXT("OnlyCandidate")), MakeExclusiveRow(EBiomeType::Bog, Cell));

    UIngredientRegistrySubsystem* Registry = MakeBiomeWeightTestRegistry(Table);
    if (!TestNotNull(TEXT("Registry constructed"), Registry)) return false;

    FHarvestContext Context;
    FRandomStream Rng(1);
    const FName Picked = Registry->GetRandomResourceForBiome(Cell, Context, Rng);
    TestEqual(TEXT("Single candidate for Cell.Biome picked via the empty-BiomeWeights fallback"), Picked, FName(TEXT("OnlyCandidate")));

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistIngredientBiomeWeight_EqualSplitPicksBothDefaultRarityIngredientsRoughlyEvenly,
    "Herbalist.IngredientBiomeWeight.EqualSplitPicksBothDefaultRarityIngredientsRoughlyEvenly",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistIngredientBiomeWeight_EqualSplitPicksBothDefaultRarityIngredientsRoughlyEvenly::RunTest(const FString& Parameters)
{
    // Регресс-пин на int32->float: RarityWeight ДЕФОЛТНЫЙ (1) у обеих
    // карточек -- без правки B (1 * 0.5, усечённое до 0 в TArray<int32>)
    // не выпал бы вообще ни разу за 2000 прогонов.
    UDataTable* Table = MakeBiomeWeightTestTable();
    FGridCell Cell = MakeBiomeWeightTestCell(EBiomeType::Bog); // Cell.Biome тут не смотрится -- решает BiomeWeights
    Cell.BiomeWeights.Add(FBiomeWeightEntry{ EBiomeType::Bog, 0.5f });
    Cell.BiomeWeights.Add(FBiomeWeightEntry{ EBiomeType::Taiga, 0.5f });

    Table->AddRow(FName(TEXT("BogHerb")), MakeExclusiveRow(EBiomeType::Bog, Cell, /*RarityWeight=*/1));
    Table->AddRow(FName(TEXT("TaigaHerb")), MakeExclusiveRow(EBiomeType::Taiga, Cell, /*RarityWeight=*/1));

    UIngredientRegistrySubsystem* Registry = MakeBiomeWeightTestRegistry(Table);
    if (!TestNotNull(TEXT("Registry constructed"), Registry)) return false;

    FHarvestContext Context;
    int32 CountA = 0, CountB = 0;
    CountBiomeWeightPicks(Registry, Cell, Context, FName(TEXT("BogHerb")), FName(TEXT("TaigaHerb")), CountA, CountB);

    TestTrue(TEXT("BogHerb (0.5 share) is picked a meaningful number of times"), CountA > 500);
    TestTrue(TEXT("TaigaHerb (0.5 share, default RarityWeight=1) is ALSO picked -- would be zero under the int32-truncation bug"), CountB > 500);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistIngredientBiomeWeight_SkewedSplitFavorsHigherShareButNotExclusively,
    "Herbalist.IngredientBiomeWeight.SkewedSplitFavorsHigherShareButNotExclusively",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistIngredientBiomeWeight_SkewedSplitFavorsHigherShareButNotExclusively::RunTest(const FString& Parameters)
{
    UDataTable* Table = MakeBiomeWeightTestTable();
    FGridCell Cell = MakeBiomeWeightTestCell(EBiomeType::Bog);
    Cell.BiomeWeights.Add(FBiomeWeightEntry{ EBiomeType::Bog, 0.9f });
    Cell.BiomeWeights.Add(FBiomeWeightEntry{ EBiomeType::Taiga, 0.1f });

    Table->AddRow(FName(TEXT("BogHerb")), MakeExclusiveRow(EBiomeType::Bog, Cell));
    Table->AddRow(FName(TEXT("TaigaHerb")), MakeExclusiveRow(EBiomeType::Taiga, Cell));

    UIngredientRegistrySubsystem* Registry = MakeBiomeWeightTestRegistry(Table);
    if (!TestNotNull(TEXT("Registry constructed"), Registry)) return false;

    FHarvestContext Context;
    int32 CountA = 0, CountB = 0;
    CountBiomeWeightPicks(Registry, Cell, Context, FName(TEXT("BogHerb")), FName(TEXT("TaigaHerb")), CountA, CountB);

    TestTrue(TEXT("Higher-share biome (0.9) is picked substantially more often"), CountA > CountB * 3);
    TestTrue(TEXT("Lower-share biome (0.1) still gets picked sometimes, not starved to zero"), CountB > 0);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistIngredientBiomeWeight_SameIngredientInBothBiomesAddsProbabilityMass,
    "Herbalist.IngredientBiomeWeight.SameIngredientInBothBiomesAddsProbabilityMass",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistIngredientBiomeWeight_SameIngredientInBothBiomesAddsProbabilityMass::RunTest(const FString& Parameters)
{
    // Один и тот же кандидат в обоих биомах -- НЕ дедуплицируется
    // намеренно, его вероятностная масса складывается. Third — уникальный
    // кандидат только в Taiga, сравниваем относительно него.
    UDataTable* Table = MakeBiomeWeightTestTable();
    FGridCell Cell = MakeBiomeWeightTestCell(EBiomeType::Bog);
    Cell.BiomeWeights.Add(FBiomeWeightEntry{ EBiomeType::Bog, 0.5f });
    Cell.BiomeWeights.Add(FBiomeWeightEntry{ EBiomeType::Taiga, 0.5f });

    FIngredientTableRow SharedRow = MakeExclusiveRow(EBiomeType::Bog, Cell);
    SharedRow.AllowedBiomes = { EBiomeType::Bog, EBiomeType::Taiga };
    Table->AddRow(FName(TEXT("SharedHerb")), SharedRow);
    Table->AddRow(FName(TEXT("TaigaOnlyHerb")), MakeExclusiveRow(EBiomeType::Taiga, Cell));

    UIngredientRegistrySubsystem* Registry = MakeBiomeWeightTestRegistry(Table);
    if (!TestNotNull(TEXT("Registry constructed"), Registry)) return false;

    FHarvestContext Context;
    int32 CountShared = 0, CountTaigaOnly = 0;
    CountBiomeWeightPicks(Registry, Cell, Context, FName(TEXT("SharedHerb")), FName(TEXT("TaigaOnlyHerb")), CountShared, CountTaigaOnly);

    // SharedHerb получает вклад из ОБЕИХ долей (Bog 0.5 + Taiga 0.5 = 1.0
    // суммарной массы), TaigaOnlyHerb -- только из доли Taiga (0.5) --
    // Shared должен выбираться заметно чаще, не поровну.
    TestTrue(TEXT("A candidate present in both overlapping biomes is picked more often than one present in only one"),
        CountShared > CountTaigaOnly * 1.5f);

    return true;
}

#endif // WITH_AUTOMATION_TESTS
