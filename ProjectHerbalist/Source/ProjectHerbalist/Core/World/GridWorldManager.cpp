// GridWorldManager.cpp
#include "GridWorldManager.h"
#include "ProjectHerbalist.h"
#include "Core/Pipeline/HerbalistPipeline.h"
#include "Core/Types/BiomeTypes.h"
#include "Core/Harvest/HerbalistHarvest.h"
#include "Core/Inventory/HerbalistInventoryComponent.h"
#include "ProjectHerbalistGameModeBase.h"
#include "Player/HerbalistPlayerController.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "TimerManager.h"

    AGridWorldManager::AGridWorldManager()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = true;
}

void AGridWorldManager::BeginPlay()
{
    Super::BeginPlay();
    WorldRNG.Initialize(12345);
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

    // 1. Инициализация всех клеток (обычные)
    for (int32 Y = 0; Y < GridSizeY; Y++)
    {
        for (int32 X = 0; X < GridSizeX; X++)
        {
            int32 Index = Y * GridSizeX + X;
            int32 BlockX = X / BlockSize;
            int32 BlockY = Y / BlockSize;
            int32 BlockIndex = (BlockY * BlocksX + BlockX) % 4;
            EBiomeType biome;
            switch (BlockIndex)
            {
            case 0: biome = EBiomeType::MixedForest; break;
            case 1: biome = EBiomeType::Swamp; break;
            case 2: biome = EBiomeType::Steppe; break;
            default: biome = EBiomeType::Floodplain; break;
            }
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
            Cells[Index].AvailableResource = FBiomeDefaults::GetRandomResourceForBiome(biome, WorldRNG);
            Cells[Index].ResourceRegrowthTimer = 0.0f;
            Cells[Index].bIsWater = false;
        }
    }

    // 2. Назначение водных ячеек (одна на блок)
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
                Cell.AvailableResource = EResourceType::None;
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
    Cell.AvailableResource = FBiomeDefaults::GetRandomResourceForBiome(Cell.Biome, WorldRNG);
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
    Memory.AccumulatedDistortion += (NewState.Meta.Distortion - Memory.AccumulatedDistortion) * Rate;
    Memory.StabilityMemory += (NewState.Meta.Stability - Memory.StabilityMemory) * Rate;
    Memory.HistoryPurity += (NewState.Meta.Purity - Memory.HistoryPurity) * Rate;
    Memory.AccumulatedDistortion = FMath::Clamp(Memory.AccumulatedDistortion, 0.0f, 1.0f);
    Memory.StabilityMemory = FMath::Clamp(Memory.StabilityMemory, 0.0f, 1.0f);
    Memory.HistoryPurity = FMath::Clamp(Memory.HistoryPurity, 0.0f, 1.0f);
}

void AGridWorldManager::RecalculateDistortionFromHarvestStress(FGridCell& Cell)
{
    float t = FMath::Clamp((Cell.HarvestStress - HarvestStressThreshold) / (1.0f - HarvestStressThreshold), 0.0f, 1.0f);
    float DistortionIncrease = t * MaxHarvestImpactOnDistortion;
    float MagnitudeDecrease = t * MaxHarvestImpactOnMagnitude;
    float BaseDistortion = Cell.State.Meta.Distortion;
    Cell.TargetState.Meta.Distortion = FMath::Clamp(BaseDistortion + DistortionIncrease, 0.0f, 1.0f);
    Cell.TargetState.Magnitude = FMath::Clamp(Cell.TargetState.Magnitude - MagnitudeDecrease, 0.0f, 1.0f);
    MarkDirty(Cell.X, Cell.Y);
}

