#include "Core/Subsystems/IngredientRegistrySubsystem.h"
#include "Core/Data/IngredientTableRow.h"
#include "Misc/AutomationTest.h"
#include "Engine/DataTable.h"
#include "Engine/GameInstance.h"
#include "Engine/Engine.h"

// Раньше классификация ингредиентов жила в статическом FIngredientRegistry
// (Core/Data/IngredientRegistry.h) — этого файла давно нет в Source/, он ушёл
// вместе со всем кластером Core/Pipeline/* при переходе на PipelineV2. Тесты
// ниже переписаны на актуальный UIngredientRegistrySubsystem с тем же составом
// проверок; тест FAlchemyAtom убран — этого типа сегодня не существует, и
// придумывать его заново только ради теста не было запрошено.

#if WITH_AUTOMATION_TESTS

namespace
{
    /**
     * UIngredientRegistrySubsystem наследует UGameInstanceSubsystem, у которого
     * ClassWithin = UGameInstance. Поэтому NewObject<>() без Outer недопустим:
     * движок выдаёт ensure «created in invalid Outer /Script/CoreUObject.Package»
     * и прогон падает. Создаём временный UGameInstance как Outer — проверяемая
     * логика (LoadFromDataTable/Classify/IsKnown) работает с собственной картой
     * Rows и от GameInstance никак не зависит.
     */
    UIngredientRegistrySubsystem* MakeRegistry(UDataTable* Table)
    {
        UGameInstance* OwnerGameInstance = NewObject<UGameInstance>(GEngine);
        UIngredientRegistrySubsystem* Registry = NewObject<UIngredientRegistrySubsystem>(OwnerGameInstance);
        Registry->LoadFromDataTable(Table);
        return Registry;
    }

