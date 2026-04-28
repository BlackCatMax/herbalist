// SnapshotService.cpp
#include "SnapshotService.h"
#include "Core/Simulation/Private/PipelineV2.h"
#include "Core/Simulation/Public/SnapshotTypes.h"
#include "Core/Simulation/Public/DeltaTypes.h"

namespace Simulation
{
    // ----- Захват состояния (пока заглушки) -----
    FWorldSnapshot FSnapshotService::CaptureWorld()
    {
        return FWorldSnapshot{};
    }

    FInventorySnapshot FSnapshotService::CaptureInventory()
    {
        return FInventorySnapshot{};
    }

    FBiomeSnapshot FSnapshotService::CaptureBiomes()
    {
        return FBiomeSnapshot{};
    }

    // ----- Применение дельты (пока заглушки) -----
    void FSnapshotService::ApplyDeltaToWorld(const FStateDelta& Delta)
    {
        // Будет реализовано после интеграции с GridWorldManager
    }

    void FSnapshotService::ApplyDeltaToInventory(const FStateDelta& Delta)
    {
        // Будет реализовано после интеграции с инвентарём
    }

    void FSnapshotService::ApplyDeltaToBiomes(const FStateDelta& Delta)
    {
        // Будет реализовано после интеграции с BiomeGraph
    }

    // ----- Сборка графа команд (заглушка) -----
    FCommandGraph FSnapshotService::BuildCommandGraph(const TArray<FCommandEntry>& RawCommands)
    {
        FCommandGraph Graph;
        Graph.Commands = RawCommands;
        return Graph;
    }

    // ----- Выполнение одного тика симуляции -----
    FStateDelta FSnapshotService::ExecuteTick(const FCommandGraph& Commands)
    {
        // 1. Захват состояния
        FWorldSnapshot WorldSnap = CaptureWorld();
        FInventorySnapshot InvSnap = CaptureInventory();
        FBiomeSnapshot BiomeSnap = CaptureBiomes();   // пока не используется

        // 2. Инициализация ГПСЧ из сида мира (пока WorldSeed = 0)
        FRandomStream Rng(WorldSnap.WorldSeed);

        // 3. Запуск детерминированного пайплайна
        FStateDelta Delta = ExecutePipeline(WorldSnap, InvSnap, Commands, Rng);

        // 4. Применение полученной дельты
        ApplyDeltaToWorld(Delta);
        ApplyDeltaToInventory(Delta);
        ApplyDeltaToBiomes(Delta);

        return Delta;
    }
}