FRealState AGridWorldManager::HarvestFromCell(int32 X, int32 Y, const FConditionModifier& Conditions)
{
    FGridCell* Cell = GetCell(X, Y);
    if (!Cell) return FRealState();

    if (Cell->bIsWater)
    {
        FRealState Water = Cell->State;
        constexpr float k_condition = 0.4f;
        Water.Magnitude += k_condition * Conditions.DeltaMagnitude;
        Water.Direction.Body += k_condition * Conditions.DeltaDirection.Body;
        Water.Direction.Mind += k_condition * Conditions.DeltaDirection.Mind;
        Water.Direction.Spirit += k_condition * Conditions.DeltaDirection.Spirit;
        Water.Direction.Nature += k_condition * Conditions.DeltaDirection.Nature;
        Water.Meta.Distortion += k_condition * Conditions.DeltaDistortion;
        Water.Meta.Stability += k_condition * Conditions.DeltaStability;
        Water.Meta.Purity += k_condition * Conditions.DeltaPurity;
        Water.Meta.Potency += k_condition * Conditions.DeltaPotency;
        Water.Meta.Resonance += k_condition * Conditions.DeltaResonance;
        Water.Meta.Corruption += k_condition * Conditions.DeltaCorruption;

        Water.Direction.NormalizeSum();
        Water.Magnitude = FMath::Clamp(Water.Magnitude, 0.0f, 1.0f);
        Water.Meta.Distortion = FMath::Clamp(Water.Meta.Distortion, 0.0f, 1.0f);
        Water.Meta.Stability = FMath::Clamp(Water.Meta.Stability, 0.0f, 1.0f);
        Water.Meta.Purity = FMath::Clamp(Water.Meta.Purity, 0.0f, 1.0f);
        Water.Meta.Potency = FMath::Clamp(Water.Meta.Potency, 0.0f, 1.0f);
        Water.Meta.Resonance = FMath::Clamp(Water.Meta.Resonance, 0.0f, 1.0f);
        Water.Meta.Corruption = FMath::Clamp(Water.Meta.Corruption, 0.0f, 1.0f);

        UE_LOG(LogHerbalist, Log, TEXT("Harvested water from (%d,%d): Mag=%.2f Dist=%.2f"), X, Y, Water.Magnitude, Water.Meta.Distortion);
        return Water;
    }

    if (Cell->ResourceRegrowthTimer > 0.0f)
    {
        UE_LOG(LogHerbalist, Warning, TEXT("Cell (%d,%d) resource not ready (regrowing)"), X, Y);
        return FRealState();
    }
    FRealState Resource = FHerbalistHarvest::Harvest(Cell->AvailableResource, Cell->State, Conditions);
    Cell->ResourceRegrowthTimer = ResourceRegrowthTime;
    MarkRegrowing(X, Y);
    if (bHarvestAffectsBiome)
    {
        Cell->HarvestStress += HarvestStressIncrement;
        Cell->HarvestStress = FMath::Clamp(Cell->HarvestStress, 0.0f, 1.0f);
        MarkStress(X, Y);
        RecalculateDistortionFromHarvestStress(*Cell);
        if (!bInterpolationActive)
        {
            bInterpolationActive = true;
            SetActorTickEnabled(true);
        }
    }
    return Resource;
}

FRealState AGridWorldManager::HarvestFromCellSimple(int32 X, int32 Y)
{
    return HarvestFromCell(X, Y, FConditionModifier());
}

