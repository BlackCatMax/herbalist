// Source/ProjectHerbalistTests/Private/Tests/CellRandomStreamTest.cpp
//
// Разметка мира, этап 4 (2026-09-12) -- сиды от координаты клетки
// (DESIGN_World_Layout.md §6, §10). Основа клетки -- тип воды, число, виды и
// места ресурсов -- берётся из потока, выведенного из сида мира и глобальной
// координаты клетки, а не из общего WorldRNG. Тест из плана: одна и та же
// клетка даёт один и тот же результат при любом порядке обхода. Место ресурса
// -- из потока с солью его слота, и слот хранится у ресурса: ростер, поставленный
// заново, встаёт на свои места, даже если одного ресурса в нём уже нет.

#include "Core/World/GridWorldManager.h"
#include "Core/World/WorldLayout.h"
#include "Core/Resources/AHerbalistResourceActor.h"
#include "Core/Subsystems/IngredientRegistrySubsystem.h"
#include "Core/Subsystems/WaterTypeRegistrySubsystem.h"
#include "Core/Data/IngredientTableRow.h"
#include "Core/Data/WaterTypeRow.h"
#include "Misc/AutomationTest.h"
#include "Editor.h"
#include "Engine/World.h"
#include "Engine/DataTable.h"
#include "Engine/GameInstance.h"
#include "Engine/Engine.h"

#if WITH_AUTOMATION_TESTS && WITH_EDITOR

#include "TestWorldHelpers.h"

namespace
{
    using ECellPurposeForSeedTest = FWorldLayoutSolver::ECellRandomPurpose;

    // Три вида Тайги с равным весом: разные клетки должны получать разные
    // наборы. Тот же приём синтетической таблицы, что у GardenPlantingTest.cpp
    // -- у editor-мира автотеста нет GameInstance.
    UIngredientRegistrySubsystem* MakeThreeTaigaSpeciesRegistryForSeedTest()
    {
        UGameInstance* Owner = NewObject<UGameInstance>(GEngine);
        UIngredientRegistrySubsystem* Registry = NewObject<UIngredientRegistrySubsystem>(Owner);

        UDataTable* Table = NewObject<UDataTable>();
        Table->RowStruct = FIngredientTableRow::StaticStruct();
        const TCHAR* Names[] = { TEXT("CellSeedHerbA"), TEXT("CellSeedHerbB"), TEXT("CellSeedHerbC") };
        for (const TCHAR* Name : Names)
        {
            FIngredientTableRow Row;
            Row.DisplayName = FText::FromString(Name);
            Row.AllowedBiomes.Add(EBiomeType::Taiga);
            Row.RarityWeight = 1;
            Table->AddRow(FName(Name), Row);
        }
        Registry->LoadFromDataTable(Table);
        return Registry;
    }

    struct FSeededResourceForSeedTest
    {
        FName IngredientID;
        FVector Location;
        int32 Slot = INDEX_NONE;
    };

    using FSeededMapForSeedTest = TMap<FIntPoint, TArray<FSeededResourceForSeedTest>>;

    // Ресурсы клеток в порядке регистрации в клетке.
    FSeededMapForSeedTest CollectResourcesForSeedTest(AGridWorldManager* Manager, const TArray<FIntPoint>& Coords)
    {
        FSeededMapForSeedTest Result;
        for (const FIntPoint& Coord : Coords)
        {
            TArray<FSeededResourceForSeedTest>& Out = Result.Add(Coord);
            const FGridCell* Cell = Manager->GetCellConst(Coord.X, Coord.Y);
            if (!Cell)
            {
                continue;
            }
            for (const TWeakObjectPtr<AHerbalistResourceActor>& Ptr : Cell->ResourceActors)
            {
                if (const AHerbalistResourceActor* Actor = Ptr.Get())
                {
                    Out.Add({ Actor->GetIngredientID(), Actor->GetActorLocation(), Actor->GetPlacementSlot() });
                }
            }
        }
        return Result;
    }

