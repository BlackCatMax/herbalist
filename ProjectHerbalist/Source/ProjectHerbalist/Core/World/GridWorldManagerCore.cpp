// GridWorldManagerCore.cpp
#include "GridWorldManager.h"
#include "Core/BiomeGraph/BiomeGraphSubsystem.h"
#include "Core/Harvest/HarvestService.h"
#include "Core/Pipeline/AlchemyWorldStateApplier.h"
#include "Core/Types/BiomeTypes.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "DrawDebugHelpers.h"
#include "ProjectHerbalist.h"

FVector AGridWorldManager::GetCellWorldPosition(int32 X, int32 Y) const
{
    return GetActorLocation() + FVector(X * CellSize, Y * CellSize, CellHeight / 2.0f);
}

TArray<FGridBiomeSample> AGridWorldManager::GetBiomeSamples() const
{
    TArray<FGridBiomeSample> Samples;
    for (const FGridCell& Cell : Cells)
    {
        FGridBiomeSample Sample;
        Sample.BiomeID = FBiomeDefaults::BiomeTypeToName(Cell.Biome);
        Sample.MorokValue = Cell.State.Meta.Distortion;
        Sample.ZaryanaValue = 1.f - Cell.State.Meta.Distortion; // упрощённо
        Samples.Add(Sample);
    }
    return Samples;
}

TMap<FName, FVector> AGridWorldManager::GetBiomeCenters() const
{
    TMap<FName, FVector> Centers;
    TMap<FName, int32> Counts;

    for (const FGridCell& Cell : Cells)
    {
        FName BiomeID = FBiomeDefaults::BiomeTypeToName(Cell.Biome);
        FVector Pos = GetCellWorldPosition(Cell.X, Cell.Y);
        Centers.FindOrAdd(BiomeID) += Pos;
        Counts.FindOrAdd(BiomeID)++;
    }

    for (auto& Pair : Centers)
    {
        int32 Count = Counts[Pair.Key];
        if (Count > 0) Pair.Value /= Count;
    }
    return Centers;
}

void AGridWorldManager::ApplyBiomeInfluences(const TMap<FName, float>& MorokFields, const TMap<FName, float>& ZaryanaFields, float GlobalScale)
{
    for (FGridCell& Cell : Cells)
    {
        FName BiomeID = FBiomeDefaults::BiomeTypeToName(Cell.Biome);
        const float* MorokField = MorokFields.Find(BiomeID);
        const float* ZaryanaField = ZaryanaFields.Find(BiomeID);
        if (!MorokField || !ZaryanaField) continue;

        float TargetDistortion = *MorokField;
        Cell.TargetState.Meta.Distortion = FMath::Lerp(Cell.TargetState.Meta.Distortion, TargetDistortion, 0.2f * GlobalScale);
        Cell.TargetState.Meta.Distortion = FMath::Clamp(Cell.TargetState.Meta.Distortion, 0.f, 1.f);

        float ZaryanaInfluence = *ZaryanaField * 0.1f * GlobalScale;
        Cell.TargetState.Meta.Stability = FMath::Clamp(Cell.TargetState.Meta.Stability + ZaryanaInfluence, 0.f, 1.f);
        Cell.TargetState.Meta.Purity = FMath::Clamp(Cell.TargetState.Meta.Purity + ZaryanaInfluence * 0.5f, 0.f, 1.f);
    }
}