void AGridWorldManager::ApplyAlchemyResult(int32 X, int32 Y, const TArray<FInventoryItem>& Ingredients, const FIntent& Intent, FRngState& Rng)
{
    FGridCell* Cell = GetCell(X, Y);
    if (!Cell) return;

    FRealState OldState = Cell->State;
    FRealState NewState = HerbalistCore::Pipeline::ApplyMorok(
        Ingredients,
        Cell->State,
        Cell->Environment,
        Cell->Memory,
        Intent,
        Rng
    );

    // БИФУРКАЦИЯ (катастрофа / очищение)
    constexpr float BifurcationThreshold = 0.85f;
    constexpr float MaxDistortion = 1.0f;
    if (NewState.Meta.Distortion > BifurcationThreshold)
    {
        float Excess = (NewState.Meta.Distortion - BifurcationThreshold) / (MaxDistortion - BifurcationThreshold);
        float BaseChance = FMath::Pow(Excess, 1.5f);
        float Instability = (1.0f - NewState.Meta.Stability) * (1.0f - NewState.Meta.Purity);
        float InstabilityFactor = FMath::Lerp(0.5f, 1.0f, Instability);
        float MemoryFactor = 1.0f - Cell->Memory.StabilityMemory * 0.7f;
        float EventChance = BaseChance * InstabilityFactor * MemoryFactor;
        EventChance = FMath::Clamp(EventChance, 0.0f, 0.95f);

        if (HerbalistCore::Random01(Rng) < EventChance)
        {
            bool bCollapse = HerbalistCore::Random01(Rng) < 0.5f;
            if (bCollapse)
            {
                NewState.Meta.Distortion = 0.2f;
                NewState.Meta.Stability = FMath::Clamp(NewState.Meta.Stability + 0.1f, 0.0f, 1.0f);
                NewState.Direction.Body = FMath::Lerp(NewState.Direction.Body, 0.25f, 0.1f);
                NewState.Direction.Mind = FMath::Lerp(NewState.Direction.Mind, 0.25f, 0.1f);
                NewState.Direction.Spirit = FMath::Lerp(NewState.Direction.Spirit, 0.25f, 0.1f);
                NewState.Direction.Nature = FMath::Lerp(NewState.Direction.Nature, 0.25f, 0.1f);
                NewState.Direction.NormalizeSum();
                UE_LOG(LogHerbalist, Warning, TEXT("[CATASTROPHE] COLLAPSE! Distortion reset to 0.2, Stability+0.1"));
            }
            else
            {
                NewState.Meta.Distortion = 0.4f;
                float Boost = 0.3f * (1.0f - NewState.Meta.Stability);
                NewState.Meta.Stability = FMath::Clamp(NewState.Meta.Stability + Boost, 0.0f, 1.0f);
                NewState.Meta.Purity = FMath::Clamp(NewState.Meta.Purity + Boost * 0.8f, 0.0f, 1.0f);
                NewState.Direction.Body = FMath::Lerp(NewState.Direction.Body, 0.25f, 0.2f);
                NewState.Direction.Mind = FMath::Lerp(NewState.Direction.Mind, 0.25f, 0.2f);
                NewState.Direction.Spirit = FMath::Lerp(NewState.Direction.Spirit, 0.25f, 0.2f);
                NewState.Direction.Nature = FMath::Lerp(NewState.Direction.Nature, 0.25f, 0.2f);
                NewState.Direction.NormalizeSum();
                UE_LOG(LogHerbalist, Warning, TEXT("[CATASTROPHE] PURIFICATION! Distortion reset to 0.4, Stability+%.2f, Purity+%.2f"), Boost, Boost * 0.8f);
            }
        }
    }

    // Дельта
    FRealState Delta;
    Delta.Magnitude = NewState.Magnitude - OldState.Magnitude;
    Delta.Direction.Body = NewState.Direction.Body - OldState.Direction.Body;
    Delta.Direction.Mind = NewState.Direction.Mind - OldState.Direction.Mind;
    Delta.Direction.Spirit = NewState.Direction.Spirit - OldState.Direction.Spirit;
    Delta.Direction.Nature = NewState.Direction.Nature - OldState.Direction.Nature;
    Delta.Meta.Distortion = NewState.Meta.Distortion - OldState.Meta.Distortion;
    Delta.Meta.Stability = NewState.Meta.Stability - OldState.Meta.Stability;
    Delta.Meta.Purity = NewState.Meta.Purity - OldState.Meta.Purity;
    Delta.Meta.Potency = NewState.Meta.Potency - OldState.Meta.Potency;
    Delta.Meta.Resonance = NewState.Meta.Resonance - OldState.Meta.Resonance;
    Delta.Meta.Corruption = NewState.Meta.Corruption - OldState.Meta.Corruption;

    SetTargetState(X, Y, NewState);
    PropagateToNeighbors(X, Y, Delta, 0.5f, PropagationDepth);
}

void AGridWorldManager::ApplyAlchemyResult(int32 X, int32 Y, const TArray<FRealState>& Ingredients, const FIntent& Intent, FRngState& Rng)
{
    TArray<FInventoryItem> Items;
    for (const FRealState& State : Ingredients)
    {
        FInventoryItem Item;
        Item.Type = EResourceType::None;
        Item.State = State;
        Items.Add(Item);
    }
    ApplyAlchemyResult(X, Y, Items, Intent, Rng);
}

