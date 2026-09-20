// AmbientEntityActor.cpp
#include "Core/Entities/AmbientEntityActor.h"
#include "Core/Config/HerbalistSettings.h"

AAmbientEntityActor::AAmbientEntityActor()
{
    // Тикает только Низший, и только когда ему задали зону брожения
    // (ревью 2026-09-20: до этого тикал КАЖДЫЙ Низший, включая поставленных
    // прежним клеточным путём, где брожения нет вовсе). Хозяева мест и
    // Легендарные привязаны к своей клетке и тика не просят вообще.
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = false;
}

void AAmbientEntityActor::SetWanderZone(const FVector& InCenter, float InRadiusCm, int32 InSeed)
{
    WanderCenter = InCenter;
    WanderRadiusCm = FMath::Max(0.0f, InRadiusCm);
    bHasWanderZone = WanderRadiusCm > 0.0f;
    WanderRng = FRandomStream(InSeed);
    SetActorTickEnabled(bHasWanderZone);
    PickNextWanderTarget();
}

void AAmbientEntityActor::PickNextWanderTarget()
{
    if (!bHasWanderZone)
    {
        WanderTarget = GetActorLocation();
        return;
    }

    // Равномерно по площади круга (корень из случайной доли), не по радиусу:
    // иначе особи толпились бы у центра зоны.
    const float Angle = WanderRng.FRandRange(0.0f, 2.0f * PI);
    const float Distance = WanderRadiusCm * FMath::Sqrt(WanderRng.FRand());
    WanderTarget = WanderCenter + FVector(FMath::Cos(Angle) * Distance, FMath::Sin(Angle) * Distance, 0.0f);
    // Высоту не считаем: она берётся от центра зоны (клетка спавнера). Рельеф
    // внутри зоны -- забота этапа с настоящим визуалом, не этого.
    WanderTarget.Z = WanderCenter.Z;

    const UHerbalistSettings* Settings = GetHerbalistSettings();
    const float Pause = Settings ? Settings->AmbientWanderPauseSeconds : 3.0f;
    // Пауза своя у каждой особи, иначе стайка шагала бы строем.
    PauseRemainingSeconds = Pause * WanderRng.FRand();
}

void AAmbientEntityActor::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (!bHasWanderZone) return;

    if (PauseRemainingSeconds > 0.0f)
    {
        PauseRemainingSeconds -= DeltaSeconds;
        return;
    }

    const UHerbalistSettings* Settings = GetHerbalistSettings();
    const float Speed = Settings ? Settings->AmbientWanderSpeedCmPerSecond : 60.0f;
    const FVector Location = GetActorLocation();
    const FVector ToTarget = WanderTarget - Location;
    const float Step = Speed * DeltaSeconds;

    if (ToTarget.SizeSquared() <= FMath::Square(Step))
    {
        SetActorLocation(WanderTarget);
        PickNextWanderTarget();
        return;
    }
    SetActorLocation(Location + ToTarget.GetSafeNormal() * Step);
}
