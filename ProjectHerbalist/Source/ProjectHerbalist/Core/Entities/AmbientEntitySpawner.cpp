// AmbientEntitySpawner.cpp
#include "Core/Entities/AmbientEntitySpawner.h"
#include "Core/World/GridWorldManager.h"
#include "Core/Config/HerbalistSettings.h"
#include "Components/SceneComponent.h"
#include "EngineUtils.h"

AAmbientEntitySpawner::AAmbientEntitySpawner()
{
    PrimaryActorTick.bCanEverTick = false;
    // Без корневого компонента актор не держит позицию вовсе: GetActorLocation
    // отдаёт ноль, и спавнер садится в начало координат (найдено тестом
    // ManualSpawnerOverridesAutoAndPicksItsOwnSpecies). Пустая сцена, не меш:
    // спавнер -- разметка уровня, видимого тела у него нет.
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

float AAmbientEntitySpawner::GetRadiusMeters() const
{
    if (RadiusMeters > 0.0f) return RadiusMeters;
    const UHerbalistSettings* Settings = GetHerbalistSettings();
    return Settings ? Settings->AmbientSpawnerRadiusMeters : 25.0f;
}

void AAmbientEntitySpawner::BeginPlay()
{
    Super::BeginPlay();

    // Тот же приём, что у капища: актор уровня сам говорит симуляции о себе,
    // а не симуляция ищет его обходом каждый тик. Порядок BeginPlay между
    // акторами уровня UE не гарантирует, поэтому менеджер принимает
    // регистрацию в любой момент (см. RegisterAmbientSpawner).
    for (TActorIterator<AGridWorldManager> It(GetWorld()); It; ++It)
    {
        RegisteredManager = *It;
        It->RegisterAmbientSpawner(this);
        break;
    }
}

void AAmbientEntitySpawner::EndPlay(const EEndPlayReason::Type Reason)
{
    if (AGridWorldManager* Manager = RegisteredManager.Get())
    {
        Manager->UnregisterAmbientSpawner(this);
    }
    Super::EndPlay(Reason);
}
