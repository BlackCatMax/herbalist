// BiomeGraphCommands.cpp
//
// Консольные команды биом-графа регистрируются статическими
// FAutoConsoleCommand-объектами ниже (сама их конструкция при загрузке
// модуля и есть регистрация). Заголовок BiomeGraphCommands.h с пустой
// функцией Register(), которую никто никогда не вызывал, удалён
// 2026-09-02 при чистке мёртвого кода.
#include "BiomeGraphSubsystem.h"
#include "Core/World/GridWorldManager.h"
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

#if WITH_EDITOR
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
#endif // WITH_EDITOR