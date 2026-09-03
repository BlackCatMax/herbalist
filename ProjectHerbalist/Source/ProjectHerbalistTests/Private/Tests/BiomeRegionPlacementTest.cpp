// Source/ProjectHerbalistTests/Private/Tests/BiomeRegionPlacementTest.cpp
//
// Случайная трансформация ресурсов региона (2026-09-03, прямой запрос
// пользователя после починки тайлинга джиттера: "как у PCG в ноде
// Transform" -- скейл, поворот по Z 0..360°, лёгкий завал по X/Y ±5°,
// доп.смещение, затухание плотности/скейла к границе региона).
//
// Плотность (SpawnResourcesInCell) здесь НЕ тестируется напрямую: тестовый
// мир редактора не имеет UGameInstance, GetSubsystem<UIngredientRegistrySubsystem>
// возвращает null, и ни один актор не заспавнится вообще, что бы ни стояло
// в NumResources -- та же граница, что уже задокументирована у
// RegistryLazyLoadTest.cpp/PcgResourcePlacementTest.cpp. Формула затухания
// (GetNormalizedDistanceFromCenter + Lerp) у плотности и у скейла ровно
// одна и та же -- тесты ниже покрывают её через RollPlacementTransform,
// которую тестировать можно без мира вообще.

#include "Core/World/BiomeRegionVolume.h"
#include "Core/Types/BiomeTypes.h"
#include "Misc/AutomationTest.h"
#include "Editor.h"
#include "Engine/World.h"
#include "Math/RandomStream.h"

#if WITH_AUTOMATION_TESTS && WITH_EDITOR

