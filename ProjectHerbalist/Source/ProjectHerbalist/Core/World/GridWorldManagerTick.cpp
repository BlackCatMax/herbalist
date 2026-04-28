// GridWorldManagerTick.cpp
#include "Core/World/GridWorldManager.h"
#include "ProjectHerbalist.h"
#include "DrawDebugHelpers.h"
#include "Core/Simulation/Public/CommandTypes.h"
#include "Core/Simulation/Public/SnapshotService.h"
#include "Core/BiomeGraph/BiomeGraphSubsystem.h"

// ============================================================================
// ТИК МИРА
// ============================================================================

void AGridWorldManager::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    // Шаг симуляции биомного графа
    if (UBiomeGraphSubsystem* Graph = GetWorld()->GetSubsystem<UBiomeGraphSubsystem>())
    {
        Graph->StepSimulation(DeltaTime);
    }

    // Построение и выполнение графа команд нового пайплайна
    const FCommandGraph CmdGraph = Simulation::FSnapshotService::BuildCommandGraph(PendingCommands);
    PendingCommands.Empty();
    Simulation::FSnapshotService::ExecuteTick(CmdGraph);

    // Тик всегда активен для вызова нового пайплайна каждый кадр
    SetActorTickEnabled(true);

#if WITH_EDITOR
    // Отладочная отрисовка
    if (bEnableDebugDraw)
    {
        DrawGridDebug();
        DrawBiomeGraphDebug();
    }
#endif
}