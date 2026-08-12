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

#endif // WITH_AUTOMATION_TESTS
