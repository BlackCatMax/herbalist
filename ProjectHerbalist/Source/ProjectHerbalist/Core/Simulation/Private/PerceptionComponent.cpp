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

    // Захватываем реальный снапшот мира и инвентаря — один Rng на оба, чтобы
    // искажение было согласовано в рамках одного тика восприятия (0.5с).
    FWorldSnapshot WorldSnap = Simulation::FSnapshotService::CaptureWorld();
    if (WorldSnap.GridState.Num() == 0) return;

    // GlobalPerceptionClarity (обсуждение в сессии 2026-08-24, "Прогрессия
    // через Заряну") — компонент живёт прямо на AGridWorldManager (см.
    // AGridWorldManager::PerceptionComponent), не нужен отдельный поиск.
    float Clarity = 0.0f;
    if (const AGridWorldManager* Owner = Cast<AGridWorldManager>(GetOwner()))
    {
        Clarity = Owner->GetGlobalPerceptionClarity();
    }

    FRandomStream Rng(WorldSnap.WorldSeed);
    CachedPerceivedWorld = Simulation::FPerceptionService::ComputePerceivedWorld(WorldSnap, Rng, Clarity);

    FInventorySnapshot InvSnap = Simulation::FSnapshotService::CaptureInventory();
    CachedPerceivedInventory = Simulation::FPerceptionService::ComputePerceivedInventory(InvSnap, Rng, Clarity);
}