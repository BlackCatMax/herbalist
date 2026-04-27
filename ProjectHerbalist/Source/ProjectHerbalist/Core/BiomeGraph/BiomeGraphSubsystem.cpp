// Core/BiomeGraph/BiomeGraphSubsystem.cpp
#include "BiomeGraphSubsystem.h"
#include "BiomeGraphAsset.h"
#include "Core/World/GridWorldManager.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Materials/MaterialParameterCollection.h"
#include "Kismet/KismetMaterialLibrary.h"
#include "ProjectHerbalist.h"

DEFINE_LOG_CATEGORY_STATIC(LogBiomeGraph, Log, All);

void UBiomeGraphSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
    Super::OnWorldBeginPlay(InWorld);
    CachedGridWorldManager.Reset();
    bInitialized = false;
}

void UBiomeGraphSubsystem::Deinitialize()
{
    Super::Deinitialize();
    Nodes.Empty();
    Edges.Empty();
    AdjacencyList.Empty();
    CachedBiomeCenters.Empty();
    bInitialized = false;
}

void UBiomeGraphSubsystem::InitializeFromAsset(UBiomeGraphAsset* Asset)
{
    if (!Asset)
    {
        UE_LOG(LogBiomeGraph, Error, TEXT("InitializeFromAsset: Asset is null"));
        return;
    }

    Nodes.Empty();
    for (const FBiomeGraphNodeEntry& Entry : Asset->Nodes)
    {
        Nodes.Add(Entry.BiomeID, Entry.Node);
    }
    Edges = Asset->Edges;
    GlobalMorokDecay = Asset->GlobalMorokDecay;
    GlobalZaryanaDecay = Asset->GlobalZaryanaDecay;
    CollapseThreshold = Asset->CollapseThreshold;
    FixedTimeStep = Asset->FixedTimeStep;
    GlobalInfluenceScale = Asset->GlobalInfluenceScale;

    BuildAdjacencyList();
    bInitialized = true;

    UE_LOG(LogBiomeGraph, Log, TEXT("Initialized with %d nodes, %d edges"), Nodes.Num(), Edges.Num());
}

void UBiomeGraphSubsystem::BuildAdjacencyList()
{
    AdjacencyList.Empty();
    for (int32 i = 0; i < Edges.Num(); ++i)
    {
        AdjacencyList.FindOrAdd(Edges[i].FromBiome).Add(i);
    }
}

bool UBiomeGraphSubsystem::IsGridValid() const
{
    return FindGridWorldManager() != nullptr;
}

AGridWorldManager* UBiomeGraphSubsystem::FindGridWorldManager() const
{
    if (CachedGridWorldManager.IsValid())
        return CachedGridWorldManager.Get();

    UWorld* World = GetWorld();
    if (!World) return nullptr;

    for (TActorIterator<AGridWorldManager> It(World); It; ++It)
    {
        CachedGridWorldManager = *It;
        return *It;
    }
    return nullptr;
}

const FBiomeGraphNode* UBiomeGraphSubsystem::GetNode(FName BiomeID) const
{
    return Nodes.Find(BiomeID);
}

FBiomeGraphNode* UBiomeGraphSubsystem::GetMutableNode(FName BiomeID)
{
    return Nodes.Find(BiomeID);
}

void UBiomeGraphSubsystem::StepSimulation(float DeltaTime)
{
    if (!bInitialized || Nodes.Num() == 0) return;

    TimeAccumulator += DeltaTime;
    while (TimeAccumulator >= FixedTimeStep)
    {
        InternalStep(FixedTimeStep);
        TimeAccumulator -= FixedTimeStep;
    }
}

void UBiomeGraphSubsystem::ForceStep()
{
    if (!bInitialized || Nodes.Num() == 0) return;
    InternalStep(FixedTimeStep);
    UE_LOG(LogBiomeGraph, Log, TEXT("ForceStep executed"));
}

void UBiomeGraphSubsystem::InternalStep(float StepDeltaTime)
{
    AGridWorldManager* Grid = FindGridWorldManager();
    if (!Grid)
    {
        UE_LOG(LogBiomeGraph, Warning, TEXT("InternalStep: GridWorldManager not found"));
        return;
    }

    RecalculateFieldsFromGrid(Grid);
    PropagateWaves();
    ApplyFieldsToGrid(Grid);
    UpdateMemories(StepDeltaTime);

    CenterCacheTimer += StepDeltaTime;
    if (CenterCacheTimer >= CenterCacheUpdateInterval)
    {
        CenterCacheTimer = 0.f;
        UpdateBiomeCenters(Grid);
    }

    // Обновляем визуализацию после каждого шага
    UpdateVisualization();

    OnStepExecuted.Broadcast(StepDeltaTime);
}