void AGridWorldManager::PropagateToNeighbors(int32 X, int32 Y, const FRealState& Delta, float Falloff, int32 Depth)
{
    if (Depth <= 0) return;

    struct FPropagationNode
    {
        int32 X, Y;
        int32 RemainingDepth;
        FRealState CurrentDelta;
    };

    TQueue<FPropagationNode> Queue;
    Queue.Enqueue({ X, Y, Depth, Delta });
    TSet<int32> Visited;

    while (!Queue.IsEmpty())
    {
        FPropagationNode Node;
        Queue.Dequeue(Node);
        if (Node.RemainingDepth <= 0) continue;

        for (int32 dx = -1; dx <= 1; dx++)
        {
            for (int32 dy = -1; dy <= 1; dy++)
            {
                if (dx == 0 && dy == 0) continue;
                int32 NX = Node.X + dx;
                int32 NY = Node.Y + dy;
                int32 Idx = GetCellIndex(NX, NY);
                if (Visited.Contains(Idx)) continue;
                FGridCell* Neighbor = GetCell(NX, NY);
                if (!Neighbor) continue;

                Visited.Add(Idx);

                FRealState WeakDelta = Node.CurrentDelta;
                WeakDelta.Magnitude *= Falloff;
                WeakDelta.Meta.Distortion = FMath::Max(0.0f, WeakDelta.Meta.Distortion * Falloff);
                WeakDelta.Meta.Corruption *= Falloff;
                WeakDelta.Meta.Stability = 0.0f;
                WeakDelta.Meta.Purity = 0.0f;
                WeakDelta.Meta.Potency = 0.0f;
                WeakDelta.Meta.Resonance = 0.0f;
                WeakDelta.Direction.Body = 0.0f;
                WeakDelta.Direction.Mind = 0.0f;
                WeakDelta.Direction.Spirit = 0.0f;
                WeakDelta.Direction.Nature = 0.0f;

                FRealState NewTargetState = Neighbor->TargetState;
                NewTargetState.Magnitude += WeakDelta.Magnitude;
                NewTargetState.Meta.Distortion += WeakDelta.Meta.Distortion;
                NewTargetState.Meta.Corruption += WeakDelta.Meta.Corruption;

                NewTargetState.Magnitude = FMath::Clamp(NewTargetState.Magnitude, 0.0f, 1.0f);
                NewTargetState.Meta.Distortion = FMath::Clamp(NewTargetState.Meta.Distortion, 0.0f, 1.0f);
                NewTargetState.Meta.Corruption = FMath::Clamp(NewTargetState.Meta.Corruption, 0.0f, 1.0f);

                Neighbor->TargetState = NewTargetState;
                MarkDirty(NX, NY);

                if (Node.RemainingDepth - 1 > 0)
                {
                    Queue.Enqueue({ NX, NY, Node.RemainingDepth - 1, WeakDelta });
                }
            }
        }
    }
}

void AGridWorldManager::InterpolateCell(FGridCell& Cell, float DeltaTime)
{
    FRealState& Cur = Cell.State;
    const FRealState& Target = Cell.TargetState;

    Cur.Magnitude = FMath::FInterpTo(Cur.Magnitude, Target.Magnitude, DeltaTime, StateInterpolationSpeed);
    Cur.Direction.Body = FMath::FInterpTo(Cur.Direction.Body, Target.Direction.Body, DeltaTime, StateInterpolationSpeed);
    Cur.Direction.Mind = FMath::FInterpTo(Cur.Direction.Mind, Target.Direction.Mind, DeltaTime, StateInterpolationSpeed);
    Cur.Direction.Spirit = FMath::FInterpTo(Cur.Direction.Spirit, Target.Direction.Spirit, DeltaTime, StateInterpolationSpeed);
    Cur.Direction.Nature = FMath::FInterpTo(Cur.Direction.Nature, Target.Direction.Nature, DeltaTime, StateInterpolationSpeed);
    Cur.Meta.Distortion = FMath::FInterpTo(Cur.Meta.Distortion, Target.Meta.Distortion, DeltaTime, StateInterpolationSpeed);
    Cur.Meta.Stability = FMath::FInterpTo(Cur.Meta.Stability, Target.Meta.Stability, DeltaTime, StateInterpolationSpeed);
    Cur.Meta.Purity = FMath::FInterpTo(Cur.Meta.Purity, Target.Meta.Purity, DeltaTime, StateInterpolationSpeed);
    Cur.Meta.Potency = FMath::FInterpTo(Cur.Meta.Potency, Target.Meta.Potency, DeltaTime, StateInterpolationSpeed);
    Cur.Meta.Resonance = FMath::FInterpTo(Cur.Meta.Resonance, Target.Meta.Resonance, DeltaTime, StateInterpolationSpeed);
    Cur.Meta.Corruption = FMath::FInterpTo(Cur.Meta.Corruption, Target.Meta.Corruption, DeltaTime, StateInterpolationSpeed);

    Cur.Direction.NormalizeSum();
}