#include "TestWorldHelpers.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistBiomeRegion_DefaultRangesMatchWhatWasRequested,
    "Herbalist.BiomeRegion.DefaultRangesMatchWhatWasRequested",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistBiomeRegion_DefaultRangesMatchWhatWasRequested::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    ABiomeRegionVolume* Region = SpawnRegionCoveringWorldRect(World, EBiomeType::Taiga, 0.0f, 0.0f, 1000.0f, 1000.0f);
    if (!TestNotNull(TEXT("Region spawned"), Region)) return false;

    // Yaw -- полный круг по умолчанию (запрошено дословно: "0..360").
    TestEqual(TEXT("MinYawDegrees default"), Region->MinYawDegrees, 0.0f);
    TestEqual(TEXT("MaxYawDegrees default"), Region->MaxYawDegrees, 360.0f);

    // Завал по X/Y -- ±5° по умолчанию (запрошено дословно).
    TestEqual(TEXT("MinTiltDegrees default"), Region->MinTiltDegrees, -5.0f);
    TestEqual(TEXT("MaxTiltDegrees default"), Region->MaxTiltDegrees, 5.0f);

    // Затухание и доп.смещение -- выключены по умолчанию (запрошены как
    // "возможность", не как всегда применяемый эффект).
    TestEqual(TEXT("DensityFalloffStrength default is off"), Region->DensityFalloffStrength, 0.0f);
    TestEqual(TEXT("ScaleFalloffStrength default is off"), Region->ScaleFalloffStrength, 0.0f);
    TestEqual(TEXT("MinPositionOffset default is zero"), Region->MinPositionOffset, FVector::ZeroVector);
    TestEqual(TEXT("MaxPositionOffset default is zero"), Region->MaxPositionOffset, FVector::ZeroVector);

    Region->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistBiomeRegion_NormalizedDistanceIsZeroAtCentroidAndOneAtCorner,
    "Herbalist.BiomeRegion.NormalizedDistanceIsZeroAtCentroidAndOneAtCorner",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistBiomeRegion_NormalizedDistanceIsZeroAtCentroidAndOneAtCorner::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    // Квадрат (0,0)-(1000,1000) -- центроид (500,500), макс. радиус до
    // угла = 500*sqrt(2) ≈ 707.1. Известная геометрия, не сплайн от руки.
    ABiomeRegionVolume* Region = SpawnRegionCoveringWorldRect(World, EBiomeType::Taiga, 0.0f, 0.0f, 1000.0f, 1000.0f);
    if (!TestNotNull(TEXT("Region spawned"), Region)) return false;

    const float AtCentroid = Region->GetNormalizedDistanceFromCenter(FVector(500.0f, 500.0f, 0.0f));
    const float AtCorner = Region->GetNormalizedDistanceFromCenter(FVector(0.0f, 0.0f, 0.0f));
    const float AtEdgeMidpoint = Region->GetNormalizedDistanceFromCenter(FVector(500.0f, 0.0f, 0.0f));

    TestTrue(FString::Printf(TEXT("At centroid ~0 (got %f)"), AtCentroid), FMath::IsNearlyEqual(AtCentroid, 0.0f, 0.01f));
    TestTrue(FString::Printf(TEXT("At the farthest corner ~1 (got %f)"), AtCorner), FMath::IsNearlyEqual(AtCorner, 1.0f, 0.01f));
    // 500 / 707.1 ≈ 0.707 -- edge midpoint is closer than a corner.
    TestTrue(FString::Printf(TEXT("At edge midpoint ~0.707 (got %f)"), AtEdgeMidpoint), FMath::IsNearlyEqual(AtEdgeMidpoint, 0.7071f, 0.01f));

    Region->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistBiomeRegion_RollPlacementTransformStaysWithinConfiguredRanges,
    "Herbalist.BiomeRegion.RollPlacementTransformStaysWithinConfiguredRanges",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistBiomeRegion_RollPlacementTransformStaysWithinConfiguredRanges::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    ABiomeRegionVolume* Region = SpawnRegionCoveringWorldRect(World, EBiomeType::Taiga, 0.0f, 0.0f, 1000.0f, 1000.0f);
    if (!TestNotNull(TEXT("Region spawned"), Region)) return false;

    Region->MinUniformScale = 0.5f;
    Region->MaxUniformScale = 2.0f;
    Region->MinYawDegrees = 10.0f;
    Region->MaxYawDegrees = 20.0f;
    Region->MinTiltDegrees = -2.0f;
    Region->MaxTiltDegrees = 2.0f;
    Region->MinPositionOffset = FVector(10.0f, 10.0f, 10.0f);
    Region->MaxPositionOffset = FVector(20.0f, 20.0f, 20.0f);

    FRandomStream Rng(20260903);
    TSet<float> DistinctScales;
    bool bAllWithinRanges = true;

    for (int32 i = 0; i < 200; ++i)
    {
        const FRandomPlacementTransform Xform = Region->RollPlacementTransform(FVector(500.0f, 500.0f, 0.0f), Rng);
        DistinctScales.Add(Xform.UniformScale);

        if (Xform.UniformScale < 0.5f || Xform.UniformScale > 2.0f) bAllWithinRanges = false;
        if (Xform.Rotation.Yaw < 10.0f || Xform.Rotation.Yaw > 20.0f) bAllWithinRanges = false;
        if (Xform.Rotation.Pitch < -2.0f || Xform.Rotation.Pitch > 2.0f) bAllWithinRanges = false;
        if (Xform.Rotation.Roll < -2.0f || Xform.Rotation.Roll > 2.0f) bAllWithinRanges = false;
        if (Xform.PositionOffset.X < 10.0f || Xform.PositionOffset.X > 20.0f) bAllWithinRanges = false;
        if (Xform.PositionOffset.Y < 10.0f || Xform.PositionOffset.Y > 20.0f) bAllWithinRanges = false;
        if (Xform.PositionOffset.Z < 10.0f || Xform.PositionOffset.Z > 20.0f) bAllWithinRanges = false;
    }

    TestTrue(TEXT("Every rolled value stays within its configured Min/Max"), bAllWithinRanges);
    // Реально случайно, не одно и то же число 200 раз подряд.
    TestTrue(TEXT("Rolls actually vary (more than one distinct scale seen)"), DistinctScales.Num() > 1);

    Region->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistBiomeRegion_ScaleFalloffShrinksNearTheEdge,
    "Herbalist.BiomeRegion.ScaleFalloffShrinksNearTheEdge",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistBiomeRegion_ScaleFalloffShrinksNearTheEdge::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    ABiomeRegionVolume* Region = SpawnRegionCoveringWorldRect(World, EBiomeType::Taiga, 0.0f, 0.0f, 1000.0f, 1000.0f);
    if (!TestNotNull(TEXT("Region spawned"), Region)) return false;

    // Скейл фиксирован (Min==Max) -- вся разница ниже только от затухания,
    // не от случайного разброса самого скейла.
    Region->MinUniformScale = 1.0f;
    Region->MaxUniformScale = 1.0f;
    Region->ScaleFalloffStrength = 1.0f;   // максимальное -- у самого края скейл уходит в 0

    FRandomStream Rng(20260903);
    const float ScaleAtCentroid = Region->RollPlacementTransform(FVector(500.0f, 500.0f, 0.0f), Rng).UniformScale;
    const float ScaleAtCorner = Region->RollPlacementTransform(FVector(0.0f, 0.0f, 0.0f), Rng).UniformScale;

    TestTrue(FString::Printf(TEXT("At centroid, scale stays near 1.0 (got %f)"), ScaleAtCentroid),
        FMath::IsNearlyEqual(ScaleAtCentroid, 1.0f, 0.01f));
    TestTrue(FString::Printf(TEXT("At the farthest corner, scale shrinks toward 0 (got %f)"), ScaleAtCorner),
        FMath::IsNearlyEqual(ScaleAtCorner, 0.0f, 0.01f));

    Region->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistBiomeRegion_ZeroFalloffStrengthMeansNoEffect,
    "Herbalist.BiomeRegion.ZeroFalloffStrengthMeansNoEffect",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistBiomeRegion_ZeroFalloffStrengthMeansNoEffect::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    ABiomeRegionVolume* Region = SpawnRegionCoveringWorldRect(World, EBiomeType::Taiga, 0.0f, 0.0f, 1000.0f, 1000.0f);
    if (!TestNotNull(TEXT("Region spawned"), Region)) return false;

    Region->MinUniformScale = 1.0f;
    Region->MaxUniformScale = 1.0f;
    Region->ScaleFalloffStrength = 0.0f;   // дефолт -- регрессия на "не ломает старые регионы"

    FRandomStream Rng(20260903);
    const float ScaleAtCentroid = Region->RollPlacementTransform(FVector(500.0f, 500.0f, 0.0f), Rng).UniformScale;
    const float ScaleAtCorner = Region->RollPlacementTransform(FVector(0.0f, 0.0f, 0.0f), Rng).UniformScale;

    TestEqual(TEXT("Falloff strength 0 -- scale at centroid unaffected"), ScaleAtCentroid, 1.0f);
    TestEqual(TEXT("Falloff strength 0 -- scale at the corner equally unaffected"), ScaleAtCorner, 1.0f);

    Region->Destroy();
    return true;
}

#endif // WITH_AUTOMATION_TESTS && WITH_EDITOR