void UBiomeGraphSubsystem::RecalculateFieldsFromGrid(AGridWorldManager* Grid)
{
    if (!Grid) return;

    TMap<FName, float> MorokSum, ZaryanaSum;
    TMap<FName, int32> CountMap;

    for (const auto& Pair : Nodes)
    {
        MorokSum.Add(Pair.Key, 0.f);
        ZaryanaSum.Add(Pair.Key, 0.f);
        CountMap.Add(Pair.Key, 0);
    }

    TArray<FGridBiomeSample> Samples = Grid->GetBiomeSamples();
    for (const FGridBiomeSample& Sample : Samples)
    {
        if (!Nodes.Contains(Sample.BiomeID)) continue;
        MorokSum[Sample.BiomeID] += Sample.MorokValue;
        ZaryanaSum[Sample.BiomeID] += Sample.ZaryanaValue;
        CountMap[Sample.BiomeID]++;
    }

    for (auto& Pair : Nodes)
    {
        FName BiomeID = Pair.Key;
        int32 Count = CountMap[BiomeID];
        if (Count > 0)
        {
            Pair.Value.MorokField = FMath::Clamp(MorokSum[BiomeID] / Count, 0.f, 1.f);
            Pair.Value.ZaryanaField = FMath::Clamp(ZaryanaSum[BiomeID] / Count, 0.f, 1.f);
        }
    }

    UE_LOG(LogBiomeGraph, VeryVerbose, TEXT("RecalculateFieldsFromGrid completed"));
}

void UBiomeGraphSubsystem::PropagateWaves()
{
    TMap<FName, float> PrevMorok, PrevZaryana;
    for (const auto& Pair : Nodes)
    {
        PrevMorok.Add(Pair.Key, FMath::Clamp(Pair.Value.MorokField, 0.f, 1.f));
        PrevZaryana.Add(Pair.Key, FMath::Clamp(Pair.Value.ZaryanaField, 0.f, 1.f));
    }

    TMap<FName, float> DeltaMorok, DeltaZaryana;
    for (const auto& Pair : Nodes)
    {
        DeltaMorok.Add(Pair.Key, 0.f);
        DeltaZaryana.Add(Pair.Key, 0.f);
    }

    for (const FBiomeGraphEdge& Edge : Edges)
    {
        float SourceMorok = PrevMorok[Edge.FromBiome];
        float SourceZaryana = PrevZaryana[Edge.FromBiome];

        DeltaMorok[Edge.ToBiome] += SourceMorok * Edge.MorokLeak * GlobalInfluenceScale;
        DeltaZaryana[Edge.ToBiome] += SourceZaryana * Edge.ZaryanaFlow * GlobalInfluenceScale;
    }

    for (auto& Pair : Nodes)
    {
        Pair.Value.MorokField = FMath::Clamp(PrevMorok[Pair.Key] + DeltaMorok[Pair.Key], 0.f, 1.f);
        Pair.Value.ZaryanaField = FMath::Clamp(PrevZaryana[Pair.Key] + DeltaZaryana[Pair.Key], 0.f, 1.f);
    }

    UE_LOG(LogBiomeGraph, VeryVerbose, TEXT("PropagateWaves completed"));
}

void UBiomeGraphSubsystem::ApplyFieldsToGrid(AGridWorldManager* Grid)
{
    if (!Grid) return;

    TMap<FName, float> MorokFields, ZaryanaFields;
    for (const auto& Pair : Nodes)
    {
        MorokFields.Add(Pair.Key, Pair.Value.MorokField);
        ZaryanaFields.Add(Pair.Key, Pair.Value.ZaryanaField);
    }

    Grid->ApplyBiomeInfluences(MorokFields, ZaryanaFields, GlobalInfluenceScale);

    UE_LOG(LogBiomeGraph, VeryVerbose, TEXT("ApplyFieldsToGrid completed"));
}

void UBiomeGraphSubsystem::UpdateMemories(float StepDeltaTime)
{
    for (auto& Pair : Nodes)
    {
        FBiomeGraphNode& Node = Pair.Value;
        Node.Memory.MorokHistory = FMath::Clamp(Node.Memory.MorokHistory * (1.f - GlobalMorokDecay * StepDeltaTime), 0.f, 1.f);
        Node.Memory.ZaryanaHistory = FMath::Clamp(Node.Memory.ZaryanaHistory * (1.f - GlobalZaryanaDecay * StepDeltaTime), 0.f, 1.f);
        Node.Memory.Instability = FMath::Clamp(Node.Memory.Instability * 0.995f, 0.f, 1.f);
        Node.Memory.AxisDrift *= 0.98f;
        Node.Memory.AxisDrift.X = FMath::Clamp(Node.Memory.AxisDrift.X, 0.f, 1.f);
        Node.Memory.AxisDrift.Y = FMath::Clamp(Node.Memory.AxisDrift.Y, 0.f, 1.f);
        Node.Memory.AxisDrift.Z = FMath::Clamp(Node.Memory.AxisDrift.Z, 0.f, 1.f);
        Node.Memory.AxisDrift.W = FMath::Clamp(Node.Memory.AxisDrift.W, 0.f, 1.f);
    }
}