void AGridWorldManager::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (bEnableRecovery && StressCells.Num() > 0)
    {
        TSet<int32> ProcessedStress = StressCells;
        for (int32 Idx : ProcessedStress)
        {
            int32 X = Idx % GridSizeX;
            int32 Y = Idx / GridSizeX;
            FGridCell* Cell = GetCell(X, Y);
            if (!Cell) continue;

            float OldStress = Cell->HarvestStress;
            Cell->HarvestStress = FMath::Max(0.0f, Cell->HarvestStress - HarvestStressDecayRate * DeltaTime);
            if (!FMath::IsNearlyEqual(OldStress, Cell->HarvestStress, 1e-4f))
            {
                RecalculateDistortionFromHarvestStress(*Cell);
                MarkDirty(X, Y);
            }
            if (Cell->HarvestStress <= 0.0f)
                StressCells.Remove(Idx);
        }
    }

    TSet<int32> ProcessedRegrow = RegrowingCells;
    for (int32 Idx : ProcessedRegrow)
    {
        int32 X = Idx % GridSizeX;
        int32 Y = Idx / GridSizeX;
        FGridCell* Cell = GetCell(X, Y);
        if (!Cell) continue;

        Cell->ResourceRegrowthTimer -= DeltaTime;
        if (Cell->ResourceRegrowthTimer <= 0.0f)
        {
            RegenerateCellResource(*Cell);
            UnmarkRegrowing(X, Y);
            UE_LOG(LogHerbalist, Log, TEXT("Cell (%d,%d) resource regenerated"), X, Y);
        }
    }

    TSet<int32> ProcessedDirty = DirtyCells;
    for (int32 Idx : ProcessedDirty)
    {
        int32 X = Idx % GridSizeX;
        int32 Y = Idx / GridSizeX;
        FGridCell* Cell = GetCell(X, Y);
        if (!Cell) continue;

        bool bStateNear =
            FMath::IsNearlyEqual(Cell->State.Magnitude, Cell->TargetState.Magnitude, 0.001f) &&
            FMath::IsNearlyEqual(Cell->State.Meta.Distortion, Cell->TargetState.Meta.Distortion, 0.001f) &&
            FMath::IsNearlyEqual(Cell->State.Meta.Stability, Cell->TargetState.Meta.Stability, 0.001f) &&
            FMath::IsNearlyEqual(Cell->State.Meta.Purity, Cell->TargetState.Meta.Purity, 0.001f) &&
            FMath::IsNearlyEqual(Cell->State.Meta.Potency, Cell->TargetState.Meta.Potency, 0.001f) &&
            FMath::IsNearlyEqual(Cell->State.Meta.Resonance, Cell->TargetState.Meta.Resonance, 0.001f) &&
            FMath::IsNearlyEqual(Cell->State.Meta.Corruption, Cell->TargetState.Meta.Corruption, 0.001f) &&
            FMath::IsNearlyEqual(Cell->State.Direction.Body, Cell->TargetState.Direction.Body, 0.001f) &&
            FMath::IsNearlyEqual(Cell->State.Direction.Mind, Cell->TargetState.Direction.Mind, 0.001f) &&
            FMath::IsNearlyEqual(Cell->State.Direction.Spirit, Cell->TargetState.Direction.Spirit, 0.001f) &&
            FMath::IsNearlyEqual(Cell->State.Direction.Nature, Cell->TargetState.Direction.Nature, 0.001f);

        if (!bStateNear)
        {
            InterpolateCell(*Cell, DeltaTime);
            UpdateMemory(Cell->Memory, Cell->State, 0.05f * DeltaTime);
        }
        else
        {
            Cell->State = Cell->TargetState;
            UpdateMemory(Cell->Memory, Cell->State, 0.1f);
            DirtyCells.Remove(Idx);
        }
    }

    bool bShouldTick = (DirtyCells.Num() > 0) || (RegrowingCells.Num() > 0) || (StressCells.Num() > 0);
    if (bInterpolationActive != bShouldTick)
    {
        bInterpolationActive = bShouldTick;
        SetActorTickEnabled(bShouldTick);
    }
}

