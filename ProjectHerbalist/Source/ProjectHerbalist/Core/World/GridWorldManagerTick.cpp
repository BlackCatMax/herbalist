// Core/World/GridWorldManagerTick.cpp
// Полный файл с трассировкой, реплеем и восстановлением экологии

#include "Core/World/GridWorldManager.h"
#include "ProjectHerbalist.h"
#include "HerbalistLogChannels.h"
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

    // ========================================================================
    // ПАЙПЛАЙН НА ФИКСИРОВАННОМ ШАГЕ
    // Command Intake -> Snapshot -> Pipeline -> Delta -> World Apply выполняются
    // с шагом SimulationFixedTimeStep, а не на каждом рендер-кадре — количество
    // симуляционных тиков в секунду больше не зависит от FPS (см. Tick Execution
    // Model в архитектурном документе). При просадке FPS ниже шага может пройти
    // несколько шагов симуляции за один Tick() — цикл ниже их все отработает.
    // ========================================================================
    SimulationTimeAccumulator += DeltaTime;
    while (SimulationTimeAccumulator >= SimulationFixedTimeStep)
    {
        SimulationTimeAccumulator -= SimulationFixedTimeStep;
        RunSimulationStep();
    }

    // ========================================================================
    // ВОССТАНОВЛЕНИЕ ПАРАМЕТРОВ КЛЕТОК (ЭКОЛОГИЯ)
    // Непрерывный процесс релаксации к TargetState, не часть пайплайна —
    // идёт каждый кадр с реальным DeltaTime, как и раньше.
    // ========================================================================
    RegenerateCellParameters(DeltaTime);

    // ========================================================================
    // ПРОЯВЛЕНИЕ СУЩНОСТЕЙ (16_Entity_Manifestation, вертикальный срез)
    // Тот же внепайплайновый принцип, что и у RegenerateCellParameters выше.
    // Суточная фаза для Морочников берётся из GetWorld()->GetTimeSeconds()
    // напрямую (GetTimeOfDay01), отдельных часов больше нет.
    // ========================================================================
    UpdateEntityManifestations(DeltaTime);

    // Тик всегда активен для вызова нового пайплайна каждый кадр
    SetActorTickEnabled(true);

#if WITH_EDITOR
    if (bEnableDebugDraw)
    {
        DrawGridDebug();
        DrawBiomeGraphDebug();
    }
#endif
}

void AGridWorldManager::RunSimulationStep()
{
    // Снапшот мира и биом-графа для трассировки (если включено)
    FWorldSnapshot PreTickSnapshot;
    FBiomeSnapshot PreBiomeSnapshot;
    if (bEnableTrace)
    {
        PreTickSnapshot = CaptureState();
        if (UBiomeGraphSubsystem* Graph = GetWorld()->GetSubsystem<UBiomeGraphSubsystem>())
        {
            PreBiomeSnapshot = Graph->CaptureState();
        }
    }

    // Построение и выполнение графа команд нового пайплайна
    const FCommandGraph CmdGraph = Simulation::FSnapshotService::BuildCommandGraph(PendingCommands);
    TArray<FCommandEntry> CommandsCopy = PendingCommands;   // копируем для трейса
    PendingCommands.Empty();
    FStateDelta Delta = Simulation::FSnapshotService::ExecuteTick(CmdGraph);

    // Footprint (14_Biome_Graph.md): Pipeline — чистая функция, сам в
    // UBiomeGraphSubsystem не пишет, только формирует Delta.Footprints.
    // Применение — здесь, вне Pipeline, где обращение к UObject-подсистеме допустимо.
    if (Delta.Footprints.Num() > 0)
    {
        if (UBiomeGraphSubsystem* Graph = GetWorld()->GetSubsystem<UBiomeGraphSubsystem>())
        {
            for (const FBiomeFootprintEntry& Footprint : Delta.Footprints)
            {
                Graph->RecordFootprint(Footprint.BiomeID, Footprint.MorokImpact, Footprint.ZaryanaImpact,
                    Footprint.AxisDelta, SimulationFixedTimeStep);
            }
        }
    }

    // Запись кадра трассировки
    if (bEnableTrace)
    {
        TraceBuffer.Record(CurrentTickID, PreTickSnapshot, PreBiomeSnapshot, CommandsCopy, Delta);
    }

    CurrentTickID++;
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
        UE_LOG(LogHerbalistWorld, Warning, TEXT("ReplayLastTick: No trace frames recorded"));
        return;
    }

    FRandomStream Rng(Frame->WorldSnapshot.WorldSeed);
    Simulation::ReplayAndCompare(*Frame, Rng);
}