void UBiomeGraphSubsystem::RecordFootprint(FName BiomeID, float MorokImpact, float ZaryanaImpact, const FVector4& AxisDelta, float DeltaTime)
{
    FBiomeGraphNode* Node = GetMutableNode(BiomeID);
    if (!Node) return;

    Node->Memory.MorokHistory = FMath::Clamp(Node->Memory.MorokHistory + MorokImpact * DeltaTime * 0.5f, 0.f, 1.f);
    Node->Memory.ZaryanaHistory = FMath::Clamp(Node->Memory.ZaryanaHistory + ZaryanaImpact * DeltaTime * 0.3f, 0.f, 1.f);
    Node->Memory.Instability = FMath::Clamp(Node->Memory.Instability + MorokImpact * DeltaTime * 0.1f, 0.f, 1.f);
    Node->Memory.AxisDrift += AxisDelta * DeltaTime * 0.05f;

    Node->Memory.AxisDrift.X = FMath::Clamp(Node->Memory.AxisDrift.X, 0.f, 1.f);
    Node->Memory.AxisDrift.Y = FMath::Clamp(Node->Memory.AxisDrift.Y, 0.f, 1.f);
    Node->Memory.AxisDrift.Z = FMath::Clamp(Node->Memory.AxisDrift.Z, 0.f, 1.f);
    Node->Memory.AxisDrift.W = FMath::Clamp(Node->Memory.AxisDrift.W, 0.f, 1.f);

    // Уровень Log – теперь Footprint виден всегда
    UE_LOG(LogBiomeGraph, Log, TEXT("Footprint on %s: Morok=%.3f, Zaryana=%.3f, Axis=(%.2f,%.2f,%.2f,%.2f)"),
        *BiomeID.ToString(), MorokImpact, ZaryanaImpact, AxisDelta.X, AxisDelta.Y, AxisDelta.Z, AxisDelta.W);
}

void UBiomeGraphSubsystem::UpdateBiomeCenters(AGridWorldManager* Grid)
{
    if (!Grid) return;
    CachedBiomeCenters = Grid->GetBiomeCenters();
}

void UBiomeGraphSubsystem::DebugPrintNodes() const
{
    if (!bInitialized)
    {
        UE_LOG(LogBiomeGraph, Warning, TEXT("Graph not initialized"));
        return;
    }

    UE_LOG(LogBiomeGraph, Log, TEXT("=== BIOME GRAPH STATE (%d nodes) ==="), Nodes.Num());
    for (const auto& Pair : Nodes)
    {
        const FBiomeGraphNode& Node = Pair.Value;
        UE_LOG(LogBiomeGraph, Log, TEXT("[%s] MorokField=%.3f, ZaryanaField=%.3f, MorokAff=%.3f, Stab=%.3f, Mem: MorokHist=%.3f, Inst=%.3f"),
            *Pair.Key.ToString(),
            Node.MorokField, Node.ZaryanaField, Node.MorokAffinity, Node.Stability,
            Node.Memory.MorokHistory, Node.Memory.Instability);
    }
}

void UBiomeGraphSubsystem::ResetGraph()
{
    for (auto& Pair : Nodes)
    {
        Pair.Value.MorokField = 0.f;
        Pair.Value.ZaryanaField = 0.f;
        Pair.Value.Memory = FBiomeMemory();
    }
    UE_LOG(LogBiomeGraph, Log, TEXT("Graph reset"));
}

// ============================================================================
// ВИЗУАЛИЗАЦИЯ ЧЕРЕЗ MATERIAL PARAMETER COLLECTION
// ============================================================================

void UBiomeGraphSubsystem::UpdateVisualization()
{
    if (!bInitialized || Nodes.Num() == 0)
    {
        UE_LOG(LogBiomeGraph, Verbose, TEXT("UpdateVisualization: graph not initialized or empty"));
        return;
    }

    UWorld* World = GetWorld();
    if (!World)
        return;

    UMaterialParameterCollection* MPC = VisualizationMPC.LoadSynchronous();
    if (!MPC)
        return;

    float TotalMorok = 0.0f;
    float TotalZaryana = 0.0f;
    for (const auto& Pair : Nodes)
    {
        TotalMorok += Pair.Value.MorokField;
        TotalZaryana += Pair.Value.ZaryanaField;
    }
    float AvgMorok = TotalMorok / Nodes.Num();
    float AvgZaryana = TotalZaryana / Nodes.Num();

    UKismetMaterialLibrary::SetScalarParameterValue(World, MPC, "GlobalMorok", AvgMorok);
    UKismetMaterialLibrary::SetScalarParameterValue(World, MPC, "GlobalZaryana", AvgZaryana);

    FLinearColor MorokColor = FLinearColor(AvgMorok, 0.0f, 0.0f, 1.0f);
    FLinearColor ZaryanaColor = FLinearColor(0.0f, AvgZaryana, 0.0f, 1.0f);
    UKismetMaterialLibrary::SetVectorParameterValue(World, MPC, "MorokColor", MorokColor);
    UKismetMaterialLibrary::SetVectorParameterValue(World, MPC, "ZaryanaColor", ZaryanaColor);

    UE_LOG(LogBiomeGraph, Verbose, TEXT("UpdateVisualization: Morok=%.2f, Zaryana=%.2f"), AvgMorok, AvgZaryana);
}