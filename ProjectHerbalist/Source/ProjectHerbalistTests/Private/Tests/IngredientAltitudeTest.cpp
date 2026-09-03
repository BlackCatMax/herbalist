// Source/ProjectHerbalistTests/Private/Tests/IngredientAltitudeTest.cpp
//
// Высотный пояс произрастания (2026-09-03, прямой запрос: "нужна
// регулировка по высоте произрастания, как в Калисто").
//
// Ключевое отличие от окон сезона/времени/луны: те намеренно НИКОГДА не
// обнуляют вес ("не в сезон найти труднее, но можно"), а высота -- жёсткая
// маска. Выше границы леса трава не растёт реже, она не растёт. Тесты
// держат именно это различие, плюс мягкий край и обратную совместимость.

#include "Core/Subsystems/IngredientRegistrySubsystem.h"
#include "Core/Data/IngredientTableRow.h"
#include "Core/Types/HerbalistCoreTypes.h"
#include "Core/Types/BiomeTypes.h"
#include "Misc/AutomationTest.h"
#include "Engine/DataTable.h"
#include "Engine/GameInstance.h"

#if WITH_AUTOMATION_TESTS

namespace
{
    const FName LowlandHerb(TEXT("AltLowland"));   // пояс 0..100 м
    const FName AnyHeightHerb(TEXT("AltAnywhere")); // без пояса вовсе

    UIngredientRegistrySubsystem* MakeAltitudeRegistry()
    {
        UGameInstance* Owner = NewObject<UGameInstance>(GEngine);
        UIngredientRegistrySubsystem* Registry = NewObject<UIngredientRegistrySubsystem>(Owner);

        UDataTable* Table = NewObject<UDataTable>();
        Table->RowStruct = FIngredientTableRow::StaticStruct();

        FIngredientTableRow Lowland;
        Lowland.AllowedBiomes.Add(EBiomeType::Taiga);
        Lowland.bUseAltitudeRange = true;
        Lowland.MinAltitudeMeters = 0.0f;
        Lowland.MaxAltitudeMeters = 100.0f;
        Lowland.AltitudeFalloffMeters = 0.0f;   // резкая граница, чтобы тест был однозначным
        Table->AddRow(LowlandHerb, Lowland);

        FIngredientTableRow Anywhere;
        Anywhere.AllowedBiomes.Add(EBiomeType::Taiga);
        Table->AddRow(AnyHeightHerb, Anywhere);

        Registry->LoadFromDataTable(Table);
        return Registry;
    }

    FGridCell MakeTaigaCell()
    {
        FGridCell Cell;
        Cell.X = 0; Cell.Y = 0;
        Cell.Biome = EBiomeType::Taiga;
        Cell.BiomeWeights.Add({ EBiomeType::Taiga, 1.0f });
        return Cell;
    }

    FHarvestContext MakeContextAt(float AltitudeMeters, bool bKnown = true)
    {
        FHarvestContext Context;
        Context.AltitudeMeters = AltitudeMeters;
        Context.bAltitudeKnown = bKnown;
        return Context;
    }

