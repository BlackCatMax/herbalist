// Source/ProjectHerbalistTests/Private/Tests/GridWorldManagerBiomeRegionTest.cpp
//
// PCG-биомы (2026-08-31): интеграция ABiomeRegionVolume в
// AGridWorldManager::InitializeCells(). Точечная математика point-in-
// polygon уже проверена отдельно в BiomeRegionVolumeTest.cpp — этот файл
// проверяет ПРОВОД: равное распределение весов на пересечении регионов,
// доминанту, фолбэк на старую блочную формулу вне регионов, и что при
// нуле регионов поведение байт-в-байт совпадает с оригиналом (весь
// остальной набор тестов проекта — implicit-регресс на этот же случай,
// ни один существующий тест не размещает регион).

#include "Core/World/GridWorldManager.h"
#include "Core/World/BiomeRegionVolume.h"
#include "Core/Types/BiomeTypes.h"
#include "Components/SplineComponent.h"
#include "Misc/AutomationTest.h"
#include "Editor.h"
#include "Engine/World.h"

#if WITH_AUTOMATION_TESTS && WITH_EDITOR

// SpawnRegionCoveringWorldRect — вынесен в TestWorldHelpers.h (2026-09-02,
// GridWorldManagerSpawnPositionTest.cpp стал вторым потребителем).
#include "TestWorldHelpers.h"

namespace
{
    // Тот же список и та же формула, что AGridWorldManager::InitializeCells()
    // — продублировано намеренно (не рефакторим продакшн ради теста),
    // чтобы ловить тихий дрейф фолбэка, а не полагаться на "предполагаем,
    // что тестовое окружение не грузит DT_BiomeDefaults".
    TArray<EBiomeType> ExpectedFallbackBiomeList()
    {
        TArray<EBiomeType> AllBiomes = FBiomeDefaults::GetAllBiomeTypes();
        if (AllBiomes.Num() == 0)
        {
            AllBiomes = {
                EBiomeType::Tundra, EBiomeType::Taiga, EBiomeType::MixedForest,
                EBiomeType::BroadleafForest, EBiomeType::ForestSteppe,
                EBiomeType::Steppe, EBiomeType::Floodplain, EBiomeType::Bog
            };
        }
        return AllBiomes;
    }