void AGridWorldManager::RedrawDebugBoxes()
{
    if (!bEnableDebugDraw) return;
    FlushPersistentDebugLines(GetWorld());
    for (const FGridCell& Cell : Cells)
    {
        FVector Center = FVector(Cell.X * CellSize, Cell.Y * CellSize, 0.0f);
        FVector Extent = FVector(CellSize / 2.0f, CellSize / 2.0f, CellHeight / 2.0f);
        FColor Color;
        if (Cell.bIsWater)
        {
            Color = FColor::Blue;
        }
        else
        {
            float Dist = Cell.Memory.AccumulatedDistortion;
            FLinearColor LinearColor = FLinearColor::LerpUsingHSV(FLinearColor::Green, FLinearColor::Red, Dist);
            Color = LinearColor.ToFColor(false);
        }
        DrawDebugBox(GetWorld(), Center, Extent, Color, true, -1.0f, 0, BorderThickness);
    }
}

void AGridWorldManager::SelectCell(int32 X, int32 Y)
{
    if (GetCell(X, Y))
    {
        SelectedX = X;
        SelectedY = Y;
        UE_LOG(LogHerbalist, Log, TEXT("Selected cell (%d, %d)"), X, Y);
    }
    else
    {
        UE_LOG(LogHerbalist, Warning, TEXT("Invalid cell (%d, %d)"), X, Y);
    }
}

FString AGridWorldManager::GetSelectedCellInfo() const
{
    const FGridCell* Cell = GetCellConst(SelectedX, SelectedY);
    if (!Cell) return TEXT("No cell selected");
    FString ResourceStr = Cell->bIsWater ? TEXT("Water") : FString::Printf(TEXT("%d"), (int32)Cell->AvailableResource);
    return FString::Printf(TEXT("Cell (%d,%d): Mag=%.2f, Dist=%.2f, Stress=%.3f, Resource=%s, Regrowth=%.1f"),
        SelectedX, SelectedY,
        Cell->State.Magnitude,
        Cell->State.Meta.Distortion,
        Cell->HarvestStress,
        *ResourceStr,
        Cell->ResourceRegrowthTimer);
}

void AGridWorldManager::GetSelectedCellInfoBP(int32& X, int32& Y, FString& ResourceName, float& RegrowthTimer, float& Distortion, float& HarvestStress)
{
    X = SelectedX;
    Y = SelectedY;
    if (X >= 0 && Y >= 0)
    {
        FGridCell* Cell = GetCell(X, Y);
        if (Cell)
        {
            if (Cell->bIsWater)
            {
                ResourceName = TEXT("Вода");
            }
            else
            {
                ResourceName = FHerbalistHarvest::GetResourceName(Cell->AvailableResource, false);
            }
            RegrowthTimer = Cell->ResourceRegrowthTimer;
            Distortion = Cell->State.Meta.Distortion;
            HarvestStress = Cell->HarvestStress;
            return;
        }
    }
    ResourceName = TEXT("None");
    RegrowthTimer = 0.0f;
    Distortion = 0.0f;
    HarvestStress = 0.0f;
}

void AGridWorldManager::HarvestTest(int32 X, int32 Y)
{
    // ЗАЩИТА ОТ ДВОЙНОГО ВЫЗОВА (добавлено)
    int32 CellIdx = Y * GridSizeX + X;
    float CurrentTime = GetWorld()->GetTimeSeconds();
    if (LastHarvestTimeMap.Contains(CellIdx))
    {
        float TimeSinceLast = CurrentTime - LastHarvestTimeMap[CellIdx];
        if (TimeSinceLast < HarvestCooldown)
        {
            UE_LOG(LogHerbalist, Warning, TEXT("HarvestTest: too fast from (%d,%d), ignored (%.3f s)"), X, Y, TimeSinceLast);
            return;
        }
    }
    LastHarvestTimeMap.Add(CellIdx, CurrentTime);

    // ОРИГИНАЛЬНЫЙ КОД HARVESTTEST (без изменений)
    FRealState Res = HarvestFromCellSimple(X, Y);
    AHerbalistPlayerController* PC = Cast<AHerbalistPlayerController>(GetWorld()->GetFirstPlayerController());
    if (PC && PC->InventoryComponent)
    {
        FGridCell* Cell = GetCell(X, Y);
        if (Cell)
        {
            EResourceType Type = Cell->bIsWater ? EResourceType::Water : Cell->AvailableResource;
            UE_LOG(LogHerbalist, Log, TEXT("HarvestTest: cell (%d,%d), Type=%d, bIsWater=%d, AvailableResource=%d"),
                X, Y, (int32)Type, Cell->bIsWater, (int32)Cell->AvailableResource);

            if (Type != EResourceType::None)
            {
                bool bAdded = PC->InventoryComponent->AddItem(Res, Type);
                UE_LOG(LogHerbalist, Log, TEXT("HarvestTest: AddItem result = %d, Mag=%.2f, Dist=%.2f"),
                    bAdded, Res.Magnitude, Res.Meta.Distortion);
            }
            else
            {
                UE_LOG(LogHerbalist, Warning, TEXT("Harvested resource has None type, skipping add to inventory"));
            }
        }
    }
    FGridCell* Cell = GetCell(X, Y);
    FString ResourceName;
    if (Cell)
    {
        if (Cell->bIsWater)
            ResourceName = TEXT("Water");
        else
            ResourceName = FHerbalistHarvest::GetResourceName(Cell->AvailableResource, false);
    }
    UE_LOG(LogHerbalist, Log, TEXT("Harvested from (%d,%d): Mag=%.2f Dist=%.2f Stress=%.3f Resource=%s"),
        X, Y, Res.Magnitude, Res.Meta.Distortion, Cell ? Cell->HarvestStress : -1.0f, *ResourceName);
}

