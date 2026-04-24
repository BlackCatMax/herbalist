#include "Core/Data/IngredientRegistry.h"
#include "Core/Data/IngredientTableRow.h"
#include "Core/Pipeline/AlchemyTypes.h"
#include "Misc/AutomationTest.h"
#include "Engine/DataTable.h"

#if WITH_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistRegistry_UnknownReturnsUnknown,
    "Herbalist.Registry.UnknownReturnsUnknown",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistRegistry_UnknownReturnsUnknown::RunTest(const FString& Parameters)
{
    FIngredientRegistry::Reset();

    UDataTable* Table = NewObject<UDataTable>();
    Table->RowStruct = FIngredientTableRow::StaticStruct();
    FIngredientRegistry::Initialize(Table);

    EIngredientClass Result = FIngredientRegistry::Classify(FName(TEXT("NonExistent")));
    TestEqual(TEXT("Unknown ingredient -> Unknown class"), Result, EIngredientClass::Unknown);

    FIngredientRegistry::Reset();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistRegistry_KnownWaterClassifiesCorrectly,
    "Herbalist.Registry.KnownWaterClassifiesCorrectly",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistRegistry_KnownWaterClassifiesCorrectly::RunTest(const FString& Parameters)
{
    FIngredientRegistry::Reset();

    UDataTable* Table = NewObject<UDataTable>();
    Table->RowStruct = FIngredientTableRow::StaticStruct();

    FIngredientTableRow Row;
    Row.Class = EIngredientClass::Water;
    Table->AddRow(FName(TEXT("Water_Spring")), Row);

    FIngredientRegistry::Initialize(Table);

    EIngredientClass Result = FIngredientRegistry::Classify(FName(TEXT("Water_Spring")));
    TestEqual(TEXT("Water_Spring -> Water"), Result, EIngredientClass::Water);

    FIngredientRegistry::Reset();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistRegistry_FungusClassifiesCorrectly,
    "Herbalist.Registry.FungusClassifiesCorrectly",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistRegistry_FungusClassifiesCorrectly::RunTest(const FString& Parameters)
{
    FIngredientRegistry::Reset();

    UDataTable* Table = NewObject<UDataTable>();
    Table->RowStruct = FIngredientTableRow::StaticStruct();

    FIngredientTableRow Row;
    Row.Class = EIngredientClass::Fungus;
    Table->AddRow(FName(TEXT("Deathcap")), Row);

    FIngredientRegistry::Initialize(Table);

    EIngredientClass Result = FIngredientRegistry::Classify(FName(TEXT("Deathcap")));
    TestEqual(TEXT("Deathcap -> Fungus"), Result, EIngredientClass::Fungus);

    FIngredientRegistry::Reset();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistRegistry_EssenceClassifiesCorrectly,
    "Herbalist.Registry.EssenceClassifiesCorrectly",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistRegistry_EssenceClassifiesCorrectly::RunTest(const FString& Parameters)
{
    FIngredientRegistry::Reset();

    UDataTable* Table = NewObject<UDataTable>();
    Table->RowStruct = FIngredientTableRow::StaticStruct();

    FIngredientTableRow Row;
    Row.Class = EIngredientClass::Essence;
    Table->AddRow(FName(TEXT("FireElementalEssence")), Row);

    FIngredientRegistry::Initialize(Table);

    EIngredientClass Result = FIngredientRegistry::Classify(FName(TEXT("FireElementalEssence")));
    TestEqual(TEXT("FireElementalEssence -> Essence"), Result, EIngredientClass::Essence);

    FIngredientRegistry::Reset();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistAtom_UnknownClassCreatesValidAtom,
    "Herbalist.Atom.UnknownClassCreatesValidAtom",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistAtom_UnknownClassCreatesValidAtom::RunTest(const FString& Parameters)
{
    FIngredientRegistry::Reset();

    UDataTable* Table = NewObject<UDataTable>();
    Table->RowStruct = FIngredientTableRow::StaticStruct();
    FIngredientRegistry::Initialize(Table);

    FAlchemyAtom Atom(FName(TEXT("MysterySubstance")), false, FRealState(),
                      EAtomOrigin::Harvest, 0.5f, 100.0f);

    TestEqual(TEXT("Atom Class == Unknown"), Atom.Class, EIngredientClass::Unknown);
    TestTrue(TEXT("Atom UID is valid"), Atom.AtomUID.IsValid());
    TestEqual(TEXT("Atom Origin preserved"), Atom.OriginContext, EAtomOrigin::Harvest);
    TestEqual(TEXT("Atom SourceID preserved"), Atom.SourceID, FName(TEXT("MysterySubstance")));

    FIngredientRegistry::Reset();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistRegistry_IsKnownDistinguishesKnownFromUnknown,
    "Herbalist.Registry.IsKnownDistinguishesKnownFromUnknown",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistRegistry_IsKnownDistinguishesKnownFromUnknown::RunTest(const FString& Parameters)
{
    FIngredientRegistry::Reset();

    UDataTable* Table = NewObject<UDataTable>();
    Table->RowStruct = FIngredientTableRow::StaticStruct();

    FIngredientTableRow Row;
    Row.Class = EIngredientClass::Plant;
    Table->AddRow(FName(TEXT("Nightshade")), Row);

    FIngredientRegistry::Initialize(Table);

    TestTrue(TEXT("Nightshade is known"), FIngredientRegistry::IsKnown(FName(TEXT("Nightshade"))));
    TestFalse(TEXT("NonExistent is not known"), FIngredientRegistry::IsKnown(FName(TEXT("NonExistent"))));

    FIngredientRegistry::Reset();
    return true;
}

#endif // WITH_AUTOMATION_TESTS