    void ClearResourcesForSeedTest(AGridWorldManager* Manager, const TArray<FIntPoint>& Coords)
    {
        for (const FIntPoint& Coord : Coords)
        {
            FGridCell* Cell = Manager->GetCell(Coord.X, Coord.Y);
            if (!Cell)
            {
                continue;
            }
            for (const TWeakObjectPtr<AHerbalistResourceActor>& Ptr : Cell->ResourceActors)
            {
                if (AHerbalistResourceActor* Actor = Ptr.Get())
                {
                    Actor->Destroy();
                }
            }
            Cell->ResourceActors.Empty();
            Cell->DormantResourceIDs.Empty();
            Cell->DormantResourceSlots.Empty();
        }
    }

    // Блок 4x4 клеток Тайги без воды и посадок. Регионов нет (их убирает
    // SpawnAndBeginPlay) -- плотность по умолчанию, 1-3 на 100 м²; клетка 10 м,
    // чтобы в каждой было 1-3 ресурса.
    TArray<FIntPoint> PrepareTaigaBlockForSeedTest(AGridWorldManager* Manager)
    {
        Manager->CellSize = 1000.0f;
        TArray<FIntPoint> Coords;
        for (int32 Y = 2; Y < 6; ++Y)
        {
            for (int32 X = 2; X < 6; ++X)
            {
                FGridCell* Cell = Manager->GetCell(X, Y);
                if (!Cell)
                {
                    continue;
                }
                Cell->Biome = EBiomeType::Taiga;
                Cell->BiomeWeights.Empty();
                Cell->bIsWater = false;
                Cell->PlantedSpeciesID = NAME_None;
                Coords.Add(FIntPoint(X, Y));
            }
        }
        ClearResourcesForSeedTest(Manager, Coords);
        return Coords;
    }

    void SeedBlockForSeedTest(AGridWorldManager* Manager, const TArray<FIntPoint>& Coords, UIngredientRegistrySubsystem* Registry)
    {
        for (const FIntPoint& Coord : Coords)
        {
            Manager->SpawnResourcesInCell(*Manager->GetCell(Coord.X, Coord.Y), Registry);
        }
    }

    // Сравнивает два набора поклеточно; возвращает число ресурсов в первом.
    int32 CompareResourcesForSeedTest(FAutomationTestBase& Test, const TArray<FIntPoint>& Coords,
        const FSeededMapForSeedTest& Expected, const FSeededMapForSeedTest& Actual, TSet<FName>& OutSpecies)
    {
        int32 Total = 0;
        for (const FIntPoint& Coord : Coords)
        {
            const TArray<FSeededResourceForSeedTest>& A = Expected.FindChecked(Coord);
            const TArray<FSeededResourceForSeedTest>& B = Actual.FindChecked(Coord);
            Total += A.Num();
            if (!Test.TestEqual(FString::Printf(TEXT("Клетка (%d,%d): столько же ресурсов"), Coord.X, Coord.Y), B.Num(), A.Num()))
            {
                continue;
            }
            for (int32 Index = 0; Index < A.Num(); ++Index)
            {
                OutSpecies.Add(A[Index].IngredientID);
                Test.TestEqual(FString::Printf(TEXT("Клетка (%d,%d), ресурс %d: тот же вид"), Coord.X, Coord.Y, Index),
                    B[Index].IngredientID, A[Index].IngredientID);
                Test.TestEqual(FString::Printf(TEXT("Клетка (%d,%d), ресурс %d: тот же слот"), Coord.X, Coord.Y, Index),
                    B[Index].Slot, A[Index].Slot);
                Test.TestTrue(FString::Printf(TEXT("Клетка (%d,%d), ресурс %d: то же место"), Coord.X, Coord.Y, Index),
                    B[Index].Location.Equals(A[Index].Location, 0.01));
            }
        }
        return Total;
    }

