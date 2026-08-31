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
