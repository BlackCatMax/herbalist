// GridWorldManager.cpp
#include "GridWorldManager.h"
#include "Player/HerbalistPlayerController.h"
#include "ProjectHerbalist.h"
#include "Core/Pipeline/HerbalistPipeline.h"
#include "Core/Types/BiomeTypes.h"
#include "Core/Harvest/HerbalistHarvest.h"
#include "ProjectHerbalistGameModeBase.h"
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
    WorldRNG.Initialize(12345); // фиксированный сид для детерминированности
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
    // Вычисляем вклад стресса как добавку к базовому искажению (из текущего устоявшегося состояния)
    float t = FMath::Clamp((Cell.HarvestStress - HarvestStressThreshold) / (1.0f - HarvestStressThreshold), 0.0f, 1.0f);
    float DistortionIncrease = t * MaxHarvestImpactOnDistortion;
    float MagnitudeDecrease = t * MaxHarvestImpactOnMagnitude;

    // Берём базу из текущего состояния (State), а не из TargetState
    float BaseDistortion = Cell.State.Meta.Distortion;
    Cell.TargetState.Meta.Distortion = FMath::Clamp(BaseDistortion + DistortionIncrease, 0.0f, 1.0f);
    Cell.TargetState.Magnitude = FMath::Clamp(Cell.TargetState.Magnitude - MagnitudeDecrease, 0.0f, 1.0f);
    MarkDirty(Cell.X, Cell.Y);
}

FRealState AGridWorldManager::HarvestFromCell(int32 X, int32 Y, const FConditionModifier& Conditions)
{
    FGridCell* Cell = GetCell(X, Y);
    if (!Cell) return FRealState();
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

void AGridWorldManager::ApplyAlchemyResult(int32 X, int32 Y, const TArray<FRealState>& Ingredients, const FIntent& Intent, FRngState& Rng)
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

    // ========== БИФУРКАЦИЯ (катастрофа / очищение) ==========
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
                // Сдвиг направления к S₀ (0.25 по каждой оси) с нормализацией по сумме
                FVector4 DirVec(NewState.Direction.Body, NewState.Direction.Mind, NewState.Direction.Spirit, NewState.Direction.Nature);
                FVector4 TargetDir(0.25f, 0.25f, 0.25f, 0.25f);
                DirVec = FMath::Lerp(DirVec, TargetDir, 0.1f);
                float Sum = DirVec.X + DirVec.Y + DirVec.Z + DirVec.W;
                if (Sum > KINDA_SMALL_NUMBER)
                {
                    NewState.Direction.Body = DirVec.X / Sum;
                    NewState.Direction.Mind = DirVec.Y / Sum;
                    NewState.Direction.Spirit = DirVec.Z / Sum;
                    NewState.Direction.Nature = DirVec.W / Sum;
                }
                else
                {
                    NewState.Direction.Body = NewState.Direction.Mind = NewState.Direction.Spirit = NewState.Direction.Nature = 0.25f;
                }
                UE_LOG(LogHerbalist, Warning, TEXT("[CATASTROPHE] COLLAPSE! Distortion reset to 0.2, Stability+0.1, Direction shifted to center"));
            }
            else
            {
                NewState.Meta.Distortion = 0.4f;
                float Boost = 0.3f * (1.0f - NewState.Meta.Stability);
                NewState.Meta.Stability = FMath::Clamp(NewState.Meta.Stability + Boost, 0.0f, 1.0f);
                NewState.Meta.Purity = FMath::Clamp(NewState.Meta.Purity + Boost * 0.8f, 0.0f, 1.0f);
                FVector4 DirVec(NewState.Direction.Body, NewState.Direction.Mind, NewState.Direction.Spirit, NewState.Direction.Nature);
                FVector4 TargetDir(0.25f, 0.25f, 0.25f, 0.25f);
                DirVec = FMath::Lerp(DirVec, TargetDir, 0.2f);
                float Sum = DirVec.X + DirVec.Y + DirVec.Z + DirVec.W;
                if (Sum > KINDA_SMALL_NUMBER)
                {
                    NewState.Direction.Body = DirVec.X / Sum;
                    NewState.Direction.Mind = DirVec.Y / Sum;
                    NewState.Direction.Spirit = DirVec.Z / Sum;
                    NewState.Direction.Nature = DirVec.W / Sum;
                }
                else
                {
                    NewState.Direction.Body = NewState.Direction.Mind = NewState.Direction.Spirit = NewState.Direction.Nature = 0.25f;
                }
                UE_LOG(LogHerbalist, Warning, TEXT("[CATASTROPHE] PURIFICATION! Distortion reset to 0.4, Stability+%.2f, Purity+%.2f, Direction shifted to center"), Boost, Boost * 0.8f);
            }
        }
    }

    // ========== ВЫЧИСЛЕНИЕ ДЕЛЬТЫ ==========
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

                // Ослабляем дельту
                FRealState WeakDelta = Node.CurrentDelta;
                WeakDelta.Magnitude *= Falloff;
                // Распространяем только Distortion (только положительную) и Corruption
                WeakDelta.Meta.Distortion = FMath::Max(0.0f, WeakDelta.Meta.Distortion * Falloff);
                WeakDelta.Meta.Corruption *= Falloff;
                // Остальные Meta не распространяем
                WeakDelta.Meta.Stability = 0.0f;
                WeakDelta.Meta.Purity = 0.0f;
                WeakDelta.Meta.Potency = 0.0f;
                WeakDelta.Meta.Resonance = 0.0f;
                // Направление не распространяем
                WeakDelta.Direction.Body = 0.0f;
                WeakDelta.Direction.Mind = 0.0f;
                WeakDelta.Direction.Spirit = 0.0f;
                WeakDelta.Direction.Nature = 0.0f;

                // Применяем к TargetState соседа
                FRealState NewTargetState = Neighbor->TargetState;
                NewTargetState.Magnitude += WeakDelta.Magnitude;
                NewTargetState.Meta.Distortion += WeakDelta.Meta.Distortion;
                NewTargetState.Meta.Corruption += WeakDelta.Meta.Corruption;

                // Клиппинг
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

    // Нормализация направления по сумме (композиция)
    float Sum = Cur.Direction.Body + Cur.Direction.Mind + Cur.Direction.Spirit + Cur.Direction.Nature;
    if (Sum > KINDA_SMALL_NUMBER)
    {
        Cur.Direction.Body /= Sum;
        Cur.Direction.Mind /= Sum;
        Cur.Direction.Spirit /= Sum;
        Cur.Direction.Nature /= Sum;
    }
    else
    {
        Cur.Direction.Body = Cur.Direction.Mind = Cur.Direction.Spirit = Cur.Direction.Nature = 0.25f;
    }
}

