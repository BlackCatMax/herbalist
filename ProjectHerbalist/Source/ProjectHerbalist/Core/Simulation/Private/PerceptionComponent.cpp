// Core/Simulation/Private/PerceptionComponent.cpp
#include "Core/Simulation/Public/PerceptionComponent.h"
#include "Core/Simulation/Public/SnapshotService.h"
#include "Core/Simulation/Private/PerceptionService.h"
#include "Core/World/GridWorldManager.h"
#include "Engine/World.h"
#include "EngineUtils.h"

UPerceptionComponent::UPerceptionComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickInterval = 0.5f; // обновляем дважды в секунду
}

void UPerceptionComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    // Захватываем реальный снапшот мира
    FWorldSnapshot WorldSnap = Simulation::FSnapshotService::CaptureWorld();
    if (WorldSnap.GridState.Num() == 0) return;

    FRandomStream Rng(WorldSnap.WorldSeed);
    CachedPerceivedWorld = Simulation::FPerceptionService::ComputePerceivedWorld(WorldSnap, Rng);
}