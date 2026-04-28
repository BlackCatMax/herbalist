// Core/World/GridWorldManagerTick.cpp
// Полный файл с трассировкой и реплеем

#include "Core/World/GridWorldManager.h"
#include "ProjectHerbalist.h"
#include "DrawDebugHelpers.h"
#include "Core/Simulation/Public/CommandTypes.h"
#include "Core/Simulation/Public/SnapshotService.h"
#include "Core/Simulation/Public/TraceTypes.h"
#include "Core/Simulation/Private/TraceReplay.h"
#include "Core/BiomeGraph/BiomeGraphSubsystem.h"

void AGridWorldManager::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (UBiomeGraphSubsystem* Graph = GetWorld()->GetSubsystem<UBiomeGraphSubsystem>())
    {
        Graph->StepSimulation(DeltaTime);
    }

    // Снапшот мира для трассировки (если включено)
    FWorldSnapshot PreTickSnapshot;
    if (bEnableTrace)
    {
        PreTickSnapshot = CaptureState();
    }

    // Построение и выполнение графа команд нового пайплайна
    const FCommandGraph CmdGraph = Simulation::FSnapshotService::BuildCommandGraph(PendingCommands);
    TArray<FCommandEntry> CommandsCopy = PendingCommands;   // копируем для трейса
    PendingCommands.Empty();
    FStateDelta Delta = Simulation::FSnapshotService::ExecuteTick(CmdGraph);

    // Запись кадра трассировки
    if (bEnableTrace)
    {
        TraceBuffer.Record(CurrentTickID, PreTickSnapshot, CommandsCopy, Delta);
    }

    // Тик всегда активен для вызова нового пайплайна каждый кадр
    SetActorTickEnabled(true);
    CurrentTickID++;

#if WITH_EDITOR
    if (bEnableDebugDraw)
    {
        DrawGridDebug();
        DrawBiomeGraphDebug();
    }
#endif
}

void AGridWorldManager::DumpTrace()
{
    TraceBuffer.DumpToLog();
}

void AGridWorldManager::ReplayLastTick()
{
    const FTraceFrame* Frame = TraceBuffer.GetLastFrame();
    if (!Frame)
    {
        UE_LOG(LogHerbalist, Warning, TEXT("ReplayLastTick: No trace frames recorded"));
        return;
    }

    FRandomStream Rng(Frame->WorldSnapshot.WorldSeed);
    Simulation::ReplayAndCompare(*Frame, Rng);
}