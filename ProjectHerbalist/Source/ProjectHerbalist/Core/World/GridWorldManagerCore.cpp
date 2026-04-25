// GridWorldManagerCore.cpp
#include "Core/World/GridWorldManager.h"
#include "Core/BiomeGraph/BiomeGraphSubsystem.h"
#include "Core/Data/WaterTypeRegistry.h"
#include "Core/Harvest/HarvestService.h"
#include "Core/Pipeline/AlchemyWorldStateApplier.h"
#include "Core/Types/BiomeTypes.h"
#include "Core/Data/IngredientRegistry.h"
#include "Core/Resources/AHerbalistResourceActor.h"
#include "Engine/World.h"
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
        Sample.ZaryanaValue = 1.f - Cell.State.Meta.Distortion;
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
        int32 Cnt = Counts[Pair.Key];
        if (Cnt > 0) Pair.Value /= Cnt;
    }
    return Centers;
}

void AGridWorldManager::ApplyBiomeInfluences(const TMap<FName, float>& MorokFields, const TMap<FName, float>& ZaryanaFields, float GlobalScale)
{
    for (FGridCell& Cell : Cells)
    {
        FName BiomeID = FBiomeDefaults::BiomeTypeToName(Cell.Biome);
        const float* ZaryanaField = ZaryanaFields.Find(BiomeID);
        if (!ZaryanaField) continue;

        float ZaryanaInfluence = *ZaryanaField * 0.05f * GlobalScale;
        Cell.TargetState.Meta.Stability = FMath::Clamp(Cell.TargetState.Meta.Stability + ZaryanaInfluence, 0.f, 1.f);
        Cell.TargetState.Meta.Purity = FMath::Clamp(Cell.TargetState.Meta.Purity + ZaryanaInfluence * 0.5f, 0.f, 1.f);
    }
}

AGridWorldManager::AGridWorldManager()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = false;
}

void AGridWorldManager::BeginPlay()
{
    Super::BeginPlay();
    WorldRNG.Initialize(12345);
    HarvestService = NewObject<UHarvestService>(this);
}

void AGridWorldManager::InitializeCells()
{
    const int32 TotalCells = GridSizeX * GridSizeY;
    Cells.SetNum(TotalCells);

    TArray<EBiomeType> AllBiomes = FBiomeDefaults::GetAllBiomeTypes();
    if (AllBiomes.Num() == 0)
    {
        AllBiomes = {
            EBiomeType::Tundra, EBiomeType::Taiga, EBiomeType::MixedForest,
            EBiomeType::BroadleafForest, EBiomeType::ForestSteppe,
            EBiomeType::Steppe, EBiomeType::Floodplain, EBiomeType::Bog
        };
    }

    const int32 BlockSize = 5;
    const int32 BlocksX = GridSizeX / BlockSize;
    const int32 BlocksY = GridSizeY / BlockSize;

    for (int32 Y = 0; Y < GridSizeY; ++Y)
    {
        for (int32 X = 0; X < GridSizeX; ++X)
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
            Cells[Index].AvailableIngredientID = FIngredientRegistry::GetRandomResourceForBiome(biome, WorldRNG);
            Cells[Index].ResourceRegrowthTimer = 0.0f;
            Cells[Index].bIsWater = false;
            Cells[Index].WaterTypeID = NAME_None;
        }
    }

    int32 TargetWaterCount = TotalCells * 0.2f;
    if (TargetWaterCount < 1) TargetWaterCount = 1;
    TArray<bool> IsWaterAlready;
    IsWaterAlready.Init(false, TotalCells);
    int32 PlacedWater = 0;
    while (PlacedWater < TargetWaterCount)
    {
        int32 W = (WorldRNG.FRand() < 0.5f) ? 1 : 2;
        int32 H = (WorldRNG.FRand() < 0.5f) ? 1 : 2;
        W = FMath::Min(W, 2);
        H = FMath::Min(H, 2);
        int32 StartX = WorldRNG.RandRange(0, GridSizeX - W);
        int32 StartY = WorldRNG.RandRange(0, GridSizeY - H);
        bool bAreaFree = true;
        for (int32 dy = 0; dy < H; ++dy)
        {
            for (int32 dx = 0; dx < W; ++dx)
            {
                int32 X = StartX + dx;
                int32 Y = StartY + dy;
                int32 Idx = Y * GridSizeX + X;
                if (IsWaterAlready[Idx]) { bAreaFree = false; break; }
            }
            if (!bAreaFree) break;
        }
        if (!bAreaFree) continue;
        for (int32 dy = 0; dy < H; ++dy)
        {
            for (int32 dx = 0; dx < W; ++dx)
            {
                int32 X = StartX + dx;
                int32 Y = StartY + dy;
                int32 Idx = Y * GridSizeX + X;
                FGridCell& Cell = Cells[Idx];
                Cell.bIsWater = true;
                Cell.AvailableIngredientID = NAME_None;
                Cell.WaterTypeID = FWaterTypeRegistry::GetRandomWaterType(Cell.Biome, WorldRNG);
                FRealState waterState = FBiomeDefaults::GetDefaultWaterState(Cell.Biome);
                if (const FWaterTypeRow* WaterRow = FWaterTypeRegistry::GetWaterType(Cell.WaterTypeID))
                {
                    waterState.Meta.Purity = WaterRow->BasePurity;
                    waterState.Meta.Distortion = WaterRow->BaseDistortion;
                    waterState.Meta.Stability = WaterRow->BaseStability;
                    waterState.Meta.Potency = WaterRow->BasePotency;
                    waterState.Meta.Corruption = WaterRow->BaseCorruption;
                }
                Cell.State = waterState;
                Cell.TargetState = waterState;
                Cell.HarvestStress = 0.0f;
                Cell.ResourceRegrowthTimer = 0.0f;
                IsWaterAlready[Idx] = true;
                PlacedWater++;
            }
        }
        if (PlacedWater >= TargetWaterCount) break;
    }

    SetActorTickEnabled(true);
}

