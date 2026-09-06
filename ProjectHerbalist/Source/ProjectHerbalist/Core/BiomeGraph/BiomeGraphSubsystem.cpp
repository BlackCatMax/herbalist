// Core/BiomeGraph/BiomeGraphSubsystem.cpp
#include "BiomeGraphSubsystem.h"
#include "BiomeGraphAsset.h"
#include "Core/World/GridWorldManager.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Materials/MaterialParameterCollection.h"
#include "Kismet/KismetMaterialLibrary.h"
#include "Core/Simulation/Public/SnapshotTypes.h"
#include "Core/Simulation/Public/DeltaTypes.h"
#include "Core/Config/HerbalistSettings.h"
#include "ProjectHerbalist.h"
#include "HerbalistLogChannels.h"

void UBiomeGraphSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
    Super::OnWorldBeginPlay(InWorld);
    CachedGridWorldManager.Reset();
    bInitialized = false;
}

void UBiomeGraphSubsystem::Deinitialize()
{
    // Собственный teardown ПЕРЕД Super:: (2026-09-05, чистка -- раньше было
    // наоборот, что для UGameInstanceSubsystem не по стандартной практике
    // движка: базовый Deinitialize может завершать жизненный цикл
    // подсистемы, сначала должны освободиться свои ресурсы).
    Nodes.Empty();
    Edges.Empty();
    CachedBiomeCenters.Empty();
    bInitialized = false;
    Super::Deinitialize();
}

void UBiomeGraphSubsystem::InitializeFromAsset(UBiomeGraphAsset* Asset)
{
    if (!Asset)
    {
        UE_LOG(LogHerbalistBiome, Error, TEXT("InitializeFromAsset: Asset is null"));
        return;
    }

    Nodes.Empty();
    for (const FBiomeGraphNodeEntry& Entry : Asset->Nodes)
    {
        Nodes.Add(Entry.BiomeID, Entry.Node);
    }

    // Валидация рёбер (аудит 2026-09-05): PropagateWaves читает
    // PrevMorok[Edge.FromBiome]/[Edge.ToBiome] через TMap::operator[]
    // (FindChecked) -- ребро, ссылающееся на узел, которого нет среди
    // Nodes (переименованный/удалённый узел в ассете, опечатка при
    // импорте), рушило бы игру на первом же шаге графа вместо честного
    // отказа при загрузке. Отбрасываем такие рёбра здесь, с явным логом,
    // а не молча падаем пятью шагами глубже.
    Edges.Reset();
    Edges.Reserve(Asset->Edges.Num());
    for (const FBiomeGraphEdge& Edge : Asset->Edges)
    {
        if (!Nodes.Contains(Edge.FromBiome) || !Nodes.Contains(Edge.ToBiome))
        {
            UE_LOG(LogHerbalistBiome, Error, TEXT("InitializeFromAsset: ребро %s -> %s ссылается на неизвестный узел, пропущено"),
                *Edge.FromBiome.ToString(), *Edge.ToBiome.ToString());
            continue;
        }
        Edges.Add(Edge);
    }

    GlobalMorokDecay = Asset->GlobalMorokDecay;
    GlobalZaryanaDecay = Asset->GlobalZaryanaDecay;
    PotionCollapseThreshold = Asset->PotionCollapseThreshold;
    BiomeCollapseThreshold = Asset->BiomeCollapseThreshold;
    FixedTimeStep = Asset->FixedTimeStep;
    GlobalInfluenceScale = Asset->GlobalInfluenceScale;
    GridBlendFactor = Asset->GridBlendFactor;

    bInitialized = true;

    UE_LOG(LogHerbalistBiome, Log, TEXT("Initialized with %d nodes, %d edges"), Nodes.Num(), Edges.Num());
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

void UBiomeGraphSubsystem::RestoreNodeFieldState(const TMap<FName, FBiomeGraphNode>& InNodes)
{
    for (const auto& Pair : InNodes)
    {
        if (FBiomeGraphNode* Node = Nodes.Find(Pair.Key))
        {
            Node->MorokField = Pair.Value.MorokField;
            Node->ZaryanaField = Pair.Value.ZaryanaField;
            Node->Memory = Pair.Value.Memory;
        }
    }
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
    UE_LOG(LogHerbalistBiome, Log, TEXT("ForceStep executed"));
}

void UBiomeGraphSubsystem::InternalStep(float StepDeltaTime)
{
    AGridWorldManager* Grid = FindGridWorldManager();
    if (!Grid)
    {
        UE_LOG(LogHerbalistBiome, Warning, TEXT("InternalStep: GridWorldManager not found"));
        return;
    }

    RecalculateFieldsFromGrid(Grid);
    PropagateWaves(Grid);
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
            // Смешиваем со средним по гриду, а не перезаписываем: иначе вклад
            // соседей, добавленный PropagateWaves на предыдущем шаге, стирается
            // здесь же на следующем шаге, и поле не может по-настоящему
            // распространяться дальше одного узла за раз (см. аудит математики).
            const float GridAvgMorok = FMath::Clamp(MorokSum[BiomeID] / Count, 0.f, 1.f);
            const float GridAvgZaryana = FMath::Clamp(ZaryanaSum[BiomeID] / Count, 0.f, 1.f);
            Pair.Value.MorokField = FMath::Clamp(FMath::Lerp(Pair.Value.MorokField, GridAvgMorok, GridBlendFactor), 0.f, 1.f);
            Pair.Value.ZaryanaField = FMath::Clamp(FMath::Lerp(Pair.Value.ZaryanaField, GridAvgZaryana, GridBlendFactor), 0.f, 1.f);
        }
    }

    UE_LOG(LogHerbalistBiome, VeryVerbose, TEXT("RecalculateFieldsFromGrid completed"));
}

