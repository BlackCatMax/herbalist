// Source/ProjectHerbalistTests/Private/Tests/RegistryLazyLoadTest.cpp
//
// Регрессия на реальный баг, найденный по PIE-логу пользователя
// (2026-09-03): в игре не появлялось НИ ОДНОГО ресурсного актора, хотя
// таблица заполнена, меши на месте, а редакторское превью точек показывало
// сотни свободных мест.
//
// Причина была не в данных, а в порядке. Initialize() у
// UIngredientRegistrySubsystem ничего не грузил -- таблицу подавал
// AProjectHerbalistGameModeBase::BeginPlay. Но GameMode спавнится
// динамически и попадает в конец списка акторов, поэтому BeginPlay
// РАЗМЕЩЁННОГО на уровне BP_GridWorldManager отрабатывал раньше.
// SpawnResourcesInCell брала подсистему -- она есть, не null! -- с пустой
// картой Rows, GetRandomResourceForBiome возвращала NAME_None на каждый
// бросок, и цикл спавна молча делал continue для всего мира. В логе это
// видно буквально: "IngredientRegistrySubsystem loaded 89 ingredients"
// стоит ПОСЛЕ "InitializeCells" и "Cached 10000 cell heights".
//
// Проверяется здесь именно инвариант, а не тот конкретный порядок:
// **чтение реестра, которому никто не подал таблицу, обязано вернуть
// содержимое боевой таблицы, а не пустоту**. Тестировать сам порядок
// BeginPlay бессмысленно -- он деталь движка и может измениться; ценность
// в том, что от порядка больше ничего не зависит.
//
// Почему не через SpawnAndBeginPlay: тестовый мир редактора не имеет
// UGameInstance, поэтому SpawnResourcesInCell выходит ещё раньше (на
// проверке GameInstance) и ресурсы там не спавнятся ни до фикса, ни после
// -- такой тест был бы зелёным всегда и не проверял бы ничего. Подсистема
// создаётся напрямую, тем же приёмом MakeRegistry, что уже у
// IngredientRegistryTest.cpp (ClassWithin = UGameInstance, поэтому нужен
// временный GameInstance как Outer).

#include "Core/Subsystems/IngredientRegistrySubsystem.h"
#include "Core/Subsystems/WaterTypeRegistrySubsystem.h"
#include "Core/Data/IngredientTableRow.h"
#include "Misc/AutomationTest.h"
#include "Engine/DataTable.h"
#include "Engine/GameInstance.h"
#include "Engine/Engine.h"

#if WITH_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistRegistry_IngredientsSelfLoadWithoutExplicitCall,
    "Herbalist.Registry.IngredientsSelfLoadWithoutExplicitCall",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistRegistry_IngredientsSelfLoadWithoutExplicitCall::RunTest(const FString& Parameters)
{
    UGameInstance* Owner = NewObject<UGameInstance>(GEngine);
    UIngredientRegistrySubsystem* Registry = NewObject<UIngredientRegistrySubsystem>(Owner);

    // Намеренно НЕ зовём LoadFromDataTable -- ровно положение
    // AGridWorldManager::InitializeCells до GameMode::BeginPlay.
    // Биом задан явно, а не оставлен дефолтом: Болото гарантированно
    // населено в компендиуме, и провал теста тогда означает поломку
    // загрузки, а не пустой биом.
    FGridCell Cell;
    Cell.Biome = EBiomeType::Bog;
    const FHarvestContext Context;
    FRandomStream Rng(1234);

    // Читаем через тот же вход, которым пользуется спавн ресурсов.
    const FName Picked = Registry->GetRandomResourceForBiome(Cell, Context, Rng);

    TestFalse(TEXT("Реестр сам загрузил боевую таблицу: кандидат для биома найден, а не NAME_None"),
        Picked.IsNone());

    // И тот же ответ на прямое чтение строки -- чтобы «не None» нельзя было
    // получить мусором.
    if (!Picked.IsNone())
    {
        TestNotNull(TEXT("Выбранный кандидат реально есть в таблице"), Registry->GetRow(Picked));
    }

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistRegistry_ExplicitLoadStillWinsOverSelfLoad,
    "Herbalist.Registry.ExplicitLoadStillWinsOverSelfLoad",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistRegistry_ExplicitLoadStillWinsOverSelfLoad::RunTest(const FString& Parameters)
{
    // Обратная сторона ленивой загрузки, и она важнее первого теста: если бы
    // самозагрузка происходила в Initialize() (жадно), она бы навсегда
    // заняла bInitialized, и КАЖДЫЙ тест, подсовывающий собственную таблицу
    // (их девять), молча работал бы против боевой -- зелёный по неверной
    // причине. Явный вызов, сделанный ДО первого чтения, обязан выигрывать.
    UGameInstance* Owner = NewObject<UGameInstance>(GEngine);
    UIngredientRegistrySubsystem* Registry = NewObject<UIngredientRegistrySubsystem>(Owner);

    UDataTable* Table = NewObject<UDataTable>();
    Table->RowStruct = FIngredientTableRow::StaticStruct();

    FIngredientTableRow Row;
    Table->AddRow(FName(TEXT("ТолькоЭта")), Row);

    Registry->LoadFromDataTable(Table);

    // Достаточное условие: будь самозагрузка жадной (в Initialize),
    // bInitialized был бы уже занят, явный вызов стал бы no-op, и этой
    // строки в реестре не оказалось бы вовсе.
    TestNotNull(TEXT("Подсунутая таблица принята, самозагрузка её не перебила"),
        Registry->GetRow(FName(TEXT("ТолькоЭта"))));

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistRegistry_WaterTypesSelfLoadWithoutExplicitCall,
    "Herbalist.Registry.WaterTypesSelfLoadWithoutExplicitCall",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistRegistry_WaterTypesSelfLoadWithoutExplicitCall::RunTest(const FString& Parameters)
{
    UGameInstance* Owner = NewObject<UGameInstance>(GEngine);
    UWaterTypeRegistrySubsystem* Registry = NewObject<UWaterTypeRegistrySubsystem>(Owner);

    TestTrue(TEXT("Реестр типов воды самозагрузился (в боевой таблице 8 типов)"),
        Registry->GetWaterTypeCount() > 0);

    return true;
}

#endif // WITH_AUTOMATION_TESTS
