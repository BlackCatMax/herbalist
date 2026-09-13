// Source/ProjectHerbalistTests/Private/Tests/GridWorldManagerWaterRegionTest.cpp
//
// AWaterRegionVolume -- вода как отдельный, явно нарисованный регион
// (2026-09-02, прямой запрос пользователя): "я вручную могу указывать, где
// будет вода, и она всегда превалирует над любым биомом... её вес всегда
// 1... биом воды, размещённый поверх других биомов, должен автоматически
// становиться тем типом воды, который для биома характерен".
//
// 2026-09-13, решения пользователя: вода -- только из регионов воды (пятна
// воды по плотности региона убраны); на границах биомов вода смешивается --
// состояние водной клетки есть смесь состояний типов воды её биомов по долям
// BiomeWeights, тип воды для сбора -- от доминирующего биома.

#include "Core/World/GridWorldManager.h"
#include "Core/World/BiomeRegionVolume.h"
#include "Core/World/WaterRegionVolume.h"
#include "Core/Subsystems/WaterTypeRegistrySubsystem.h"
#include "Core/Data/WaterTypeRow.h"
#include "Core/Types/BiomeTypes.h"
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
    int32 CountWaterCellsInGrid(const AGridWorldManager* Manager)
    {
        int32 WaterCells = 0;
        for (int32 Y = 0; Y < Manager->GridSizeY; ++Y)
        {
            for (int32 X = 0; X < Manager->GridSizeX; ++X)
            {
                const FGridCell* Cell = Manager->GetCellConst(X, Y);
                WaterCells += (Cell && Cell->bIsWater) ? 1 : 0;
            }
        }
        return WaterCells;
    }

    bool IsSameWaterMeta(const FMeta& A, const FMeta& B)
    {
        const float Tolerance = 1e-5f;
        return FMath::IsNearlyEqual(A.Distortion, B.Distortion, Tolerance)
            && FMath::IsNearlyEqual(A.Stability, B.Stability, Tolerance)
            && FMath::IsNearlyEqual(A.Purity, B.Purity, Tolerance)
            && FMath::IsNearlyEqual(A.Potency, B.Potency, Tolerance)
            && FMath::IsNearlyEqual(A.Resonance, B.Resonance, Tolerance)
            && FMath::IsNearlyEqual(A.Corruption, B.Corruption, Tolerance);
    }

    FMeta MixWaterMetaHalfAndHalf(const FMeta& A, const FMeta& B)
    {
        FMeta Mixed;
        Mixed.Distortion = 0.5f * A.Distortion + 0.5f * B.Distortion;
        Mixed.Stability = 0.5f * A.Stability + 0.5f * B.Stability;
        Mixed.Purity = 0.5f * A.Purity + 0.5f * B.Purity;
        Mixed.Potency = 0.5f * A.Potency + 0.5f * B.Potency;
        Mixed.Resonance = 0.5f * A.Resonance + 0.5f * B.Resonance;
        Mixed.Corruption = 0.5f * A.Corruption + 0.5f * B.Corruption;
        return Mixed;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistWaterRegion_AlwaysWinsOverLandRegion,
    "Herbalist.WaterRegion.AlwaysWinsOverLandRegion",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistWaterRegion_AlwaysWinsOverLandRegion::RunTest(const FString& Parameters)
{
    // Земляной регион (Bog) на весь тестовый угол сетки. Регион воды поверх
    // ЧАСТИ той же площади безусловно заливает её водой, вес 1; клетка вне
    // региона воды остаётся сушей.
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    ABiomeRegionVolume* LandRegion = SpawnRegionCoveringWorldRect(World, EBiomeType::Bog, -50.f, -50.f, 950.f, 950.f);
    if (!TestNotNull(TEXT("Land region spawned"), LandRegion)) return false;

    AWaterRegionVolume* WaterRegion = SpawnWaterRegionCoveringWorldRect(World, -50.f, -50.f, 450.f, 450.f);
    if (!TestNotNull(TEXT("Water region spawned"), WaterRegion)) { LandRegion->Destroy(); return false; }

    AGridWorldManager* Manager = SpawnAndBeginPlay(World, { LandRegion, WaterRegion });
    if (!TestNotNull(TEXT("Manager spawned"), Manager))
    {
        LandRegion->Destroy(); WaterRegion->Destroy(); return false;
    }

    const FGridCell* UnderWater = Manager->GetCellConst(2, 2);   // внутри обоих регионов
    const FGridCell* LandOnly   = Manager->GetCellConst(7, 7);   // внутри земляного, вне воды
    if (!TestNotNull(TEXT("Cell under water region exists"), UnderWater) ||
        !TestNotNull(TEXT("Land-only cell exists"), LandOnly))
    {
        Manager->Destroy(); LandRegion->Destroy(); WaterRegion->Destroy(); return false;
    }

    TestTrue(TEXT("Cell under an explicit water region is water"), UnderWater->bIsWater);
    TestEqual(TEXT("Cell.Biome under the water stays the land region's biome (Bog), water doesn't replace it"),
        UnderWater->Biome, EBiomeType::Bog);
    TestFalse(TEXT("Land-only cell (outside the water region) is not water"), LandOnly->bIsWater);

    Manager->Destroy();
    LandRegion->Destroy();
    WaterRegion->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistWaterRegion_NoResourcesSpawnOnExplicitWater,
    "Herbalist.WaterRegion.NoResourcesSpawnOnExplicitWater",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistWaterRegion_NoResourcesSpawnOnExplicitWater::RunTest(const FString& Parameters)
{
    // Клетка, залитая явным регионом воды, обязана вести себя как обычная
    // водная клетка -- её засевает только аквапул водных растений
    // (SpawnResourcesInCell), а у editor-мира автотеста реестра ингредиентов нет.
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    ABiomeRegionVolume* LandRegion = SpawnRegionCoveringWorldRect(World, EBiomeType::Bog, -50.f, -50.f, 950.f, 950.f);
    AWaterRegionVolume* WaterRegion = SpawnWaterRegionCoveringWorldRect(World, -50.f, -50.f, 450.f, 450.f);
    if (!TestNotNull(TEXT("Land region spawned"), LandRegion) || !TestNotNull(TEXT("Water region spawned"), WaterRegion))
        return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World, { LandRegion, WaterRegion });
    if (!TestNotNull(TEXT("Manager spawned"), Manager))
    {
        LandRegion->Destroy(); WaterRegion->Destroy(); return false;
    }

    const FGridCell* UnderWater = Manager->GetCellConst(2, 2);
    if (TestNotNull(TEXT("Cell under water region exists"), UnderWater))
    {
        TestEqual(TEXT("No resources spawned on an explicit-water cell"), UnderWater->ResourceActors.Num(), 0);
    }

    Manager->Destroy();
    LandRegion->Destroy();
    WaterRegion->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistWaterRegion_NoWaterWithoutWaterRegions,
    "Herbalist.WaterRegion.NoWaterWithoutWaterRegions",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistWaterRegion_NoWaterWithoutWaterRegions::RunTest(const FString& Parameters)
{
    // Раньше вода ложилась пятнами: 20% клеток без регионов, WaterDensity на
    // земляных регионах. Теперь вода только там, где нарисован регион воды.
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("Manager spawned"), Manager)) return false;
    TestTrue(TEXT("Sanity: сетка не пуста"), Manager->GetGridCellCount() > 0);
    TestEqual(TEXT("Без регионов -- ни одной водной клетки"), CountWaterCellsInGrid(Manager), 0);
    Manager->Destroy();

    ABiomeRegionVolume* LandRegion = SpawnRegionCoveringWorldRect(World, EBiomeType::Bog, -50.f, -50.f, 1950.f, 1950.f);
    if (!TestNotNull(TEXT("Land region spawned"), LandRegion)) return false;
    Manager = SpawnAndBeginPlay(World, { LandRegion });
    if (!TestNotNull(TEXT("Manager with land region spawned"), Manager)) { LandRegion->Destroy(); return false; }
    TestEqual(TEXT("Земляной регион без региона воды -- ни одной водной клетки"), CountWaterCellsInGrid(Manager), 0);

    Manager->Destroy();
    LandRegion->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistWaterRegion_WaterMixesAtBiomeBorder,
    "Herbalist.WaterRegion.WaterMixesAtBiomeBorder",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistWaterRegion_WaterMixesAtBiomeBorder::RunTest(const FString& Parameters)
{
    // Болото X 0..9, Степь X 5..14 -- перекрытие X 5..9 с долями 0.5/0.5.
    // Регион воды поперёк обоих, Y 0..4. У editor-мира автотеста нет
    // GameInstance и реестра воды, поэтому состояние воды -- из умолчаний воды
    // биомов (смесь с реестром -- следующий тест).
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    const FRealState BogWater = FBiomeDefaults::GetDefaultWaterState(EBiomeType::Bog);
    const FRealState SteppeWater = FBiomeDefaults::GetDefaultWaterState(EBiomeType::Steppe);
    if (!TestFalse(TEXT("Sanity: вода Болота и Степи различается -- смеси есть что показать"), IsSameWaterMeta(BogWater.Meta, SteppeWater.Meta)))
    {
        return false;
    }
    const FMeta Mixed = MixWaterMetaHalfAndHalf(BogWater.Meta, SteppeWater.Meta);

    ABiomeRegionVolume* BogRegion = SpawnRegionCoveringWorldRect(World, EBiomeType::Bog, -50.f, -50.f, 950.f, 1950.f);
    ABiomeRegionVolume* SteppeRegion = SpawnRegionCoveringWorldRect(World, EBiomeType::Steppe, 450.f, -50.f, 1450.f, 1950.f);
    AWaterRegionVolume* WaterRegion = SpawnWaterRegionCoveringWorldRect(World, -50.f, -50.f, 1450.f, 450.f);
    const auto DestroyRegions = [BogRegion, SteppeRegion, WaterRegion]()
    {
        if (BogRegion) BogRegion->Destroy();
        if (SteppeRegion) SteppeRegion->Destroy();
        if (WaterRegion) WaterRegion->Destroy();
    };
    if (!TestNotNull(TEXT("Bog region spawned"), BogRegion) || !TestNotNull(TEXT("Steppe region spawned"), SteppeRegion)
        || !TestNotNull(TEXT("Water region spawned"), WaterRegion))
    {
        DestroyRegions();
        return false;
    }

    AGridWorldManager* Manager = SpawnAndBeginPlay(World, { BogRegion, SteppeRegion, WaterRegion });
    if (!TestNotNull(TEXT("Manager spawned"), Manager)) { DestroyRegions(); return false; }

    const FGridCell* BogOnly = Manager->GetCellConst(2, 2);
    const FGridCell* Border = Manager->GetCellConst(7, 2);
    const FGridCell* SteppeOnly = Manager->GetCellConst(12, 2);
    const FGridCell* Dry = Manager->GetCellConst(7, 7);
    if (!TestNotNull(TEXT("Cell (2, 2)"), BogOnly) || !TestNotNull(TEXT("Cell (7, 2)"), Border)
        || !TestNotNull(TEXT("Cell (12, 2)"), SteppeOnly) || !TestNotNull(TEXT("Cell (7, 7)"), Dry)
        || !TestEqual(TEXT("Sanity: клетка (7, 2) в перекрытии двух биомов"), Border->BiomeWeights.Num(), 2))
    {
        Manager->Destroy();
        DestroyRegions();
        return false;
    }

    TestTrue(TEXT("Вода в регионе воды на всех трёх клетках"), BogOnly->bIsWater && Border->bIsWater && SteppeOnly->bIsWater);
    TestFalse(TEXT("Вне региона воды -- суша"), Dry->bIsWater);
    TestTrue(TEXT("Только Болото -- вода Болота"), IsSameWaterMeta(BogOnly->State.Meta, BogWater.Meta));
    TestTrue(TEXT("Только Степь -- вода Степи"), IsSameWaterMeta(SteppeOnly->State.Meta, SteppeWater.Meta));
    TestTrue(TEXT("На стыке -- вода пополам"), IsSameWaterMeta(Border->State.Meta, Mixed));
    TestTrue(TEXT("...и цель релаксации та же"), IsSameWaterMeta(Border->TargetState.Meta, Mixed));
    TestTrue(TEXT("Умолчание клетки на стыке -- та же смесь: Морок, Заряна и выход из полюса тянут к ней"),
        IsSameWaterMeta(AGridWorldManager::GetCellDefaultState(*Border).Meta, Mixed));
    TestEqual(TEXT("Биом клетки на стыке -- доминанта (равные доли -- меньший номер, Степь)"), Border->Biome, EBiomeType::Steppe);
    TestTrue(TEXT("Умолчание суши не смешивается"),
        IsSameWaterMeta(AGridWorldManager::GetCellDefaultState(*Dry).Meta, FBiomeDefaults::GetDefaultState(Dry->Biome).Meta));

    Manager->Destroy();
    DestroyRegions();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistWaterRegion_WaterTypesOfEachBiomeMix,
    "Herbalist.WaterRegion.WaterTypesOfEachBiomeMix",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistWaterRegion_WaterTypesOfEachBiomeMix::RunTest(const FString& Parameters)
{
    // Синтетический реестр, как в CellRandomStreamTest.cpp: у Тайги и Болота
    // по одному типу воды с разными базовыми величинами. Клетка на стыке
    // пополам получает среднее двух типов; тип воды для сбора -- доминирующего
    // биома.
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    UDataTable* Table = NewObject<UDataTable>();
    Table->RowStruct = FWaterTypeRow::StaticStruct();
    FWaterTypeRow TaigaRow;
    TaigaRow.WaterTypeID = FName(TEXT("MixTestTaigaWater"));
    TaigaRow.AllowedBiomes.Add(EBiomeType::Taiga);
    TaigaRow.BasePurity = 0.2f;
    TaigaRow.BaseDistortion = 0.4f;
    TaigaRow.BaseStability = 0.6f;
    TaigaRow.BasePotency = 0.8f;
    TaigaRow.BaseCorruption = 0.1f;
    Table->AddRow(TaigaRow.WaterTypeID, TaigaRow);
    FWaterTypeRow BogRow;
    BogRow.WaterTypeID = FName(TEXT("MixTestBogWater"));
    BogRow.AllowedBiomes.Add(EBiomeType::Bog);
    BogRow.BasePurity = 0.6f;
    BogRow.BaseDistortion = 0.0f;
    BogRow.BaseStability = 0.2f;
    BogRow.BasePotency = 0.4f;
    BogRow.BaseCorruption = 0.5f;
    Table->AddRow(BogRow.WaterTypeID, BogRow);
    UWaterTypeRegistrySubsystem* Water = NewObject<UWaterTypeRegistrySubsystem>(NewObject<UGameInstance>(GEngine));
    Water->LoadFromDataTable(Table);

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;
    const FGridCell* Source = Manager->GetCellConst(3, 3);
    if (!TestNotNull(TEXT("Cell (3, 3)"), Source)) { Manager->Destroy(); return false; }

    FGridCell Cell = *Source;
    Cell.Biome = EBiomeType::Taiga;
    Cell.BiomeWeights = { FBiomeWeightEntry{ EBiomeType::Taiga, 0.5f }, FBiomeWeightEntry{ EBiomeType::Bog, 0.5f } };
    const FRealState Mixed = Manager->RollWaterStateForCell(Cell, Water);
    TestEqual(TEXT("Purity -- среднее 0.2 и 0.6"), Mixed.Meta.Purity, 0.4f, 1e-5f);
    TestEqual(TEXT("Distortion -- среднее 0.4 и 0"), Mixed.Meta.Distortion, 0.2f, 1e-5f);
    TestEqual(TEXT("Stability -- среднее 0.6 и 0.2"), Mixed.Meta.Stability, 0.4f, 1e-5f);
    TestEqual(TEXT("Potency -- среднее 0.8 и 0.4"), Mixed.Meta.Potency, 0.6f, 1e-5f);
    TestEqual(TEXT("Corruption -- среднее 0.1 и 0.5"), Mixed.Meta.Corruption, 0.3f, 1e-5f);
    // Резонанс реестр не задаёт -- смесь умолчаний воды биомов.
    const float ExpectedResonance = 0.5f * FBiomeDefaults::GetDefaultWaterState(EBiomeType::Taiga).Meta.Resonance
        + 0.5f * FBiomeDefaults::GetDefaultWaterState(EBiomeType::Bog).Meta.Resonance;
    TestEqual(TEXT("Resonance -- смесь умолчаний воды"), Mixed.Meta.Resonance, ExpectedResonance, 1e-5f);
    TestEqual(TEXT("Тип воды для сбора -- доминирующего биома"), Manager->RollWaterTypeForCell(Cell, Water), FName(TEXT("MixTestTaigaWater")));

    // Одна доля -- вода своего биома без смешивания.
    Cell.Biome = EBiomeType::Bog;
    Cell.BiomeWeights = { FBiomeWeightEntry{ EBiomeType::Bog, 1.0f } };
    const FRealState BogOnly = Manager->RollWaterStateForCell(Cell, Water);
    TestEqual(TEXT("Одна доля: Purity типа воды Болота"), BogOnly.Meta.Purity, 0.6f);
    TestEqual(TEXT("Одна доля: Corruption типа воды Болота"), BogOnly.Meta.Corruption, 0.5f);

    Manager->Destroy();
    return true;
}

#endif // WITH_AUTOMATION_TESTS && WITH_EDITOR