    UDataTable* MakeIngredientTable()
    {
        UDataTable* Table = NewObject<UDataTable>();
        Table->RowStruct = FIngredientTableRow::StaticStruct();
        return Table;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistRegistry_UnknownReturnsUnknown,
    "Herbalist.Registry.UnknownReturnsUnknown",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistRegistry_UnknownReturnsUnknown::RunTest(const FString& Parameters)
{
    UIngredientRegistrySubsystem* Registry = MakeRegistry(MakeIngredientTable());

    EIngredientClass Result = Registry->Classify(FName(TEXT("NonExistent")));
    TestEqual(TEXT("Unknown ingredient -> Unknown class"), Result, EIngredientClass::Unknown);

    Registry->Reset();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistRegistry_KnownWaterClassifiesCorrectly,
    "Herbalist.Registry.KnownWaterClassifiesCorrectly",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistRegistry_KnownWaterClassifiesCorrectly::RunTest(const FString& Parameters)
{
    UDataTable* Table = MakeIngredientTable();

    FIngredientTableRow Row;
    Row.Class = EIngredientClass::Water;
    Table->AddRow(FName(TEXT("Water_Spring")), Row);

    UIngredientRegistrySubsystem* Registry = MakeRegistry(Table);

    EIngredientClass Result = Registry->Classify(FName(TEXT("Water_Spring")));
    TestEqual(TEXT("Water_Spring -> Water"), Result, EIngredientClass::Water);

    Registry->Reset();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistRegistry_FungusClassifiesCorrectly,
    "Herbalist.Registry.FungusClassifiesCorrectly",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistRegistry_FungusClassifiesCorrectly::RunTest(const FString& Parameters)
{
    UDataTable* Table = MakeIngredientTable();

    FIngredientTableRow Row;
    Row.Class = EIngredientClass::Fungus;
    Table->AddRow(FName(TEXT("Deathcap")), Row);

    UIngredientRegistrySubsystem* Registry = MakeRegistry(Table);

    EIngredientClass Result = Registry->Classify(FName(TEXT("Deathcap")));
    TestEqual(TEXT("Deathcap -> Fungus"), Result, EIngredientClass::Fungus);

    Registry->Reset();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistRegistry_EssenceClassifiesCorrectly,
    "Herbalist.Registry.EssenceClassifiesCorrectly",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistRegistry_EssenceClassifiesCorrectly::RunTest(const FString& Parameters)
{
    UDataTable* Table = MakeIngredientTable();

    FIngredientTableRow Row;
    Row.Class = EIngredientClass::Essence;
    Table->AddRow(FName(TEXT("FireElementalEssence")), Row);

    UIngredientRegistrySubsystem* Registry = MakeRegistry(Table);

    EIngredientClass Result = Registry->Classify(FName(TEXT("FireElementalEssence")));
    TestEqual(TEXT("FireElementalEssence -> Essence"), Result, EIngredientClass::Essence);

    Registry->Reset();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistRegistry_IsKnownDistinguishesKnownFromUnknown,
    "Herbalist.Registry.IsKnownDistinguishesKnownFromUnknown",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistRegistry_IsKnownDistinguishesKnownFromUnknown::RunTest(const FString& Parameters)
{
    UDataTable* Table = MakeIngredientTable();

    FIngredientTableRow Row;
    Row.Class = EIngredientClass::Plant;
    Table->AddRow(FName(TEXT("Nightshade")), Row);

    UIngredientRegistrySubsystem* Registry = MakeRegistry(Table);

    TestTrue(TEXT("Nightshade is known"), Registry->IsKnown(FName(TEXT("Nightshade"))));
    TestFalse(TEXT("NonExistent is not known"), Registry->IsKnown(FName(TEXT("NonExistent"))));

    Registry->Reset();
    return true;
}

// Регрессия для DESIGN_World_State.md §15 (звено 3): GetRandomResourceForBiome
// должен смещать выбор к кандидату, чей BaseState ближе к состоянию клетки, а не
// раздавать шансы поровну по RarityWeight, как было до этой правки. Проверяем
// направление эффекта (CloseCount заметно больше FarCount), а не точное
// соотношение — оно завязано на IngredientSuitabilityFalloff и может тюниться
// отдельно от этой проверки.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistRegistry_SuitabilityBiasesTowardCloserBaseState,
    "Herbalist.Registry.SuitabilityBiasesTowardCloserBaseState",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistRegistry_SuitabilityBiasesTowardCloserBaseState::RunTest(const FString& Parameters)
{
    UDataTable* Table = MakeIngredientTable();

    // Белокрыльник: почти точная копия эталона испорченного Болота.
    FIngredientTableRow CloseRow;
    CloseRow.AllowedBiomes = { EBiomeType::Bog };
    CloseRow.RarityWeight = 1;
    CloseRow.BaseState.Meta.Purity = 0.35f;
    CloseRow.BaseState.Meta.Corruption = 0.70f;
    Table->AddRow(FName(TEXT("Belokrylnik")), CloseRow);

    // Сфагнум: противоположный полюс того же биома (см. таблицу в §15).
    FIngredientTableRow FarRow;
    FarRow.AllowedBiomes = { EBiomeType::Bog };
    FarRow.RarityWeight = 1;
    FarRow.BaseState.Meta.Purity = 0.80f;
    FarRow.BaseState.Meta.Corruption = 0.15f;
    Table->AddRow(FName(TEXT("Sphagnum")), FarRow);

    UIngredientRegistrySubsystem* Registry = MakeRegistry(Table);

    FGridCell Cell;
    Cell.Biome = EBiomeType::Bog;
    Cell.State.Meta.Purity = 0.35f;
    Cell.State.Meta.Corruption = 0.70f;   // совпадает с Белокрыльником, не со Сфагнумом

    // Оба ряда без AllowedSeasons/HarvestTimeWindow/луны/погоды (по умолчанию
    // "без ограничения") — окна из этого теста не участвуют, проверяем только
    // смещение по дистанции State, как и раньше.
    FHarvestContext Context;

    FRandomStream Rng(12345);
    int32 CloseCount = 0, FarCount = 0;
    const int32 Trials = 2000;
    for (int32 i = 0; i < Trials; ++i)
    {
        FName Picked = Registry->GetRandomResourceForBiome(Cell, Context, Rng);
        if (Picked == FName(TEXT("Belokrylnik"))) ++CloseCount;
        else if (Picked == FName(TEXT("Sphagnum"))) ++FarCount;
    }

    TestEqual(TEXT("Every trial picked one of the two candidates"), CloseCount + FarCount, Trials);
    TestTrue(TEXT("Candidate matching cell state is picked at least twice as often"), CloseCount > FarCount * 2);

    Registry->Reset();
    return true;
}

// Сад (DESIGN_Community_And_Homestead.md §2.4, 2026-08-31): пристройка
// подделывает нишу, не переносит биом клетки целиком — GetRandomResourceForNiche
// должен находить кандидата по EGardenNiche, ИГНОРИРУЯ то, что AllowedBiomes
// этого кандидата не совпадает с биомом клетки вообще (Кувшинка/Водоём растёт
// в Речной пойме, но грядка стоит в Степи — постройка это и обходит).
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistRegistry_NicheCandidatesIgnoreAllowedBiomes,
    "Herbalist.Registry.NicheCandidatesIgnoreAllowedBiomes",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistRegistry_NicheCandidatesIgnoreAllowedBiomes::RunTest(const FString& Parameters)
{
    UDataTable* Table = MakeIngredientTable();

    // Речная трава, физически растущая только в Пойме -- но помечена как
    // пристройка "Водоём".
    FIngredientTableRow PondRow;
    PondRow.AllowedBiomes = { EBiomeType::Floodplain };
    PondRow.RarityWeight = 1;
    PondRow.GardenNiche = EGardenNiche::Pond;
    Table->AddRow(FName(TEXT("WaterLily")), PondRow);

    // Обычная степная трава без пристройки -- не должна попадать в выборку
    // по нише, даже если растёт в том же биоме, что клетка ниже.
    FIngredientTableRow SteppeRow;
    SteppeRow.AllowedBiomes = { EBiomeType::Steppe };
    SteppeRow.RarityWeight = 1;
    Table->AddRow(FName(TEXT("Wormwood")), SteppeRow);

    UIngredientRegistrySubsystem* Registry = MakeRegistry(Table);

    // Клетка физически в Степи -- пристройка "Водоём" стоит поверх неё.
    FGridCell Cell;
    Cell.Biome = EBiomeType::Steppe;
    FHarvestContext Context;
    FRandomStream Rng(777);

    const FName Picked = Registry->GetRandomResourceForNiche(Cell, EGardenNiche::Pond, Context, Rng);
    TestEqual(TEXT("Niche query returns the pond-tagged plant despite the cell being Steppe, not Floodplain"),
        Picked, FName(TEXT("WaterLily")));

    Registry->Reset();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistRegistry_NicheNoneYieldsNoCandidate,
    "Herbalist.Registry.NicheNoneYieldsNoCandidate",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistRegistry_NicheNoneYieldsNoCandidate::RunTest(const FString& Parameters)
{
    UDataTable* Table = MakeIngredientTable();
    FIngredientTableRow Row;
    Row.AllowedBiomes = { EBiomeType::Steppe };
    Row.GardenNiche = EGardenNiche::SunnyBed;
    Table->AddRow(FName(TEXT("SomeHerb")), Row);

    UIngredientRegistrySubsystem* Registry = MakeRegistry(Table);

    FGridCell Cell;
    Cell.Biome = EBiomeType::Steppe;
    FHarvestContext Context;
    FRandomStream Rng(778);

    const FName Picked = Registry->GetRandomResourceForNiche(Cell, EGardenNiche::None, Context, Rng);
    TestTrue(TEXT("Querying EGardenNiche::None always returns NAME_None, not an arbitrary candidate"), Picked.IsNone());

    Registry->Reset();
    return true;
}

#endif // WITH_AUTOMATION_TESTS