#if WITH_EDITOR
void AGridWorldManager::DrawBiomeGraphDebug()
{
    if (!bShowBiomeGraph && !bShowCellDistortion && !bShowCellInfluence) return;

    UWorld* World = GetWorld();
    if (!World) return;

    UBiomeGraphSubsystem* Graph = World->GetSubsystem<UBiomeGraphSubsystem>();
    if (!Graph) return;

    if (bShowBiomeGraph)
    {
        const TMap<FName, FVector>& Centers = Graph->GetCachedBiomeCenters();
        const TArray<FBiomeGraphEdge>& Edges = Graph->GetEdges();
        const TMap<FName, FBiomeGraphNode>& Nodes = Graph->GetNodes();

        for (const FBiomeGraphEdge& Edge : Edges)
        {
            const FVector* FromPos = Centers.Find(Edge.FromBiome);
            const FVector* ToPos = Centers.Find(Edge.ToBiome);
            if (FromPos && ToPos)
            {
                float Thickness = FMath::Lerp(1.f, 5.f, Edge.MorokLeak);
                FColor Color = Edge.MorokLeak > 0.1f ? FColor::Red : (Edge.ZaryanaFlow > 0.1f ? FColor::Blue : FColor::White);
                DrawDebugLine(World, *FromPos, *ToPos, Color, false, World->DeltaTimeSeconds * 1.1f, 0, Thickness);
            }
        }

        for (const auto& Pair : Nodes)
        {
            const FVector* Pos = Centers.Find(Pair.Key);
            if (Pos)
            {
                const FBiomeGraphNode& Node = Pair.Value;
                FColor Color = FLinearColor::LerpUsingHSV(FLinearColor::Green, FLinearColor::Red, Node.MorokField).ToFColor(false);
                float Size = FMath::Lerp(30.f, 80.f, Node.Memory.Instability);
                DrawDebugSphere(World, *Pos, Size, 12, Color, false, World->DeltaTimeSeconds * 1.1f, 0, 2.f);
                DrawDebugString(World, *Pos + FVector(0, 0, Size + 20), Pair.Key.ToString(), nullptr, FColor::White, World->DeltaTimeSeconds * 1.1f, true, 1.2f);
            }
        }
    }

    if (bShowCellDistortion || bShowCellInfluence)
    {
        float Lifetime = World->DeltaTimeSeconds * 1.1f;
        for (const FGridCell& Cell : Cells)
        {
            FVector Pos = GetCellWorldPosition(Cell.X, Cell.Y);
            if (bShowCellDistortion)
            {
                FColor Color = FLinearColor::LerpUsingHSV(FLinearColor::Green, FLinearColor::Red, Cell.State.Meta.Distortion).ToFColor(false);
                DrawDebugString(World, Pos + FVector(0, 0, 30), FString::Printf(TEXT("%.2f"), Cell.State.Meta.Distortion), nullptr, Color, Lifetime, true);
            }
            if (bShowCellInfluence)
            {
                float Influence = Cell.TargetState.Meta.Distortion - Cell.State.Meta.Distortion;
                FColor Color = Influence > 0 ? FColor::Red : (Influence < 0 ? FColor::Blue : FColor::White);
                DrawDebugString(World, Pos + FVector(0, 0, 50), FString::Printf(TEXT("Δ%.2f"), Influence), nullptr, Color, Lifetime, true);
            }
        }
    }
}
#endif

AGridWorldManager::AGridWorldManager()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = true;
}

void AGridWorldManager::BeginPlay()
{
    Super::BeginPlay();
    WorldRNG.Initialize(12345);
    
    // Создаём сервис сбора
    HarvestService = NewObject<UHarvestService>(this);
    
    InitializeCells();
    if (bEnableDebugDraw)
    {
        GetWorldTimerManager().SetTimer(DebugDrawTimer, this, &AGridWorldManager::RedrawDebugBoxes, 0.2f, true);
    }
}

void AGridWorldManager::InitializeCells()
{
    Cells.SetNum(GridSizeX * GridSizeY);
    const int32 BlockSize = 5;
    const int32 BlocksX = GridSizeX / BlockSize;
    const int32 BlocksY = GridSizeY / BlockSize;

    TArray<EBiomeType> AllBiomes = FBiomeDefaults::GetAllBiomeTypes();
    if (AllBiomes.Num() == 0)
    {
        AllBiomes = {
            EBiomeType::Tundra,
            EBiomeType::Taiga,
            EBiomeType::MixedForest,
            EBiomeType::BroadleafForest,
            EBiomeType::ForestSteppe,
            EBiomeType::Steppe,
            EBiomeType::Floodplain,
            EBiomeType::Bog
        };
    }

    for (int32 Y = 0; Y < GridSizeY; Y++)
    {
        for (int32 X = 0; X < GridSizeX; X++)
        {
            int32 Index = Y * GridSizeX + X;
            int32 BlockX = X / BlockSize;
            int32 BlockY = Y / BlockSize;
            int32 BlockIndex = (BlockY * BlocksX + BlockX) % AllBiomes.Num();
            EBiomeType biome = AllBiomes[BlockIndex];

            FRealState defaultState = FBiomeDefaults::GetDefaultState(biome);
            FEnvironment defaultEnv = FBiomeDefaults::GetDefaultEnvironment(biome);
            Cells[Index].Biome = biome;
            Cells[Index].State = defaultState;
            Cells[Index].TargetState = defaultState;
            Cells[Index].Environment = defaultEnv;
            Cells[Index].Memory = FMemoryState();
            Cells[Index].X = X;
            Cells[Index].Y = Y;
            Cells[Index].HarvestStress = 0.0f;
            Cells[Index].bEntityTriggered = false;
            Cells[Index].AvailableIngredientID = FBiomeDefaults::GetRandomResourceForBiome(biome, WorldRNG);
            Cells[Index].ResourceRegrowthTimer = 0.0f;
            Cells[Index].bIsWater = false;
        }
    }

    for (int32 BlockY = 0; BlockY < BlocksY; BlockY++)
    {
        for (int32 BlockX = 0; BlockX < BlocksX; BlockX++)
        {
            int32 LocalX = WorldRNG.RandRange(0, BlockSize - 1);
            int32 LocalY = WorldRNG.RandRange(0, BlockSize - 1);
            int32 GlobalX = BlockX * BlockSize + LocalX;
            int32 GlobalY = BlockY * BlockSize + LocalY;
            int32 Index = GlobalY * GridSizeX + GlobalX;
            if (Index < Cells.Num())
            {
                FGridCell& Cell = Cells[Index];
                Cell.bIsWater = true;
                Cell.AvailableIngredientID = NAME_None;
                FRealState waterState = FBiomeDefaults::GetDefaultWaterState(Cell.Biome);
                Cell.State = waterState;
                Cell.TargetState = waterState;
                Cell.HarvestStress = 0.0f;
                Cell.ResourceRegrowthTimer = 0.0f;
            }
        }
    }
}

