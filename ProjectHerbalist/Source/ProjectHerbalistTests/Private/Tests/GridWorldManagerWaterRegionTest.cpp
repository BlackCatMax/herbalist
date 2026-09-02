// Source/ProjectHerbalistTests/Private/Tests/GridWorldManagerWaterRegionTest.cpp
//
// AWaterRegionVolume -- вода как отдельный, явно нарисованный регион
// (2026-09-02, прямой запрос пользователя): "я вручную могу указывать, где
// будет вода, и она всегда превалирует над любым биомом... её вес всегда
// 1... биом воды, размещённый поверх других биомов, должен автоматически
// становиться тем типом воды, который для биома характерен".

#include "Core/World/GridWorldManager.h"
#include "Core/World/BiomeRegionVolume.h"
#include "Core/World/WaterRegionVolume.h"
#include "Core/Types/BiomeTypes.h"
#include "Misc/AutomationTest.h"
#include "Editor.h"
#include "Engine/World.h"

#if WITH_AUTOMATION_TESTS && WITH_EDITOR

#include "TestWorldHelpers.h"

namespace
{
    // Тот же приём, что SpawnRegionCoveringWorldRect в TestWorldHelpers.h,
    // но для AWaterRegionVolume -- не вынесено туда, единственный
    // потребитель пока этот файл.
    AWaterRegionVolume* SpawnWaterRegionCoveringWorldRect(UWorld* World,
        float MinX, float MinY, float MaxX, float MaxY)
    {
        if (!World) return nullptr;
        AWaterRegionVolume* Region = World->SpawnActor<AWaterRegionVolume>();
        if (!Region) return nullptr;

        if (USplineComponent* Spline = Region->FindComponentByClass<USplineComponent>())
        {
            const TArray<FVector> Corners = {
                FVector(MinX, MinY, 0.0f), FVector(MaxX, MinY, 0.0f),
                FVector(MaxX, MaxY, 0.0f), FVector(MinX, MaxY, 0.0f),
            };
            Spline->SetSplinePoints(Corners, ESplineCoordinateSpace::World, false);
            for (int32 i = 0; i < Corners.Num(); ++i)
            {
                Spline->SetSplinePointType(i, ESplinePointType::Linear, false);
            }
            Spline->SetClosedLoop(true, false);
            Spline->UpdateSpline();
        }
        Region->UpdateCachedPoints();
        return Region;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistWaterRegion_AlwaysWinsRegardlessOfLandWaterDensity,
    "Herbalist.WaterRegion.AlwaysWinsRegardlessOfLandWaterDensity",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistWaterRegion_AlwaysWinsRegardlessOfLandWaterDensity::RunTest(const FString& Parameters)
{
    // Земляной регион (Bog) на весь тестовый угол сетки, WaterDensity=0 --
    // если бы вода шла только через вероятностный density-механизм, здесь
    // не было бы ни капли. Регион воды поверх ЧАСТИ той же площади должен
    // безусловно залить её водой, вес 1, не смешиваясь с density=0.
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    ABiomeRegionVolume* LandRegion = SpawnRegionCoveringWorldRect(World, EBiomeType::Bog, -50.f, -50.f, 950.f, 950.f);
    if (!TestNotNull(TEXT("Land region spawned"), LandRegion)) return false;
    LandRegion->WaterDensity = 0.0f;

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

    TestTrue(TEXT("Cell under an explicit water region is water despite WaterDensity=0 on the land region"),
        UnderWater->bIsWater);
    TestEqual(TEXT("Cell.Biome under the water stays the land region's biome (Bog), water doesn't replace it"),
        UnderWater->Biome, EBiomeType::Bog);
    TestFalse(TEXT("Land-only cell (outside the water region, WaterDensity=0) is not water"),
        LandOnly->bIsWater);

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
    // водная клетка -- InitializeCells зовёт SpawnResourcesInCell только
    // для !Cell.bIsWater (см. GridWorldManagerCore.cpp).
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

#endif // WITH_AUTOMATION_TESTS && WITH_EDITOR