void AGridWorldManager::MassHarvestTest(int32 X, int32 Y, int32 Count)
{
    for (int32 i = 0; i < Count; ++i) HarvestTest(X, Y);
    UE_LOG(LogHerbalist, Log, TEXT("Mass harvest %d times at (%d,%d)"), Count, X, Y);
}

void AGridWorldManager::ApplyTest(int32 X, int32 Y)
{
    AHerbalistPlayerController* PC = Cast<AHerbalistPlayerController>(GetWorld()->GetFirstPlayerController());
    if (!PC || !PC->InventoryComponent)
    {
        UE_LOG(LogHerbalist, Warning, TEXT("No player controller or inventory component found"));
        return;
    }

    TArray<FInventoryItem> Inventory = PC->InventoryComponent->GetItems();
    if (Inventory.Num() < 2)
    {
        UE_LOG(LogHerbalist, Warning, TEXT("Need at least 2 resources in inventory"));
        return;
    }

    FInventoryItem Ingredient1 = Inventory[0];
    FInventoryItem Ingredient2 = Inventory[1];
    PC->InventoryComponent->RemoveItem(1);
    PC->InventoryComponent->RemoveItem(0);

    TArray<FInventoryItem> Ingredients = { Ingredient1, Ingredient2 };
    FIntent Intent;
    Intent.Coherence = 0.5f;
    FRngState Rng;
    Rng.Seed = 12345;
    ApplyAlchemyResult(X, Y, Ingredients, Intent, Rng);
    UE_LOG(LogHerbalist, Log, TEXT("Applied alchemy to (%d,%d) and consumed two resources"), X, Y);
}

void AGridWorldManager::ShowInventory()
{
    AHerbalistPlayerController* PC = Cast<AHerbalistPlayerController>(GetWorld()->GetFirstPlayerController());
    if (!PC || !PC->InventoryComponent)
    {
        UE_LOG(LogHerbalist, Warning, TEXT("No player controller or inventory component found"));
        return;
    }

    TArray<FInventoryItem> Inventory = PC->InventoryComponent->GetItems();
    UE_LOG(LogHerbalist, Log, TEXT("=== INVENTORY (%d items) ==="), Inventory.Num());
    for (int32 i = 0; i < Inventory.Num(); ++i)
    {
        const FInventoryItem& Item = Inventory[i];
        const FRealState& Res = Item.State;
        FString Name = FHerbalistHarvest::GetResourceName(Item.Type, false);
        UE_LOG(LogHerbalist, Log, TEXT("[%d] %s: Mag=%.2f, Dist=%.2f, Pot=%.2f Res=%.2f Cor=%.2f, Dir: (%.2f,%.2f,%.2f,%.2f)"),
            i, *Name, Res.Magnitude, Res.Meta.Distortion, Res.Meta.Potency, Res.Meta.Resonance, Res.Meta.Corruption,
            Res.Direction.Body, Res.Direction.Mind, Res.Direction.Spirit, Res.Direction.Nature);
    }
}