// Эффект 3, Пограничное (Перун, 15_Cycles_And_Shrines.md §15.5 "Типы
// капищ"): "не даёт искажению течь между биомами" -- MorokLeak по ребру
// графа умножается на (1 − 0.5×Restoration), но только для рёбер между
// биомами, которые капище физически граничит в сетке (проверяем соседние
// клетки капища, не любые два узла графа -- капище должно реально стоять
// на стыке). Собирается один раз за шаг, до основного цикла по рёбрам ниже.
static void CollectBorderShrineDamping(AGridWorldManager* Grid, float DampeningFactor,
    TMap<FName, TMap<FName, float>>& OutDamping)
{
    if (!Grid) return;

    static const FIntPoint Offsets[4] = { FIntPoint(1, 0), FIntPoint(-1, 0), FIntPoint(0, 1), FIntPoint(0, -1) };

    for (const FShrine& S : Grid->GetShrines())
    {
        if (S.Type != EShrineType::Border || S.Restoration <= 0.0f) continue;

        const FGridCell* Center = Grid->GetCellConst(S.Cell.X, S.Cell.Y);
        if (!Center) continue;

        for (const FIntPoint& Offset : Offsets)
        {
            const FGridCell* Neighbor = Grid->GetCellConst(S.Cell.X + Offset.X, S.Cell.Y + Offset.Y);
            if (!Neighbor || Neighbor->Biome == Center->Biome) continue;

            const FName A = FBiomeDefaults::BiomeTypeToName(Center->Biome);
            const FName B = FBiomeDefaults::BiomeTypeToName(Neighbor->Biome);
            const float Factor = 1.0f - FMath::Clamp(DampeningFactor * S.Restoration, 0.0f, 1.0f);

            // Рёбра графа заведены в обе стороны (см. ROADMAP.md, импорт
            // DA_BiomeGraph) -- дампим обе, min с уже записанным на случай
            // нескольких капищ на один и тот же стык.
            float& SlotAB = OutDamping.FindOrAdd(A).FindOrAdd(B, 1.0f);
            SlotAB = FMath::Min(SlotAB, Factor);
            float& SlotBA = OutDamping.FindOrAdd(B).FindOrAdd(A, 1.0f);
            SlotBA = FMath::Min(SlotBA, Factor);
        }
    }
}