void AGridWorldManager::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    // ФАЗА 1: Decay стресса (только клетки со стрессом)
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

    // ФАЗА 2: Регенерация ресурсов
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

    // ФАЗА 3: Интерполяция грязных клеток и обновление памяти
    TSet<int32> ProcessedDirty = DirtyCells;
    for (int32 Idx : ProcessedDirty)
    {
        int32 X = Idx % GridSizeX;
        int32 Y = Idx / GridSizeX;
        FGridCell* Cell = GetCell(X, Y);
        if (!Cell) continue;

        // Проверяем достижение всех параметров
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
            // Память обновляем плавно от текущего интерполируемого состояния
            UpdateMemory(Cell->Memory, Cell->State, 0.05f * DeltaTime);
        }
        else
        {
            Cell->State = Cell->TargetState;
            UpdateMemory(Cell->Memory, Cell->State, 0.1f);
            DirtyCells.Remove(Idx);
        }
    }

    // Управление тиком
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
        float Dist = Cell.Memory.AccumulatedDistortion;
        FLinearColor LinearColor = FLinearColor::LerpUsingHSV(FLinearColor::Green, FLinearColor::Red, Dist);
        FColor Color = LinearColor.ToFColor(false);
        DrawDebugBox(GetWorld(), Center, Extent, Color, true, -1.0f, 0, BorderThickness);
    }
}

// ========== UI и выбор ячейки ==========
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
    return FString::Printf(TEXT("Cell (%d,%d): Mag=%.2f, Dist=%.2f, Stress=%.3f, Resource=%d, Regrowth=%.1f"),
        SelectedX, SelectedY,
        Cell->State.Magnitude,
        Cell->State.Meta.Distortion,
        Cell->HarvestStress,
        (int32)Cell->AvailableResource,
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
            ResourceName = FHerbalistHarvest::GetResourceName(Cell->AvailableResource, false);
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

// ========== Тестовые команды ==========
void AGridWorldManager::HarvestTest(int32 X, int32 Y)
{
    FRealState Res = HarvestFromCellSimple(X, Y);
    AHerbalistPlayerController* PC = Cast<AHerbalistPlayerController>(GetWorld()->GetFirstPlayerController());
    if (PC && PC->InventoryComponent)
    {
        FGridCell* Cell = GetCell(X, Y);
        if (Cell)
            PC->InventoryComponent->AddItem(Res, Cell->AvailableResource);
    }
    FGridCell* Cell = GetCell(X, Y);
    UE_LOG(LogHerbalist, Log, TEXT("Harvested from (%d,%d): Mag=%.2f Dist=%.2f Stress=%.3f Resource=%d"),
        X, Y, Res.Magnitude, Res.Meta.Distortion, Cell ? Cell->HarvestStress : -1.0f, Cell ? (int32)Cell->AvailableResource : -1);
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

    FRealState Ingredient1 = Inventory[0].State;
    FRealState Ingredient2 = Inventory[1].State;
    PC->InventoryComponent->RemoveItem(1);
    PC->InventoryComponent->RemoveItem(0);

    TArray<FRealState> Ingredients = { Ingredient1, Ingredient2 };
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
        UE_LOG(LogHerbalist, Log, TEXT("[%d] Mag: %.2f, Dist: %.2f, Pot:%.2f Res:%.2f Cor:%.2f, Dir: (%.2f,%.2f,%.2f,%.2f)"),
            i, Res.Magnitude, Res.Meta.Distortion, Res.Meta.Potency, Res.Meta.Resonance, Res.Meta.Corruption,
            Res.Direction.Body, Res.Direction.Mind, Res.Direction.Spirit, Res.Direction.Nature);
    }
}