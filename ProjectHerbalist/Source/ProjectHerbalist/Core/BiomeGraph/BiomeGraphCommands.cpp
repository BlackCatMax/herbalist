// BiomeGraphCommands.cpp
#include "BiomeGraphCommands.h"
#include "BiomeGraphSubsystem.h"
#include "GridWorldManager.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"

namespace
{
    UBiomeGraphSubsystem* GetGraph(UWorld* World)
    {
        if (!World) return nullptr;
        return World->GetSubsystem<UBiomeGraphSubsystem>();
    }
}

static FAutoConsoleCommandWithWorld CmdPrintGraph(
    TEXT("Herbalist.Graph.Print"),
    TEXT("Print biome graph state"),
    FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
    {
        if (auto* Graph = GetGraph(World))
            Graph->DebugPrintNodes();
    })
);

static FAutoConsoleCommandWithWorld CmdStepGraph(
    TEXT("Herbalist.Graph.Step"),
    TEXT("Force one simulation step"),
    FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
    {
        if (auto* Graph = GetGraph(World))
            Graph->ForceStep();
    })
);

static FAutoConsoleCommandWithWorld CmdResetGraph(
    TEXT("Herbalist.Graph.Reset"),
    TEXT("Reset graph state"),
    FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
    {
        if (auto* Graph = GetGraph(World))
            Graph->ResetGraph();
    })
);

static FAutoConsoleCommandWithWorld CmdToggleVis(
    TEXT("Herbalist.Graph.ToggleVis"),
    TEXT("Toggle biome graph visualization"),
    FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
    {
        if (!World) return;
        for (TActorIterator<AGridWorldManager> It(World); It; ++It)
        {
            It->bShowBiomeGraph = !It->bShowBiomeGraph;
            break;
        }
    })
);

static FAutoConsoleCommandWithWorld CmdToggleCellDistortion(
    TEXT("Herbalist.Debug.ToggleCellDistortion"),
    TEXT("Toggle per-cell Distortion overlay"),
    FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
    {
        if (!World) return;
        for (TActorIterator<AGridWorldManager> It(World); It; ++It)
        {
            It->bShowCellDistortion = !It->bShowCellDistortion;
            break;
        }
    })
);

static FAutoConsoleCommandWithWorld CmdToggleCellInfluence(
    TEXT("Herbalist.Debug.ToggleCellInfluence"),
    TEXT("Toggle per-cell Graph Influence (Δ) overlay"),
    FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
    {
        if (!World) return;
        for (TActorIterator<AGridWorldManager> It(World); It; ++It)
        {
            It->bShowCellInfluence = !It->bShowCellInfluence;
            break;
        }
    })
);

void BiomeGraphCommands::Register()
{
    // Команды регистрируются статически при загрузке модуля.
}