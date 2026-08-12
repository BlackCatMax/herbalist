// SnapshotService.cpp
#include "SnapshotService.h"
#include "Core/Simulation/Private/PipelineV2.h"
#include "Core/Simulation/Public/SnapshotTypes.h"
#include "Core/World/GridWorldManager.h"
#include "Core/BiomeGraph/BiomeGraphSubsystem.h"
#include "Player/HerbalistPlayerController.h"
#include "Engine/World.h"
#include "Engine/Engine.h"         // для GEngine
#include "EngineUtils.h"          // для TActorIterator

namespace Simulation
{
    // ----- Вспомогательная функция для поиска мира -----
    static UWorld* GetSimulationWorld()
    {
        // Простейший способ – через первый PlayerController
        if (GEngine)
        {
            for (const FWorldContext& Context : GEngine->GetWorldContexts())
            {
                if (Context.WorldType == EWorldType::Game || Context.WorldType == EWorldType::PIE)
                {
                    return Context.World();
                }
            }
        }
        return nullptr;
    }

    // ----- Захват состояния -----
    FWorldSnapshot FSnapshotService::CaptureWorld()
    {
        FWorldSnapshot Snapshot;
        UWorld* World = GetSimulationWorld();
        if (!World) return Snapshot;

        for (TActorIterator<AGridWorldManager> It(World); It; ++It)
        {
            AGridWorldManager* Grid = *It;
            if (Grid)
            {
                Snapshot = Grid->CaptureState();
                break;   // берём первый попавшийся
            }
        }
        return Snapshot;
    }

    FInventorySnapshot FSnapshotService::CaptureInventory()
    {
        FInventorySnapshot Snapshot;
        UWorld* World = GetSimulationWorld();
        if (!World) return Snapshot;

        AHerbalistPlayerController* PC = Cast<AHerbalistPlayerController>(World->GetFirstPlayerController());
        if (PC && PC->InventoryComponent)
        {
            Snapshot = PC->InventoryComponent->CaptureState();
        }
        // Здесь могут быть и другие контейнеры (сундуки) – добавим позже
        return Snapshot;
    }

    FBiomeSnapshot FSnapshotService::CaptureBiomes()
    {
        FBiomeSnapshot Snapshot;
        UWorld* World = GetSimulationWorld();
        if (!World) return Snapshot;

        UBiomeGraphSubsystem* BiomeSubsystem = World->GetSubsystem<UBiomeGraphSubsystem>();
        if (BiomeSubsystem)
        {
            Snapshot = BiomeSubsystem->CaptureState();
        }
        return Snapshot;
    }

    // ----- Применение дельты -----
    void FSnapshotService::ApplyDeltaToWorld(const FStateDelta& Delta)
    {
        UWorld* World = GetSimulationWorld();
        if (!World) return;

        for (TActorIterator<AGridWorldManager> It(World); It; ++It)
        {
            AGridWorldManager* Grid = *It;
            if (Grid)
            {
                Grid->ApplyStateDelta(Delta);
                break;
            }
        }
    }

    void FSnapshotService::ApplyDeltaToInventory(const FStateDelta& Delta)
    {
        UWorld* World = GetSimulationWorld();
        if (!World) return;

        AHerbalistPlayerController* PC = Cast<AHerbalistPlayerController>(World->GetFirstPlayerController());
        if (PC && PC->InventoryComponent)
        {
            PC->InventoryComponent->ApplyStateDelta(Delta);
        }
    }

    void FSnapshotService::ApplyDeltaToBiomes(const FStateDelta& Delta)
    {
        UWorld* World = GetSimulationWorld();
        if (!World) return;

        UBiomeGraphSubsystem* BiomeSubsystem = World->GetSubsystem<UBiomeGraphSubsystem>();
        if (BiomeSubsystem)
        {
            BiomeSubsystem->ApplyStateDelta(Delta);
        }
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
        FBiomeSnapshot BiomeSnap = CaptureBiomes();

        // 2. Инициализация ГПСЧ из сида тика (WorldSeed = HashCombine(RngBaseSeed, TickIndex),
        //    см. AGridWorldManager::CaptureState) — уникален для каждого тика, воспроизводим при реплее
        FRandomStream Rng(WorldSnap.WorldSeed);

        // 3. Запуск детерминированного пайплайна
        FStateDelta Delta = ExecutePipeline(WorldSnap, InvSnap, BiomeSnap, Commands, Rng);

        // 4. Применение полученной дельты
        ApplyDeltaToWorld(Delta);
        ApplyDeltaToInventory(Delta);
        ApplyDeltaToBiomes(Delta);

        return Delta;
    }
}