void AGridWorldManager::RegenerateCellResource(FGridCell& Cell)
{
    Cell.AvailableIngredientID = FIngredientRegistry::GetRandomResourceForBiome(Cell.Biome, WorldRNG);
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
    const float TargetDistortion = NewState.Meta.Distortion;
    const float Delta = (TargetDistortion - Memory.AccumulatedDistortion) * Rate;
    FAlchemyWorldStateApplier::ApplyDistortionDelta(Memory, Delta, CurrentTime);
}

void AGridWorldManager::RecalculateDistortionFromHarvestStress(FGridCell& Cell)
{
    float t = FMath::Clamp(Cell.HarvestStress, 0.0f, 1.0f);
    const float DistortionIncrease = t * MaxHarvestImpactOnDistortion;
    const float MagnitudeDecrease = t * MaxHarvestImpactOnMagnitude;
    const float CurrentTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;

    FAlchemyWorldStateApplier::ApplyDistortionDelta(Cell.Memory, DistortionIncrease, CurrentTime);
    Cell.TargetState.Meta.Distortion = Cell.Memory.AccumulatedDistortion;
    Cell.TargetState.Magnitude = FMath::Clamp(Cell.TargetState.Magnitude - MagnitudeDecrease, 0.0f, 1.0f);
    MarkDirty(Cell.X, Cell.Y);
}

TArray<FName> AGridWorldManager::GetResourcesForBiome(EBiomeType Biome) const
{
    return FIngredientRegistry::GetResourcesForBiome(Biome);
}

void AGridWorldManager::SpawnResourcesForCell(FGridCell& Cell)
{
    // Заглушка
}

void AGridWorldManager::OnResourceCollected(AHerbalistResourceActor* Actor)
{
    // Заглушка
}

void AGridWorldManager::SpawnResourceActor(FName IngredientID, int32 X, int32 Y, const FVector& Offset)
{
    // Заглушка
}

// ==================== ОТРИСОВКА ====================

#if WITH_EDITOR
void AGridWorldManager::DrawGridDebug()
{
    if (!bEnableDebugDraw) return;
    for (const FGridCell& Cell : Cells)
    {
        FVector Center = GetCellWorldPosition(Cell.X, Cell.Y);
        FVector Extent = FVector(CellSize/2.0f, CellSize/2.0f, CellHeight/2.0f);
        FColor Color = Cell.bIsWater ? FColor::White : FColor(64,64,64);
        DrawDebugBox(GetWorld(), Center, Extent, Color, false, 0.0f, 0, BorderThickness);
    }
}
#endif

void AGridWorldManager::DrawBiomeGraphDebug()
{
#if WITH_EDITOR
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
                float Thickness = 2.f;
                FColor Color = FColor::Yellow;
                DrawDebugLine(World, *FromPos, *ToPos, Color, false, 0.0f, 0, Thickness);
            }
        }

        for (const auto& Pair : Nodes)
        {
            const FVector* Pos = Centers.Find(Pair.Key);
            if (Pos)
            {
                const FBiomeGraphNode& Node = Pair.Value;
                FColor Color = FLinearColor::LerpUsingHSV(FLinearColor::Green, FLinearColor::Red, Node.MorokField).ToFColor(false);
                float Size = 30.f;
                DrawDebugSphere(World, *Pos, Size, 12, Color, false, 0.0f, 0, 2.f);
                DrawDebugString(World, *Pos + FVector(0, 0, Size + 20), Pair.Key.ToString(), nullptr, FColor::White, 0.0f, true, 1.2f);
            }
        }
    }

    if (bShowCellDistortion || bShowCellInfluence)
    {
        for (const FGridCell& Cell : Cells)
        {
            FVector Pos = GetCellWorldPosition(Cell.X, Cell.Y);
            if (bShowCellDistortion)
            {
                FColor Color = FLinearColor::LerpUsingHSV(FLinearColor::Green, FLinearColor::Red, Cell.State.Meta.Distortion).ToFColor(false);
                DrawDebugString(World, Pos + FVector(0, 0, 30), FString::Printf(TEXT("%.2f"), Cell.State.Meta.Distortion), nullptr, Color, 0.0f, true);
            }
            if (bShowCellInfluence)
            {
                float Influence = Cell.TargetState.Meta.Distortion - Cell.State.Meta.Distortion;
                FColor Color = Influence > 0 ? FColor::Red : (Influence < 0 ? FColor::Blue : FColor::White);
                DrawDebugString(World, Pos + FVector(0, 0, 50), FString::Printf(TEXT("Δ%.2f"), Influence), nullptr, Color, 0.0f, true);
            }
        }
    }
#endif
}