    // Сколько раз из N роллов выпала конкретная карточка.
    int32 CountRolls(UIngredientRegistrySubsystem* Registry, const FGridCell& Cell,
        const FHarvestContext& Context, FName Wanted, int32 Rolls = 200)
    {
        int32 Hits = 0;
        FRandomStream Rng(20260903);
        for (int32 i = 0; i < Rolls; ++i)
        {
            if (Registry->GetRandomResourceForBiome(Cell, Context, Rng) == Wanted) ++Hits;
        }
        return Hits;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistAltitude_OutsideTheBandTheHerbNeverAppears,
    "Herbalist.Altitude.OutsideTheBandTheHerbNeverAppears",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistAltitude_OutsideTheBandTheHerbNeverAppears::RunTest(const FString& Parameters)
{
    UIngredientRegistrySubsystem* Registry = MakeAltitudeRegistry();
    if (!TestNotNull(TEXT("Registry built"), Registry)) return false;

    const FGridCell Cell = MakeTaigaCell();

    // Внутри пояса низинная трава встречается.
    TestTrue(TEXT("Inside its band the lowland herb does appear"),
        CountRolls(Registry, Cell, MakeContextAt(50.0f), LowlandHerb) > 0);

    // Выше пояса -- НИ РАЗУ. Это и есть разница с сезонными окнами: там
    // множитель падает, но остаётся положительным, здесь уходит в ноль.
    TestEqual(TEXT("Above its band the lowland herb never appears at all"),
        CountRolls(Registry, Cell, MakeContextAt(400.0f), LowlandHerb), 0);

    // При этом карточка без пояса растёт на любой высоте -- высотный гейт
    // не должен вычищать всё подряд.
    TestTrue(TEXT("A card without an altitude band still grows high up"),
        CountRolls(Registry, Cell, MakeContextAt(400.0f), AnyHeightHerb) > 0);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistAltitude_UnknownAltitudeDisablesTheGate,
    "Herbalist.Altitude.UnknownAltitudeDisablesTheGate",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistAltitude_UnknownAltitudeDisablesTheGate::RunTest(const FString& Parameters)
{
    UIngredientRegistrySubsystem* Registry = MakeAltitudeRegistry();
    if (!TestNotNull(TEXT("Registry built"), Registry)) return false;

    const FGridCell Cell = MakeTaigaCell();

    // Без ландшафта GetCellHeight отдаёт 0 для всей сетки -- гейт по такой
    // "высоте" был бы вымыслом и молча вычистил бы половину таблицы. Когда
    // высота неизвестна, пояс не применяется вовсе.
    TestTrue(TEXT("With unknown altitude the banded herb is not filtered out"),
        CountRolls(Registry, Cell, MakeContextAt(400.0f, /*bKnown=*/false), LowlandHerb) > 0);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistAltitude_FalloffMakesTheEdgeGradual,
    "Herbalist.Altitude.FalloffMakesTheEdgeGradual",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistAltitude_FalloffMakesTheEdgeGradual::RunTest(const FString& Parameters)
{
    UGameInstance* Owner = NewObject<UGameInstance>(GEngine);
    UIngredientRegistrySubsystem* Registry = NewObject<UIngredientRegistrySubsystem>(Owner);

    UDataTable* Table = NewObject<UDataTable>();
    Table->RowStruct = FIngredientTableRow::StaticStruct();

    FIngredientTableRow Banded;
    Banded.AllowedBiomes.Add(EBiomeType::Taiga);
    Banded.bUseAltitudeRange = true;
    Banded.MinAltitudeMeters = 0.0f;
    Banded.MaxAltitudeMeters = 100.0f;
    Banded.AltitudeFalloffMeters = 100.0f;   // широкий мягкий край
    Table->AddRow(LowlandHerb, Banded);

    FIngredientTableRow Anywhere;
    Anywhere.AllowedBiomes.Add(EBiomeType::Taiga);
    Table->AddRow(AnyHeightHerb, Anywhere);
    Registry->LoadFromDataTable(Table);

    const FGridCell Cell = MakeTaigaCell();

    // В полосе затухания карточка ещё встречается, но реже, чем внутри
    // пояса, а за полосой -- уже нет вовсе.
    const int32 Inside = CountRolls(Registry, Cell, MakeContextAt(50.0f), LowlandHerb);
    const int32 Edge   = CountRolls(Registry, Cell, MakeContextAt(150.0f), LowlandHerb);
    const int32 Beyond = CountRolls(Registry, Cell, MakeContextAt(250.0f), LowlandHerb);

    TestTrue(FString::Printf(TEXT("Inside the band (%d) beats the fade zone (%d)"), Inside, Edge), Inside > Edge);
    TestTrue(FString::Printf(TEXT("The fade zone (%d) still yields something"), Edge), Edge > 0);
    TestEqual(FString::Printf(TEXT("Past the fade (%d) nothing at all"), Beyond), Beyond, 0);

    return true;
}

// Регрессия 2026-09-03: пользователь настроил "пояс, как в плане проверки"
// -- одна строка в DT_IngredientClass, Use Altitude Range=true -- и вне
// пояса растение всё равно появлялось. Причина не в самом гейте (тесты
// выше его и держат), а в фолбэке PickWeightedResource: "TotalWeight ~ 0
// -> вернуть Candidates[0] всё равно". Фолбэк придуман для МЯГКИХ гейтов
// (сезон/время/луна/погода никогда не гасят вес до истинного нуля), но
// высота -- единственный по-настоящему жёсткий множитель, и когда в биоме
// РОВНО ОДИН кандидат и он же гейтится по высоте, TotalWeight честно уходит
// в 0, а старый код как ни в чём не бывало отдавал именно его же.
// Существующие тесты выше это не ловили -- у них всегда рядом стоит
// AnyHeightHerb, и TotalWeight никогда не достигает нуля.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistAltitude_SoleCandidateOutsideItsBandYieldsNothing,
    "Herbalist.Altitude.SoleCandidateOutsideItsBandYieldsNothing",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistAltitude_SoleCandidateOutsideItsBandYieldsNothing::RunTest(const FString& Parameters)
{
    UGameInstance* Owner = NewObject<UGameInstance>(GEngine);
    UIngredientRegistrySubsystem* Registry = NewObject<UIngredientRegistrySubsystem>(Owner);

    UDataTable* Table = NewObject<UDataTable>();
    Table->RowStruct = FIngredientTableRow::StaticStruct();

    // Единственная строка в таблице -- ровно то, что настраивал пользователь
    // по инструкции плана проверки ("в DT_IngredientClass одной строке...").
    FIngredientTableRow Lowland;
    Lowland.AllowedBiomes.Add(EBiomeType::Taiga);
    Lowland.bUseAltitudeRange = true;
    Lowland.MinAltitudeMeters = 0.0f;
    Lowland.MaxAltitudeMeters = 100.0f;
    Lowland.AltitudeFalloffMeters = 0.0f;
    Table->AddRow(LowlandHerb, Lowland);

    Registry->LoadFromDataTable(Table);
    const FGridCell Cell = MakeTaigaCell();

    // Внутри пояса -- растёт, единственный кандидат всегда выигрывает ролл.
    TestEqual(TEXT("Inside its band, the sole candidate always wins the roll"),
        CountRolls(Registry, Cell, MakeContextAt(50.0f), LowlandHerb, 20), 20);

    // Вне пояса -- НИЧЕГО, не сам этот кандидат по фолбэку. Проверяем
    // напрямую результат GetRandomResourceForBiome, не просто "не совпало":
    // NAME_None -- единственный честный ответ, когда буквально нечему расти.
    FRandomStream Rng(20260903);
    const FName Result = Registry->GetRandomResourceForBiome(Cell, MakeContextAt(400.0f), Rng);
    TestTrue(TEXT("Outside its band with no other candidate, the result is NAME_None, not the excluded herb itself"),
        Result.IsNone());

    return true;
}

#endif // WITH_AUTOMATION_TESTS
