// Source/ProjectHerbalistTests/Private/Tests/BiomeRegionVolumeTest.cpp
//
// PCG-биомы (2026-08-31): point-in-polygon в изоляции, без GridWorldManager
// — проверяет ровно алгоритм, адаптированный из пользовательского
// SplineTriggerVolume (T:\IDOL\...), не его интеграцию (та проверяется
// отдельно в GridWorldManagerBiomeRegionTest.cpp).

#include "Core/World/BiomeRegionVolume.h"
#include "Components/SplineComponent.h"
#include "Misc/AutomationTest.h"
#include "Editor.h"
#include "Engine/World.h"

#if WITH_AUTOMATION_TESTS && WITH_EDITOR

namespace
{
    // Квадрат 200x200 с центром в начале координат мира, Z произвольная —
    // алгоритм 2D (X-Y), высота не участвует.
    ABiomeRegionVolume* SpawnSquareRegion(UWorld* World)
    {
        ABiomeRegionVolume* Region = World->SpawnActor<ABiomeRegionVolume>();
        if (!Region) return nullptr;

        USplineComponent* Spline = Region->FindComponentByClass<USplineComponent>();
        if (!Spline) return Region;

        const TArray<FVector> Corners = {
            FVector(-100.0f, -100.0f, 0.0f),
            FVector( 100.0f, -100.0f, 0.0f),
            FVector( 100.0f,  100.0f, 0.0f),
            FVector(-100.0f,  100.0f, 0.0f),
        };
        Spline->SetSplinePoints(Corners, ESplineCoordinateSpace::World, true);
        Spline->SetClosedLoop(true);
        return Region;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistBiomeRegion_SquareContainsCenterNotOutside,
    "Herbalist.BiomeRegion.SquareContainsCenterNotOutside",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistBiomeRegion_SquareContainsCenterNotOutside::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    ABiomeRegionVolume* Region = SpawnSquareRegion(World);
    if (!TestNotNull(TEXT("Region spawned"), Region)) return false;

    Region->UpdateCachedPoints();

    TestTrue(TEXT("Center of the square is inside"), Region->IsPointInside(FVector(0.0f, 0.0f, 500.0f)));
    TestFalse(TEXT("A point clearly outside the square is not inside"), Region->IsPointInside(FVector(1000.0f, 1000.0f, 0.0f)));

    Region->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistBiomeRegion_SelfHealingCacheWorksWithoutExplicitUpdate,
    "Herbalist.BiomeRegion.SelfHealingCacheWorksWithoutExplicitUpdate",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistBiomeRegion_SelfHealingCacheWorksWithoutExplicitUpdate::RunTest(const FString& Parameters)
{
    // UE не гарантирует порядок BeginPlay между акторами уровня --
    // GridWorldManager может опросить регион раньше, чем отработает его
    // BeginPlay. Регрессия ровно на этот случай: НЕ вызываем
    // UpdateCachedPoints() явно (SpawnActor в editor-мире не гоняет
    // BeginPlay сам по себе -- кэш точно пуст на старте) -- IsPointInside
    // обязана пересчитать кэш сама, не молчать false из-за пустого кэша.
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    ABiomeRegionVolume* Region = SpawnSquareRegion(World);
    if (!TestNotNull(TEXT("Region spawned"), Region)) return false;

    TestTrue(TEXT("IsPointInside self-heals its cache and gives the correct answer even without an explicit UpdateCachedPoints() call"),
        Region->IsPointInside(FVector(0.0f, 0.0f, 0.0f)));

    Region->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistBiomeRegion_DegenerateSplineIsNeverInsideAndDoesNotCrash,
    "Herbalist.BiomeRegion.DegenerateSplineIsNeverInsideAndDoesNotCrash",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistBiomeRegion_DegenerateSplineIsNeverInsideAndDoesNotCrash::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    // Регион спавнится с дефолтным сплайном движка (обычно 2 совпадающие
    // или очень близкие точки, нулевая/почти нулевая длина) -- ни один
    // Corners-массив не задаём намеренно.
    ABiomeRegionVolume* Region = World->SpawnActor<ABiomeRegionVolume>();
    if (!TestNotNull(TEXT("Region spawned"), Region)) return false;

    USplineComponent* Spline = Region->FindComponentByClass<USplineComponent>();
    if (Spline)
    {
        Spline->ClearSplinePoints(true);
    }
    Region->UpdateCachedPoints();

    TestFalse(TEXT("A degenerate (zero-length) spline contains nothing, doesn't crash"),
        Region->IsPointInside(FVector::ZeroVector));

    Region->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistBiomeRegion_SetSplinePointsWorldMatchesManualSpline,
    "Herbalist.BiomeRegion.SetSplinePointsWorldMatchesManualSpline",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistBiomeRegion_SetSplinePointsWorldMatchesManualSpline::RunTest(const FString& Parameters)
{
    // SetSplinePointsWorld (2026-09-06, "сделай тестовую карту в движке" --
    // хук для процедурной/коммандлет-сборки уровня) должна давать тот же
    // результат, что и ручная расстановка через сам USplineComponent
    // (см. SpawnSquareRegion выше) -- тот же квадрат, другой путь установки.
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    ABiomeRegionVolume* Region = World->SpawnActor<ABiomeRegionVolume>();
    if (!TestNotNull(TEXT("Region spawned"), Region)) return false;

    const TArray<FVector> Corners = {
        FVector(-100.0f, -100.0f, 0.0f),
        FVector( 100.0f, -100.0f, 0.0f),
        FVector( 100.0f,  100.0f, 0.0f),
        FVector(-100.0f,  100.0f, 0.0f),
    };
    Region->SetSplinePointsWorld(Corners);

    TestTrue(TEXT("Center of the square is inside"), Region->IsPointInside(FVector(0.0f, 0.0f, 500.0f)));
    TestFalse(TEXT("A point clearly outside the square is not inside"), Region->IsPointInside(FVector(1000.0f, 1000.0f, 0.0f)));

    // Меньше 3 точек -- явный отказ, не крах и не тихая порча существующей формы.
    Region->SetSplinePointsWorld({ FVector(0,0,0), FVector(1,1,0) });
    TestTrue(TEXT("Fewer than 3 points: previous valid shape is left untouched"), Region->IsPointInside(FVector(0.0f, 0.0f, 500.0f)));

    Region->Destroy();
    return true;
}

#endif // WITH_AUTOMATION_TESTS && WITH_EDITOR