    // Ставит ростер из записанного набора заново, пропуская ресурсы, для
    // которых Skip вернёт true; возвращает то, что ожидается увидеть.
    FSeededMapForSeedTest RespawnRosterForSeedTest(AGridWorldManager* Manager, const TArray<FIntPoint>& Coords,
        const FSeededMapForSeedTest& Seeded, UIngredientRegistrySubsystem* Registry, bool bWithSlots,
        TFunctionRef<bool(const FIntPoint&, int32)> Skip)
    {
        FSeededMapForSeedTest Expected;
        for (const FIntPoint& Coord : Coords)
        {
            TArray<FName> Roster;
            TArray<int32> Slots;
            TArray<FSeededResourceForSeedTest>& Kept = Expected.Add(Coord);
            const TArray<FSeededResourceForSeedTest>& Resources = Seeded.FindChecked(Coord);
            for (int32 Index = 0; Index < Resources.Num(); ++Index)
            {
                if (Skip(Coord, Index))
                {
                    continue;
                }
                Roster.Add(Resources[Index].IngredientID);
                Slots.Add(Resources[Index].Slot);
                Kept.Add(Resources[Index]);
            }
            Manager->SpawnResourceRoster(*Manager->GetCell(Coord.X, Coord.Y), Roster, bWithSlots ? Slots : TArray<int32>(), Registry);
        }
        return Expected;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistCellSeed_SeedDependsOnlyOnItsInputs,
    "Herbalist.WorldLayout.CellSeed.SeedDependsOnlyOnItsInputs",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistCellSeed_SeedDependsOnlyOnItsInputs::RunTest(const FString& Parameters)
{
    const FIntPoint Cell(7, -3);
    const int32 Base = FWorldLayoutSolver::MakeCellSeed(12345, Cell, ECellPurposeForSeedTest::ResourceCount, 0);
    TestEqual(TEXT("Те же входы -- тот же сид"), FWorldLayoutSolver::MakeCellSeed(12345, Cell, ECellPurposeForSeedTest::ResourceCount, 0), Base);
    TestNotEqual(TEXT("Другой сид мира -- другой сид клетки"), FWorldLayoutSolver::MakeCellSeed(54321, Cell, ECellPurposeForSeedTest::ResourceCount, 0), Base);
    TestNotEqual(TEXT("Другое назначение -- другой поток"), FWorldLayoutSolver::MakeCellSeed(12345, Cell, ECellPurposeForSeedTest::WaterType, 0), Base);
    TestNotEqual(TEXT("Другая соль -- другой поток"), FWorldLayoutSolver::MakeCellSeed(12345, Cell, ECellPurposeForSeedTest::ResourceCount, 1), Base);
    TestNotEqual(TEXT("Переставленные X и Y -- другая клетка"), FWorldLayoutSolver::MakeCellSeed(12345, FIntPoint(-3, 7), ECellPurposeForSeedTest::ResourceCount, 0), Base);

    // 64x64 клетки вокруг начала сетки, включая отрицательные координаты:
    // соседние клетки не делят поток.
    TSet<int32> Seeds;
    for (int32 Y = -32; Y < 32; ++Y)
    {
        for (int32 X = -32; X < 32; ++X)
        {
            Seeds.Add(FWorldLayoutSolver::MakeCellSeed(12345, FIntPoint(X, Y), ECellPurposeForSeedTest::ResourceCount, 0));
        }
    }
    TestEqual(TEXT("4096 клеток -- 4096 разных сидов"), Seeds.Num(), 4096);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistCellSeed_StreamFollowsGlobalCoordinate,
    "Herbalist.WorldLayout.CellSeed.StreamFollowsGlobalCoordinate",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistCellSeed_StreamFollowsGlobalCoordinate::RunTest(const FString& Parameters)
{
    // Решение пользователя 13: координаты клеток -- от начала сетки World
    // Partition. Сетка, сдвинутая разметкой, у той же мировой клетки получает
    // тот же поток, что и несдвинутая.
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;
    AGridWorldManager* Manager = SpawnAndBeginPlay(World, {}, 4242);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;
    if (!TestFalse(TEXT("У менеджера автотеста разметка не выведена"), Manager->ResolvedLayout.bValid))
    {
        Manager->Destroy();
        return false;
    }

    // С этапа 6 координаты клеток глобальные: поток клетки -- функция её
    // координаты и сида мира, от сдвига сетки он не зависит.
    const FRandomStream Unshifted = Manager->MakeCellRandomStream(-5, 7, ECellPurposeForSeedTest::ResourceSpecies);
    TestEqual(TEXT("Поток клетки -- сид от её координаты"), Unshifted.GetInitialSeed(),
        FWorldLayoutSolver::MakeCellSeed(Manager->RngBaseSeed, FIntPoint(-5, 7), ECellPurposeForSeedTest::ResourceSpecies, 0));
    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistCellSeed_ResourcesDoNotDependOnTraversalOrder,
    "Herbalist.WorldLayout.CellSeed.ResourcesDoNotDependOnTraversalOrder",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistCellSeed_ResourcesDoNotDependOnTraversalOrder::RunTest(const FString& Parameters)
{
    // Тест из плана (§10, этап 4). Прежде число, виды и места брались из
    // общего WorldRNG, и обратный обход раздавал клеткам чужие броски.
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;
    UIngredientRegistrySubsystem* Registry = MakeThreeTaigaSpeciesRegistryForSeedTest();
    AGridWorldManager* Manager = SpawnAndBeginPlay(World, {}, 4242);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;

    const TArray<FIntPoint> Coords = PrepareTaigaBlockForSeedTest(Manager);

    SeedBlockForSeedTest(Manager, Coords, Registry);
    const FSeededMapForSeedTest Forward = CollectResourcesForSeedTest(Manager, Coords);
    ClearResourcesForSeedTest(Manager, Coords);

    for (int32 Index = Coords.Num() - 1; Index >= 0; --Index)
    {
        Manager->SpawnResourcesInCell(*Manager->GetCell(Coords[Index].X, Coords[Index].Y), Registry);
    }
    const FSeededMapForSeedTest Reverse = CollectResourcesForSeedTest(Manager, Coords);
    ClearResourcesForSeedTest(Manager, Coords);

    TSet<FName> Species;
    const int32 Total = CompareResourcesForSeedTest(*this, Coords, Forward, Reverse, Species);
    TestTrue(TEXT("Ресурсы вообще появились"), Total > 0);
    TestTrue(TEXT("Больше одного вида -- поток клетки действительно случаен"), Species.Num() > 1);

    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistCellSeed_RespawnedRosterReturnsToItsPlaces,
    "Herbalist.WorldLayout.CellSeed.RespawnedRosterReturnsToItsPlaces",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistCellSeed_RespawnedRosterReturnsToItsPlaces::RunTest(const FString& Parameters)
{
    // Материализация чанка и загрузка сейва ставят сохранённый ростер через
    // SpawnResourceRoster: каждый ресурс -- на место своего слота, того же, что
    // при первичном заселении. Прежде каждый возврат игрока давал новые места.
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;
    UIngredientRegistrySubsystem* Registry = MakeThreeTaigaSpeciesRegistryForSeedTest();
    AGridWorldManager* Manager = SpawnAndBeginPlay(World, {}, 4242);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;

    const TArray<FIntPoint> Coords = PrepareTaigaBlockForSeedTest(Manager);
    SeedBlockForSeedTest(Manager, Coords, Registry);
    const FSeededMapForSeedTest Seeded = CollectResourcesForSeedTest(Manager, Coords);
    ClearResourcesForSeedTest(Manager, Coords);

    const FSeededMapForSeedTest Expected = RespawnRosterForSeedTest(Manager, Coords, Seeded, Registry, /*bWithSlots=*/true,
        [](const FIntPoint&, int32) { return false; });
    const FSeededMapForSeedTest Respawned = CollectResourcesForSeedTest(Manager, Coords);
    ClearResourcesForSeedTest(Manager, Coords);

    TSet<FName> Species;
    const int32 Total = CompareResourcesForSeedTest(*this, Coords, Expected, Respawned, Species);
    TestTrue(TEXT("Ресурсы вообще появились"), Total > 0);

    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistCellSeed_RosterKeepsPlacesWhenAResourceIsGone,
    "Herbalist.WorldLayout.CellSeed.RosterKeepsPlacesWhenAResourceIsGone",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistCellSeed_RosterKeepsPlacesWhenAResourceIsGone::RunTest(const FString& Parameters)
{
    // Найдено ревью: с одним потоком мест подряд ресурс, собранный игроком или
    // не нашедший места при заселении, сдвигал всех следующих -- после
    // усыпления чанка они вставали на чужие места, а ресурс после неудачной
    // попытки мог не встать вовсе и пропасть из клетки и сейва. Со слотами
    // первый ресурс каждой клетки выброшен из ростера -- остальные на прежних
    // местах.
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;
    UIngredientRegistrySubsystem* Registry = MakeThreeTaigaSpeciesRegistryForSeedTest();
    AGridWorldManager* Manager = SpawnAndBeginPlay(World, {}, 4242);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;

    const TArray<FIntPoint> Coords = PrepareTaigaBlockForSeedTest(Manager);
    SeedBlockForSeedTest(Manager, Coords, Registry);
    const FSeededMapForSeedTest Seeded = CollectResourcesForSeedTest(Manager, Coords);
    ClearResourcesForSeedTest(Manager, Coords);

    const FSeededMapForSeedTest Expected = RespawnRosterForSeedTest(Manager, Coords, Seeded, Registry, /*bWithSlots=*/true,
        [](const FIntPoint&, int32 Index) { return Index == 0; });
    const FSeededMapForSeedTest Respawned = CollectResourcesForSeedTest(Manager, Coords);
    ClearResourcesForSeedTest(Manager, Coords);

    TSet<FName> Species;
    const int32 Kept = CompareResourcesForSeedTest(*this, Coords, Expected, Respawned, Species);
    TestTrue(TEXT("В блоке есть клетки больше чем с одним ресурсом -- проверке есть что сравнивать"), Kept > 0);

    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistCellSeed_RosterWithoutSlotsTakesSeedingPlaces,
    "Herbalist.WorldLayout.CellSeed.RosterWithoutSlotsTakesSeedingPlaces",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistCellSeed_RosterWithoutSlotsTakesSeedingPlaces::RunTest(const FString& Parameters)
{
    // Сейв старее слотов хранит только ID. Слоты выдаются по порядку, 0..n-1,
    // и в клетке, где при заселении встали все ресурсы, это те же места.
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;
    UIngredientRegistrySubsystem* Registry = MakeThreeTaigaSpeciesRegistryForSeedTest();
    AGridWorldManager* Manager = SpawnAndBeginPlay(World, {}, 4242);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;

    const TArray<FIntPoint> Coords = PrepareTaigaBlockForSeedTest(Manager);
    SeedBlockForSeedTest(Manager, Coords, Registry);
    FSeededMapForSeedTest Seeded = CollectResourcesForSeedTest(Manager, Coords);
    ClearResourcesForSeedTest(Manager, Coords);

    // Только клетки, где слоты идут подряд с нуля: там порядок совпадает.
    TArray<FIntPoint> Contiguous;
    for (const FIntPoint& Coord : Coords)
    {
        const TArray<FSeededResourceForSeedTest>& Resources = Seeded.FindChecked(Coord);
        bool bContiguous = true;
        for (int32 Index = 0; Index < Resources.Num(); ++Index)
        {
            bContiguous &= Resources[Index].Slot == Index;
        }
        if (bContiguous)
        {
            Contiguous.Add(Coord);
        }
    }

    const FSeededMapForSeedTest Expected = RespawnRosterForSeedTest(Manager, Contiguous, Seeded, Registry, /*bWithSlots=*/false,
        [](const FIntPoint&, int32) { return false; });
    const FSeededMapForSeedTest Respawned = CollectResourcesForSeedTest(Manager, Contiguous);
    ClearResourcesForSeedTest(Manager, Contiguous);

    TSet<FName> Species;
    const int32 Total = CompareResourcesForSeedTest(*this, Contiguous, Expected, Respawned, Species);
    TestTrue(TEXT("Есть клетки с ресурсами -- проверке есть что сравнивать"), Total > 0);

    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistCellSeed_WaterTypeDoesNotDependOnOtherCells,
    "Herbalist.WorldLayout.CellSeed.WaterTypeDoesNotDependOnOtherCells",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistCellSeed_WaterTypeDoesNotDependOnOtherCells::RunTest(const FString& Parameters)
{
    // Проверяет RollWaterTypeForCell, которой пользуется заливка воды в
    // InitializeCells: сама заливка в автотесте не видна -- у editor-мира нет
    // GameInstance и подсистемы воды. Функция константная и WorldRNG трогать
    // не может; тест держит то, что от неё требуется: тип зависит только от
    // клетки, а разные клетки получают разные типы.
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    // Синтетическая таблица, как в WaterTypeRegistryTest.cpp: в боевой
    // DT_WaterTypes у каждого биома по одному типу воды, и выбору там не из
    // чего выбирать. Два равновероятных типа у Тайги.
    UDataTable* Table = NewObject<UDataTable>();
    Table->RowStruct = FWaterTypeRow::StaticStruct();
    for (const TCHAR* Name : { TEXT("CellSeedWaterA"), TEXT("CellSeedWaterB") })
    {
        FWaterTypeRow Row;
        Row.WaterTypeID = FName(Name);
        Row.AllowedBiomes.Add(EBiomeType::Taiga);
        Row.Rarity = 0.5f;
        Table->AddRow(FName(Name), Row);
    }
    UWaterTypeRegistrySubsystem* Water = NewObject<UWaterTypeRegistrySubsystem>(NewObject<UGameInstance>(GEngine));
    Water->LoadFromDataTable(Table);
    if (!TestEqual(TEXT("У Тайги два типа воды"), Water->GetWaterTypesForBiome(EBiomeType::Taiga).Num(), 2)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World, {}, 4242);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;

    FGridCell* Probe = Manager->GetCell(3, 3);
    if (!TestNotNull(TEXT("Probe cell exists"), Probe)) { Manager->Destroy(); return false; }
    Probe->Biome = EBiomeType::Taiga;
    const FName First = Manager->RollWaterTypeForCell(*Probe, Water);

    TSet<FName> Seen;
    for (int32 Y = 0; Y < Manager->GridSizeY; ++Y)
    {
        for (int32 X = 0; X < Manager->GridSizeX; ++X)
        {
            if (FGridCell* Cell = Manager->GetCell(X, Y))
            {
                Cell->Biome = EBiomeType::Taiga;
                Seen.Add(Manager->RollWaterTypeForCell(*Cell, Water));
            }
        }
    }

    TestEqual(TEXT("Клетка получает тот же тип воды, сколько бы клеток ни бросали до неё"),
        Manager->RollWaterTypeForCell(*Probe, Water), First);
    TestTrue(TEXT("Разные клетки получают разные типы"), Seen.Num() > 1);

    Manager->Destroy();
    return true;
}

#endif // WITH_AUTOMATION_TESTS && WITH_EDITOR