    EBiomeType ExpectedFallbackBiomeAt(int32 X, int32 Y, int32 GridSizeX)
    {
        const TArray<EBiomeType> AllBiomes = ExpectedFallbackBiomeList();
        const int32 BlockSize = 5;
        const int32 BlocksX = GridSizeX / BlockSize;
        return AllBiomes[((Y / BlockSize) * BlocksX + (X / BlockSize)) % AllBiomes.Num()];
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistGridBiomeRegion_SingleRegionCoversWholeGrid,
    "Herbalist.GridBiomeRegion.SingleRegionCoversWholeGrid",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistGridBiomeRegion_SingleRegionCoversWholeGrid::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    ABiomeRegionVolume* Region = SpawnRegionCoveringWorldRect(World, EBiomeType::Bog, -50.f, -50.f, 1950.f, 1950.f);
    if (!TestNotNull(TEXT("Region spawned"), Region)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World, { Region });
    if (!TestNotNull(TEXT("Manager spawned"), Manager)) { Region->Destroy(); return false; }

    bool bAllMatch = true;
    for (int32 Y = 0; Y < Manager->GridSizeY && bAllMatch; ++Y)
    {
        for (int32 X = 0; X < Manager->GridSizeX; ++X)
        {
            FGridCell* Cell = Manager->GetCell(X, Y);
            if (!Cell || Cell->Biome != EBiomeType::Bog || Cell->BiomeWeights.Num() != 1 ||
                Cell->BiomeWeights[0].Biome != EBiomeType::Bog ||
                !FMath::IsNearlyEqual(Cell->BiomeWeights[0].Weight, 1.0f))
            {
                bAllMatch = false;
                break;
            }
        }
    }
    TestTrue(TEXT("Every cell: Biome==Bog, BiomeWeights==[{Bog,1.0}]"), bAllMatch);

    Manager->Destroy();
    Region->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistGridBiomeRegion_TwoOverlappingRegionsSplitEqually,
    "Herbalist.GridBiomeRegion.TwoOverlappingRegionsSplitEqually",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistGridBiomeRegion_TwoOverlappingRegionsSplitEqually::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    ABiomeRegionVolume* RegionA = SpawnRegionCoveringWorldRect(World, EBiomeType::Bog, -50.f, -50.f, 1950.f, 1950.f);
    ABiomeRegionVolume* RegionB = SpawnRegionCoveringWorldRect(World, EBiomeType::Taiga, -50.f, -50.f, 1950.f, 1950.f);
    if (!TestNotNull(TEXT("Region A spawned"), RegionA) || !TestNotNull(TEXT("Region B spawned"), RegionB)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World, { RegionA, RegionB });
    if (!TestNotNull(TEXT("Manager spawned"), Manager)) { RegionA->Destroy(); RegionB->Destroy(); return false; }

    bool bAllMatch = true;
    for (int32 Y = 0; Y < Manager->GridSizeY && bAllMatch; ++Y)
    {
        for (int32 X = 0; X < Manager->GridSizeX; ++X)
        {
            FGridCell* Cell = Manager->GetCell(X, Y);
            if (!Cell || Cell->BiomeWeights.Num() != 2)
            {
                bAllMatch = false;
                break;
            }
            for (const FBiomeWeightEntry& Entry : Cell->BiomeWeights)
            {
                if (!FMath::IsNearlyEqual(Entry.Weight, 0.5f, 0.001f))
                {
                    bAllMatch = false;
                    break;
                }
            }
            // Доминанта при точном равенстве -- меньший порядковый номер
            // EBiomeType, не порядок акторов в TActorIterator.
            const EBiomeType ExpectedDominant = static_cast<uint8>(EBiomeType::Bog) < static_cast<uint8>(EBiomeType::Taiga)
                ? EBiomeType::Bog : EBiomeType::Taiga;
            if (Cell->Biome != ExpectedDominant)
            {
                bAllMatch = false;
                break;
            }
        }
    }
    TestTrue(TEXT("Every cell: exactly 2 entries, each weight 0.5, dominant is the lower EBiomeType ordinal"), bAllMatch);

    Manager->Destroy();
    RegionA->Destroy();
    RegionB->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistGridBiomeRegion_ThreeOverlappingRegionsSplitInThirds,
    "Herbalist.GridBiomeRegion.ThreeOverlappingRegionsSplitInThirds",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistGridBiomeRegion_ThreeOverlappingRegionsSplitInThirds::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    TArray<ABiomeRegionVolume*> Regions;
    Regions.Add(SpawnRegionCoveringWorldRect(World, EBiomeType::Bog, -50.f, -50.f, 1950.f, 1950.f));
    Regions.Add(SpawnRegionCoveringWorldRect(World, EBiomeType::Taiga, -50.f, -50.f, 1950.f, 1950.f));
    Regions.Add(SpawnRegionCoveringWorldRect(World, EBiomeType::Steppe, -50.f, -50.f, 1950.f, 1950.f));
    for (ABiomeRegionVolume* R : Regions)
    {
        if (!TestNotNull(TEXT("Region spawned"), R)) return false;
    }

    TArray<AActor*> RegionsAsActors;
    for (ABiomeRegionVolume* R : Regions) { RegionsAsActors.Add(R); }
    AGridWorldManager* Manager = SpawnAndBeginPlay(World, RegionsAsActors);
    if (!TestNotNull(TEXT("Manager spawned"), Manager))
    {
        for (ABiomeRegionVolume* R : Regions) R->Destroy();
        return false;
    }

    bool bAllMatch = true;
    for (int32 Y = 0; Y < Manager->GridSizeY && bAllMatch; ++Y)
    {
        for (int32 X = 0; X < Manager->GridSizeX; ++X)
        {
            FGridCell* Cell = Manager->GetCell(X, Y);
            if (!Cell || Cell->BiomeWeights.Num() != 3) { bAllMatch = false; break; }
            float Sum = 0.0f;
            for (const FBiomeWeightEntry& Entry : Cell->BiomeWeights)
            {
                if (!FMath::IsNearlyEqual(Entry.Weight, 1.0f / 3.0f, 0.001f)) { bAllMatch = false; break; }
                Sum += Entry.Weight;
            }
            if (!FMath::IsNearlyEqual(Sum, 1.0f, 0.001f)) { bAllMatch = false; break; }
        }
    }
    TestTrue(TEXT("Every cell: 3 entries each ~1/3, sum ~1.0"), bAllMatch);

    Manager->Destroy();
    for (ABiomeRegionVolume* R : Regions) R->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistGridBiomeRegion_PartialCoverageFallsBackForUncoveredCells,
    "Herbalist.GridBiomeRegion.PartialCoverageFallsBackForUncoveredCells",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistGridBiomeRegion_PartialCoverageFallsBackForUncoveredCells::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    // Покрываем только левую половину сетки (X мировые 0..900, клетки 0-9)
    // -- правая половина (клетки 10-19) остаётся вне всех регионов.
    ABiomeRegionVolume* Region = SpawnRegionCoveringWorldRect(World, EBiomeType::Bog, -50.f, -50.f, 950.f, 1950.f);
    if (!TestNotNull(TEXT("Region spawned"), Region)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World, { Region });
    if (!TestNotNull(TEXT("Manager spawned"), Manager)) { Region->Destroy(); return false; }

    bool bAllMatch = true;
    for (int32 Y = 0; Y < Manager->GridSizeY && bAllMatch; ++Y)
    {
        for (int32 X = 0; X < Manager->GridSizeX; ++X)
        {
            FGridCell* Cell = Manager->GetCell(X, Y);
            if (!Cell) { bAllMatch = false; break; }

            if (X <= 9)
            {
                if (Cell->Biome != EBiomeType::Bog || Cell->BiomeWeights.Num() != 1 ||
                    !FMath::IsNearlyEqual(Cell->BiomeWeights[0].Weight, 1.0f))
                {
                    bAllMatch = false; break;
                }
            }
            else
            {
                const EBiomeType Expected = ExpectedFallbackBiomeAt(X, Y, Manager->GridSizeX);
                if (Cell->Biome != Expected || Cell->BiomeWeights.Num() != 0)
                {
                    bAllMatch = false; break;
                }
            }
        }
    }
    TestTrue(TEXT("Covered half gets the region's biome at weight 1.0; uncovered half matches the legacy block formula exactly"), bAllMatch);

    Manager->Destroy();
    Region->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistGridBiomeRegion_ZeroRegionsMatchesLegacyFormulaExactly,
    "Herbalist.GridBiomeRegion.ZeroRegionsMatchesLegacyFormulaExactly",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistGridBiomeRegion_ZeroRegionsMatchesLegacyFormulaExactly::RunTest(const FString& Parameters)
{
    // Регресс-пин на оригинальный алгоритм: ни один регион не размещён,
    // поведение обязано быть байт-в-байт тем же, что до этой фичи. Весь
    // остальной набор тестов проекта неявно проверяет то же самое (никто
    // из них не размещает ABiomeRegionVolume), это -- явная, целевая
    // версия того же утверждения.
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("Manager spawned"), Manager)) return false;

    bool bAllMatch = true;
    for (int32 Y = 0; Y < Manager->GridSizeY && bAllMatch; ++Y)
    {
        for (int32 X = 0; X < Manager->GridSizeX; ++X)
        {
            FGridCell* Cell = Manager->GetCell(X, Y);
            const EBiomeType Expected = ExpectedFallbackBiomeAt(X, Y, Manager->GridSizeX);
            if (!Cell || Cell->Biome != Expected || Cell->BiomeWeights.Num() != 0)
            {
                bAllMatch = false;
                break;
            }
        }
    }
    TestTrue(TEXT("Zero regions: every cell matches the legacy block-pattern formula exactly, BiomeWeights stays empty"), bAllMatch);

    Manager->Destroy();
    return true;
}

#endif // WITH_AUTOMATION_TESTS && WITH_EDITOR