void UBiomeGraphSubsystem::PropagateWaves(AGridWorldManager* Grid)
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

    const UHerbalistSettings* Settings = GetDefault<UHerbalistSettings>();
    TMap<FName, TMap<FName, float>> BorderDamping;
    CollectBorderShrineDamping(Grid, Settings ? Settings->ShrineBorderLeakDampening : 0.5f, BorderDamping);

    for (const FBiomeGraphEdge& Edge : Edges)
    {
        float SourceMorok = PrevMorok[Edge.FromBiome];
        float SourceZaryana = PrevZaryana[Edge.FromBiome];

        float EffectiveMorokLeak = Edge.MorokLeak;
        if (const TMap<FName, float>* FromDamping = BorderDamping.Find(Edge.FromBiome))
        {
            if (const float* Factor = FromDamping->Find(Edge.ToBiome))
            {
                EffectiveMorokLeak *= *Factor;
            }
        }

        DeltaMorok[Edge.ToBiome] += SourceMorok * EffectiveMorokLeak * GlobalInfluenceScale;
        DeltaZaryana[Edge.ToBiome] += SourceZaryana * Edge.ZaryanaFlow * GlobalInfluenceScale;
    }

    for (auto& Pair : Nodes)
    {
        Pair.Value.MorokField = FMath::Clamp(PrevMorok[Pair.Key] + DeltaMorok[Pair.Key], 0.f, 1.f);
        Pair.Value.ZaryanaField = FMath::Clamp(PrevZaryana[Pair.Key] + DeltaZaryana[Pair.Key], 0.f, 1.f);
    }

    UE_LOG(LogHerbalistBiome, VeryVerbose, TEXT("PropagateWaves completed"));
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

    UE_LOG(LogHerbalistBiome, VeryVerbose, TEXT("ApplyFieldsToGrid completed"));
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
    UE_LOG(LogHerbalistBiome, Log, TEXT("Footprint on %s: Morok=%.3f, Zaryana=%.3f, Axis=(%.2f,%.2f,%.2f,%.2f)"),
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
        UE_LOG(LogHerbalistBiome, Warning, TEXT("Graph not initialized"));
        return;
    }

    UE_LOG(LogHerbalistBiome, Log, TEXT("=== BIOME GRAPH STATE (%d nodes) ==="), Nodes.Num());
    for (const auto& Pair : Nodes)
    {
        const FBiomeGraphNode& Node = Pair.Value;
        UE_LOG(LogHerbalistBiome, Log, TEXT("[%s] MorokField=%.3f, ZaryanaField=%.3f, MorokAff=%.3f, Stab=%.3f, Mem: MorokHist=%.3f, Inst=%.3f"),
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
    UE_LOG(LogHerbalistBiome, Log, TEXT("Graph reset"));
}

// ============================================================================
// ВИЗУАЛИЗАЦИЯ ЧЕРЕЗ MATERIAL PARAMETER COLLECTION
// ============================================================================

void UBiomeGraphSubsystem::UpdateVisualization()
{
    if (!bInitialized || Nodes.Num() == 0)
    {
        UE_LOG(LogHerbalistBiome, Verbose, TEXT("UpdateVisualization: graph not initialized or empty"));
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

    UE_LOG(LogHerbalistBiome, Verbose, TEXT("UpdateVisualization: Morok=%.2f, Zaryana=%.2f"), AvgMorok, AvgZaryana);
}

FBiomeSnapshot UBiomeGraphSubsystem::CaptureState() const
{
    FBiomeSnapshot Snapshot;
    Snapshot.CollapseThreshold = PotionCollapseThreshold;

    for (const auto& Pair : Nodes)
    {
        Snapshot.ActiveBiomeIds.Add(Pair.Key);

        FBiomeFieldContext Ctx;
        Ctx.MorokField      = Pair.Value.MorokField;
        Ctx.ZaryanaField    = Pair.Value.ZaryanaField;
        Ctx.MorokAffinity   = Pair.Value.MorokAffinity;
        Ctx.ZaryanaAffinity = Pair.Value.ZaryanaAffinity;
        Ctx.AxisDrift       = Pair.Value.Memory.AxisDrift;
        Snapshot.Contexts.Add(Pair.Key, Ctx);
    }
    return Snapshot;
}

void UBiomeGraphSubsystem::ApplyStateDelta(const FStateDelta& Delta)
{
    // В будущем здесь будет обновление MorokField, ZaryanaField и т.д.
    // Пока только логируем полученные активации
    UE_LOG(LogHerbalistBiome, Log, TEXT("ApplyStateDelta: received %d biome activations"), Delta.BiomeActivations.Num());
    for (const FName& BiomeID : Delta.BiomeActivations)
    {
        UE_LOG(LogHerbalistBiome, Verbose, TEXT(" - %s"), *BiomeID.ToString());
    }
}