void AGridWorldManager::RegenerateCellResource(FGridCell& Cell)
{
    Cell.AvailableIngredientID = FBiomeDefaults::GetRandomResourceForBiome(Cell.Biome, WorldRNG);
    Cell.ResourceRegrowthTimer = 0.0f;
}

FGridCell* AGridWorldManager::GetCell(int32 X, int32 Y)
{
    if (X >= 0 && X < GridSizeX && Y >= 0 && Y < GridSizeY)
        return &Cells[Y * GridSizeX + X];
    return nullptr;
}

const FGridCell* AGridWorldManager::GetCellConst(int32 X, int32 Y) const
{
    if (X >= 0 && X < GridSizeX && Y >= 0 && Y < GridSizeY)
        return &Cells[Y * GridSizeX + X];
    return nullptr;
}

void AGridWorldManager::SetTargetState(int32 X, int32 Y, const FRealState& NewState)
{
    FGridCell* Cell = GetCell(X, Y);
    if (Cell)
    {
        Cell->TargetState = NewState;
        MarkDirty(X, Y);
        if (!bInterpolationActive)
        {
            bInterpolationActive = true;
            SetActorTickEnabled(true);
        }
    }
}

void AGridWorldManager::UpdateMemory(FMemoryState& Memory, const FRealState& NewState, float Rate)
{
    const float CurrentTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;

    // Distortion: применять через ApplyDistortionDelta для непрерывности
    {
        const float TargetDistortion = NewState.Meta.Distortion;
        const float Delta = (TargetDistortion - Memory.AccumulatedDistortion) * Rate;
        FAlchemyWorldStateApplier::ApplyDistortionDelta(Memory, Delta, CurrentTime);
    }

    // Stability: остаётся с Lerp (не требует saturation)
    Memory.StabilityMemory += (NewState.Meta.Stability - Memory.StabilityMemory) * Rate;
    Memory.StabilityMemory = FMath::Clamp(Memory.StabilityMemory, 0.0f, 1.0f);

    // Purity: остаётся с Lerp
    Memory.HistoryPurity += (NewState.Meta.Purity - Memory.HistoryPurity) * Rate;
    Memory.HistoryPurity = FMath::Clamp(Memory.HistoryPurity, 0.0f, 1.0f);
}

void AGridWorldManager::RecalculateDistortionFromHarvestStress(FGridCell& Cell)
{
    const float t = FMath::Clamp(
        (Cell.HarvestStress - HarvestStressThreshold) / (1.0f - HarvestStressThreshold),
        0.0f, 1.0f
    );

    const float DistortionIncrease = t * MaxHarvestImpactOnDistortion;
    const float MagnitudeDecrease = t * MaxHarvestImpactOnMagnitude;

    const float CurrentTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;

    // Distortion: через ApplyDistortionDelta (непрерывность)
    FAlchemyWorldStateApplier::ApplyDistortionDelta(
        Cell.Memory,
        DistortionIncrease,
        CurrentTime
    );

    // TargetState подтягивается к Memory
    Cell.TargetState.Meta.Distortion = Cell.Memory.AccumulatedDistortion;

    // Magnitude: остаётся прямым
    Cell.TargetState.Magnitude = FMath::Clamp(
        Cell.TargetState.Magnitude - MagnitudeDecrease,
        0.0f, 1.0f
    );

    MarkDirty(Cell.X, Cell.Y);
}