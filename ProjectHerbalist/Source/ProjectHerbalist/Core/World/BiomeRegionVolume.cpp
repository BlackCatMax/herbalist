// Core/World/BiomeRegionVolume.cpp
#include "BiomeRegionVolume.h"
#include "Components/SplineComponent.h"

ABiomeRegionVolume::ABiomeRegionVolume()
{
    PrimaryActorTick.bCanEverTick = false;

    SplineComponent = CreateDefaultSubobject<USplineComponent>(TEXT("SplineComponent"));
    RootComponent = SplineComponent;
    SplineComponent->SetClosedLoop(true);
}

void ABiomeRegionVolume::BeginPlay()
{
    Super::BeginPlay();
    UpdateCachedPoints();
}

void ABiomeRegionVolume::UpdateCachedPoints()
{
    CachedPoints2D.Empty();
    CachedCentroid = FVector2D::ZeroVector;
    CachedMaxRadiusFromCentroid = 0.0f;

    if (!SplineComponent || SplineComponent->GetSplineLength() <= 0.0f)
        return;

    const float Length = SplineComponent->GetSplineLength();
    const int32 NumPoints = FMath::Max(3, SplineResolution);
    const float Step = Length / NumPoints;

    for (int32 i = 0; i < NumPoints; i++)
    {
        const float Distance = Step * i;
        const FVector Location = SplineComponent->GetLocationAtDistanceAlongSpline(Distance, ESplineCoordinateSpace::World);
        CachedPoints2D.Add(FVector2D(Location.X, Location.Y));
    }

    // Центроид + макс. радиус для GetNormalizedDistanceFromCenter (2026-09-03)
    // -- посчитаны один раз здесь, не на каждый запрос затухания (по разу на
    // клетку/актора при заселении региона, а не дороже).
    for (const FVector2D& Point : CachedPoints2D)
    {
        CachedCentroid += Point;
    }
    CachedCentroid /= CachedPoints2D.Num();

    for (const FVector2D& Point : CachedPoints2D)
    {
        CachedMaxRadiusFromCentroid = FMath::Max(CachedMaxRadiusFromCentroid, FVector2D::Distance(CachedCentroid, Point));
    }
}

void ABiomeRegionVolume::EnsureCachedPoints() const
{
    // Порядок BeginPlay между акторами уровня не гарантирован UE — регион
    // мог быть опрошен GridWorldManager'ом раньше, чем отработал его
    // собственный BeginPlay. GridWorldManager и так вызывает
    // UpdateCachedPoints() явно на каждом найденном регионе (см.
    // InitializeCells), это — второй, ленивый рубеж защиты для любого
    // другого вызывающего (тесты, Blueprint), не полагающегося на этот
    // порядок явно.
    if (CachedPoints2D.Num() == 0)
    {
        const_cast<ABiomeRegionVolume*>(this)->UpdateCachedPoints();
    }
}

bool ABiomeRegionVolume::IsPointInside(const FVector& Point) const
{
    EnsureCachedPoints();

    if (CachedPoints2D.Num() < 3)
        return false;

    // Ray-cast point-in-polygon (чётность пересечений), 2D X-Y — та же
    // математика, что в T:\IDOL\...\SplineTriggerVolume.cpp::IsPointInside,
    // без Z-ветки оригинала (наша сетка плоская, не нужна).
    const FVector2D TestPoint(Point.X, Point.Y);
    bool bInside = false;
    int32 j = CachedPoints2D.Num() - 1;

    for (int32 i = 0; i < CachedPoints2D.Num(); i++)
    {
        const FVector2D& Pi = CachedPoints2D[i];
        const FVector2D& Pj = CachedPoints2D[j];

        if (((Pi.Y > TestPoint.Y) != (Pj.Y > TestPoint.Y)) &&
            (TestPoint.X < (Pj.X - Pi.X) * (TestPoint.Y - Pi.Y) / (Pj.Y - Pi.Y) + Pi.X))
        {
            bInside = !bInside;
        }
        j = i;
    }

    return bInside;
}

float ABiomeRegionVolume::GetNormalizedDistanceFromCenter(const FVector& Point) const
{
    EnsureCachedPoints();

    if (CachedMaxRadiusFromCentroid <= KINDA_SMALL_NUMBER)
        return 0.0f;   // вырожденный/ещё не построенный регион -- нейтрально, не делить на ~0

    const float Dist = FVector2D::Distance(CachedCentroid, FVector2D(Point.X, Point.Y));
    return FMath::Clamp(Dist / CachedMaxRadiusFromCentroid, 0.0f, 1.0f);
}

FRandomPlacementTransform ABiomeRegionVolume::RollPlacementTransform(const FVector& BasePosition, FRandomStream& Rng) const
{
    FRandomPlacementTransform Result;

    Result.UniformScale = Rng.FRandRange(FMath::Min(MinUniformScale, MaxUniformScale), FMath::Max(MinUniformScale, MaxUniformScale));

    const float Yaw = Rng.FRandRange(FMath::Min(MinYawDegrees, MaxYawDegrees), FMath::Max(MinYawDegrees, MaxYawDegrees));
    const float Pitch = Rng.FRandRange(FMath::Min(MinTiltDegrees, MaxTiltDegrees), FMath::Max(MinTiltDegrees, MaxTiltDegrees));
    const float Roll = Rng.FRandRange(FMath::Min(MinTiltDegrees, MaxTiltDegrees), FMath::Max(MinTiltDegrees, MaxTiltDegrees));
    Result.Rotation = FRotator(Pitch, Yaw, Roll);

    Result.PositionOffset = FVector(
        Rng.FRandRange(FMath::Min(MinPositionOffset.X, MaxPositionOffset.X), FMath::Max(MinPositionOffset.X, MaxPositionOffset.X)),
        Rng.FRandRange(FMath::Min(MinPositionOffset.Y, MaxPositionOffset.Y), FMath::Max(MinPositionOffset.Y, MaxPositionOffset.Y)),
        Rng.FRandRange(FMath::Min(MinPositionOffset.Z, MaxPositionOffset.Z), FMath::Max(MinPositionOffset.Z, MaxPositionOffset.Z)));

    // Затухание скейла к границе -- см. довод у ScaleFalloffStrength в .h.
    // 0 (дефолт) -- множитель всегда 1, поведение не меняется.
    if (ScaleFalloffStrength > 0.0f)
    {
        const float T = GetNormalizedDistanceFromCenter(BasePosition);
        Result.UniformScale *= FMath::Lerp(1.0f, 1.0f - T, ScaleFalloffStrength);
    }

    return Result;
}
