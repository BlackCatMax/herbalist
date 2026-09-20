// AmbientEntityActor.h
//
// Низший ранг бестиария (§16.2) как физический актор — 2026-08-30. Пустой
// маркерный под-класс: собственного поведения пока нет (см.
// HerbalistEntityActor.h), но нужен отдельный C++-тип, чтобы Низший ранг мог
// разойтись от Хозяев/Легендарных позже (декоративность, отсутствие
// взаимодействия по спецификации §16.2) без переписывания базового класса.
// Разворачивает решение "амбиентная зона, без актора" из
// 16_Entity_Manifestation.md — см. правку того файла в этом же коммите.
#pragma once

#include "CoreMinimal.h"
#include "Core/Entities/HerbalistEntityActor.h"
#include "AmbientEntityActor.generated.h"

UCLASS()
class PROJECTHERBALIST_API AAmbientEntityActor : public AHerbalistEntityActor
{
    GENERATED_BODY()

public:
    AAmbientEntityActor();

    // Зона спавнера, внутри которой особь бродит (DESIGN_Entity_Spawners.md,
    // решение пользователя 2026-09-20). Центр и радиус -- в сантиметрах
    // мира. Пока зона не задана, особь стоит на месте: так ведёт себя
    // существо, поставленное прежним клеточным путём.
    void SetWanderZone(const FVector& InCenter, float InRadiusCm, int32 InSeed);

    virtual void Tick(float DeltaSeconds) override;

private:
    // Следующая точка в зоне и пауза на ней.
    void PickNextWanderTarget();

    bool bHasWanderZone = false;
    FVector WanderCenter = FVector::ZeroVector;
    float WanderRadiusCm = 0.0f;
    FVector WanderTarget = FVector::ZeroVector;
    float PauseRemainingSeconds = 0.0f;

    // Своя случайность, не WorldRNG: брожение -- презентация, оно не должно
    // сдвигать исходы симуляции (тот же довод, что у джиттера актора
    // сущности в SyncManifestedEntityActor).
    FRandomStream WanderRng;
};
