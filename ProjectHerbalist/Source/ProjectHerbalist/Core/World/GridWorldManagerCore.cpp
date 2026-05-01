// Core/World/GridWorldManagerCore.cpp
#include "Core/World/GridWorldManager.h"
#include "Landscape.h"
#include "LandscapeInfo.h"
#include "EngineUtils.h"
#include "Core/BiomeGraph/BiomeGraphSubsystem.h"
#include "Core/Subsystems/WaterTypeRegistrySubsystem.h"
#include "Core/Subsystems/IngredientRegistrySubsystem.h"
#include "Core/Harvest/HarvestService.h"
#include "Core/Types/BiomeTypes.h"
#include "Core/Resources/AHerbalistResourceActor.h"
#include "Player/HerbalistPlayerController.h"
#include "Engine/World.h"
#include "DrawDebugHelpers.h"
#include "ProjectHerbalist.h"
#include "TimerManager.h"
#include "Core/Simulation/Public/SnapshotTypes.h"
#include "Core/Simulation/Public/DeltaTypes.h"
#include "Core/Simulation/Public/CommandTypes.h"
#include "Core/Simulation/Public/PerceptionComponent.h"

// ============================================================================
// ВСПОМОГАТЕЛЬНЫЕ МЕТОДЫ (ЛАНДШАФТ)
// ============================================================================

void AGridWorldManager::FindAndCacheLandscape()
{
    if (CachedLandscape) return;
    UWorld* World = GetWorld();
    if (!World) return;

    for (TActorIterator<ALandscape> It(World); It; ++It)
    {
        CachedLandscape = *It;
        break;
    }
    if (!CachedLandscape)
    {
        UE_LOG(LogHerbalist, Warning, TEXT("No Landscape found in level. Grid cells will use flat Z."));
    }
    else
    {
        UE_LOG(LogHerbalist, Log, TEXT("Landscape found: %s"), *CachedLandscape->GetName());
    }
}

void AGridWorldManager::CacheCellHeights()
{
    FindAndCacheLandscape();
    const int32 TotalCells = GridSizeX * GridSizeY;
    CachedCellHeights.SetNum(TotalCells);

    if (!CachedLandscape)
    {
        for (int32 i = 0; i < TotalCells; ++i) CachedCellHeights[i] = 0.f;
        return;
    }

    FVector GridOrigin = GetActorLocation();
    for (int32 Y = 0; Y < GridSizeY; ++Y)
    {
        for (int32 X = 0; X < GridSizeX; ++X)
        {
            FVector WorldPoint(GridOrigin.X + X * CellSize, GridOrigin.Y + Y * CellSize, 0.f);
            TOptional<float> OptHeight = CachedLandscape->GetHeightAtLocation(WorldPoint);
            float Z = OptHeight.IsSet() ? OptHeight.GetValue() : 0.f;
            int32 Idx = Y * GridSizeX + X;
            CachedCellHeights[Idx] = Z;
        }
    }
    UE_LOG(LogHerbalist, Log, TEXT("Cached %d cell heights from landscape"), TotalCells);
}

float AGridWorldManager::GetCellHeight(int32 X, int32 Y) const
{
    int32 Idx = Y * GridSizeX + X;
    if (CachedCellHeights.IsValidIndex(Idx))
        return CachedCellHeights[Idx];
    return 0.f;
}

FVector AGridWorldManager::GetCellWorldPositionFlat(int32 X, int32 Y) const
{
    return GetActorLocation() + FVector(X * CellSize, Y * CellSize, 0.f);
}

FVector AGridWorldManager::GetCellWorldPosition(int32 X, int32 Y) const
{
    FVector Flat = GetCellWorldPositionFlat(X, Y);
    float Z = GetCellHeight(X, Y);
    return FVector(Flat.X, Flat.Y, Z);
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

// ============================================================================
// БИОМЫ
// ============================================================================

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
        FVector Pos = GetCellWorldPositionFlat(Cell.X, Cell.Y);
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

void AGridWorldManager::ApplyBiomeInfluences(const TMap<FName, float>& MorokFields,
                                             const TMap<FName, float>& ZaryanaFields,
                                             float GlobalScale)
{
    for (FGridCell& Cell : Cells)
    {
        FName BiomeID = FBiomeDefaults::BiomeTypeToName(Cell.Biome);
        
        const float* MorokField = MorokFields.Find(BiomeID);
        if (MorokField)
        {
            float MorokInfluence = *MorokField * 0.1f * GlobalScale;
            Cell.TargetState.Meta.Distortion = FMath::Clamp(Cell.TargetState.Meta.Distortion + MorokInfluence, 0.f, 1.f);
        }
        
        const float* ZaryanaField = ZaryanaFields.Find(BiomeID);
        if (ZaryanaField)
        {
            float ZaryanaInfluence = *ZaryanaField * 0.05f * GlobalScale;
            Cell.TargetState.Meta.Stability = FMath::Clamp(Cell.TargetState.Meta.Stability + ZaryanaInfluence, 0.f, 1.f);
            Cell.TargetState.Meta.Purity    = FMath::Clamp(Cell.TargetState.Meta.Purity    + ZaryanaInfluence * 0.5f, 0.f, 1.f);
        }
    }
}

// ============================================================================
// ЖИЗНЕННЫЙ ЦИКЛ
// ============================================================================

AGridWorldManager::AGridWorldManager()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = false;
    PerceptionComponent = CreateDefaultSubobject<UPerceptionComponent>(TEXT("PerceptionComp"));
}

void AGridWorldManager::BeginPlay()
{
    Super::BeginPlay();
    WorldRNG.Initialize(12345);
    HarvestService = NewObject<UHarvestService>(this);

    if (Cells.Num() == 0)
    {
        InitializeCells();
    }
}

// ============================================================================
// ИНИЦИАЛИЗАЦИЯ МИРА
// ============================================================================

void AGridWorldManager::InitializeCells()
{
    const int32 TotalCells = GridSizeX * GridSizeY;
    Cells.SetNum(TotalCells);

    UGameInstance* GameInstance = GetGameInstance();
    UIngredientRegistrySubsystem* IngredientSubsystem = GameInstance ? GameInstance->GetSubsystem<UIngredientRegistrySubsystem>() : nullptr;
    UWaterTypeRegistrySubsystem* WaterSubsystem = GameInstance ? GameInstance->GetSubsystem<UWaterTypeRegistrySubsystem>() : nullptr;

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
            EBiomeType biome = AllBiomes[( (Y / BlockSize) * BlocksX + (X / BlockSize) ) % AllBiomes.Num()];

            FGridCell& Cell = Cells[Index];
            Cell.Biome        = biome;
            Cell.State        = FBiomeDefaults::GetDefaultState(biome);
            Cell.TargetState  = Cell.State;
            Cell.Environment  = FBiomeDefaults::GetDefaultEnvironment(biome);
            Cell.Memory       = FMemoryState();
            Cell.X            = X;
            Cell.Y            = Y;
            Cell.HarvestStress = 0.0f;
            Cell.bEntityTriggered = false;
            Cell.bIsWater     = false;
            Cell.WaterTypeID  = NAME_None;
        }
    }

    int32 TargetWaterCount = FMath::Max(TotalCells / 5, 1);
    TArray<bool> IsWaterAlready;
    IsWaterAlready.Init(false, TotalCells);
    int32 PlacedWater = 0;

    while (PlacedWater < TargetWaterCount)
    {
        int32 W = (WorldRNG.FRand() < 0.5f) ? 1 : 2;
        int32 H = (WorldRNG.FRand() < 0.5f) ? 1 : 2;
        int32 StartX = WorldRNG.RandRange(0, GridSizeX - W);
        int32 StartY = WorldRNG.RandRange(0, GridSizeY - H);

        bool bAreaFree = true;
        for (int32 dy = 0; dy < H && bAreaFree; ++dy)
            for (int32 dx = 0; dx < W; ++dx)
                if (IsWaterAlready[(StartY + dy) * GridSizeX + (StartX + dx)])
                    { bAreaFree = false; break; }

        if (!bAreaFree) continue;

        for (int32 dy = 0; dy < H; ++dy)
        {
            for (int32 dx = 0; dx < W; ++dx)
            {
                int32 Idx = (StartY + dy) * GridSizeX + (StartX + dx);
                FGridCell& Cell = Cells[Idx];
                Cell.bIsWater = true;
                Cell.WaterTypeID = WaterSubsystem ? WaterSubsystem->GetRandomWaterType(Cell.Biome, WorldRNG) : NAME_None;

                FRealState waterState = FBiomeDefaults::GetDefaultWaterState(Cell.Biome);
                if (WaterSubsystem)
                {
                    if (const FWaterTypeRow* WaterRow = WaterSubsystem->GetWaterType(Cell.WaterTypeID))
                    {
                        waterState.Meta.Purity      = WaterRow->BasePurity;
                        waterState.Meta.Distortion  = WaterRow->BaseDistortion;
                        waterState.Meta.Stability   = WaterRow->BaseStability;
                        waterState.Meta.Potency     = WaterRow->BasePotency;
                        waterState.Meta.Corruption  = WaterRow->BaseCorruption;
                    }
                }
                Cell.State = waterState;
                Cell.TargetState = waterState;
                Cell.HarvestStress = 0.0f;
                Cell.ResourceActors.Empty();
                IsWaterAlready[Idx] = true;
                PlacedWater++;
            }
        }
        if (PlacedWater >= TargetWaterCount) break;
    }

    CacheCellHeights();

    for (FGridCell& Cell : Cells)
    {
        if (!Cell.bIsWater)
        {
            SpawnResourcesInCell(Cell);
        }
    }

    SetActorTickEnabled(true);
}

// ============================================================================
// РЕСУРСЫ
// ============================================================================

void AGridWorldManager::SpawnResourcesInCell(FGridCell& Cell)
{
    UGameInstance* GameInstance = GetGameInstance();
    UIngredientRegistrySubsystem* IngredientSubsystem = GameInstance ? GameInstance->GetSubsystem<UIngredientRegistrySubsystem>() : nullptr;

    int32 NumResources = FMath::RandRange(1, 3);
    for (int32 i = 0; i < NumResources; ++i)
    {
        FName IngredientID = IngredientSubsystem ? IngredientSubsystem->GetRandomResourceForBiome(Cell.Biome, WorldRNG) : NAME_None;
        if (IngredientID.IsNone()) continue;

        FVector Offset = FVector(FMath::FRandRange(-CellSize * 0.3f, CellSize * 0.3f),
                                 FMath::FRandRange(-CellSize * 0.3f, CellSize * 0.3f),
                                 0);
        FVector SpawnPos = GetCellWorldPositionFlat(Cell.X, Cell.Y);
        SpawnPos.Z = GetCellHeight(Cell.X, Cell.Y) + 5.0f;
        SpawnPos += Offset;

        const FIngredientTableRow* Row = IngredientSubsystem ? IngredientSubsystem->GetRow(IngredientID) : nullptr;
        if (!Row) continue;

        AHerbalistResourceActor* NewActor = GetWorld()->SpawnActor<AHerbalistResourceActor>(AHerbalistResourceActor::StaticClass(), SpawnPos, FRotator::ZeroRotator);
        if (NewActor)
        {
            NewActor->Init(IngredientID, Row->DisplayName, Row->ResourceMesh, Row->BaseState, SpawnPos, this, Cell.X, Cell.Y);
            Cell.ResourceActors.Add(NewActor);
            UE_LOG(LogHerbalist, Verbose, TEXT("Spawned %s at cell (%d,%d) with Z=%.1f"), *IngredientID.ToString(), Cell.X, Cell.Y, SpawnPos.Z);
        }
    }
}

void AGridWorldManager::SpawnResourceActor(FName IngredientID, int32 X, int32 Y, const FVector& Offset)
{
    FGridCell* Cell = GetCell(X, Y);
    if (!Cell) return;

    UGameInstance* GameInstance = GetGameInstance();
    UIngredientRegistrySubsystem* IngredientSubsystem = GameInstance ? GameInstance->GetSubsystem<UIngredientRegistrySubsystem>() : nullptr;
    const FIngredientTableRow* Row = IngredientSubsystem ? IngredientSubsystem->GetRow(IngredientID) : nullptr;
    if (!Row) return;

    FVector SpawnPos = GetCellWorldPositionFlat(X, Y);
    SpawnPos.Z = GetCellHeight(X, Y) + 5.0f;
    SpawnPos += Offset;

    AHerbalistResourceActor* NewActor = GetWorld()->SpawnActor<AHerbalistResourceActor>(AHerbalistResourceActor::StaticClass(), SpawnPos, FRotator::ZeroRotator);
    if (NewActor)
    {
        NewActor->Init(IngredientID, Row->DisplayName, Row->ResourceMesh, Row->BaseState, SpawnPos, this, X, Y);
        Cell->ResourceActors.Add(NewActor);
        UE_LOG(LogHerbalist, Verbose, TEXT("SpawnResourceActor: %s at cell (%d,%d) Z=%.1f"), *IngredientID.ToString(), X, Y, SpawnPos.Z);
    }
}

void AGridWorldManager::StartRegeneration(FGridCell& Cell)
{
    FTimerHandle TimerHandle;
    GetWorldTimerManager().SetTimer(TimerHandle, [this, &Cell]()
    {
        if (!Cell.bIsWater)
        {
            SpawnResourcesInCell(Cell);
        }
    }, ResourceRegrowthTime, false);
}

void AGridWorldManager::OnResourceCollected(AHerbalistResourceActor* Actor)
{
    if (!Actor) return;

    FGridCell* Cell = GetCell(Actor->GetGridX(), Actor->GetGridY());
    if (!Cell) return;

    Cell->ResourceActors.Remove(Actor);

    AHerbalistPlayerController* PC = Cast<AHerbalistPlayerController>(GetWorld()->GetFirstPlayerController());
    if (PC && Cell)
    {
        PC->CurrentGlobalDistortion = Cell->Memory.AccumulatedDistortion;
    }

    FCommandEntry Cmd;
    Cmd.Primitive             = ECommandPrimitive::Harvest;
    Cmd.Harvest.TargetCell    = FIntPoint(Cell->X, Cell->Y);
    Cmd.Harvest.IngredientID  = Actor->GetIngredientID();
    Cmd.Harvest.Amount        = 1;
    QueueCommand(Cmd);

    if (Cell->ResourceActors.Num() == 0 && !Cell->bIsWater)
    {
        StartRegeneration(*Cell);
    }
}

FRealState AGridWorldManager::CollectWater(int32 X, int32 Y)
{
    FGridCell* Cell = GetCell(X, Y);
    if (!Cell || !Cell->bIsWater || !HarvestService) return FRealState();

    FCommandEntry Cmd;
    Cmd.Primitive             = ECommandPrimitive::Harvest;
    Cmd.Harvest.TargetCell    = FIntPoint(X, Y);
    Cmd.Harvest.IngredientID  = Cell->WaterTypeID;
    Cmd.Harvest.Amount        = 1;
    QueueCommand(Cmd);

    return FRealState();
}

// ============================================================================
// SNAPSHOT / DELTA
// ============================================================================

FWorldSnapshot AGridWorldManager::CaptureState() const
{
    FWorldSnapshot Snapshot;
    for (const FGridCell& Cell : Cells)
    {
        Snapshot.GridState.Add(FIntPoint(Cell.X, Cell.Y), Cell);
    }
    Snapshot.WorldSeed = WorldRNG.GetCurrentSeed();
    return Snapshot;
}

void AGridWorldManager::ApplyStateDelta(const FStateDelta& Delta)
{
    // 1. Применяем все изменения из дельты к миру
    for (const auto& Pair : Delta.WorldChanges)
    {
        const FIntPoint& Coord = Pair.Key;
        const FGridCell& NewCellData = Pair.Value;

        FGridCell* Cell = GetCell(Coord.X, Coord.Y);
        if (Cell)
        {
            Cell->State       = NewCellData.State;
            Cell->TargetState = NewCellData.State;
            Cell->Biome       = NewCellData.Biome;
            Cell->bIsWater    = NewCellData.bIsWater;
            Cell->WaterTypeID = NewCellData.WaterTypeID;
            Cell->HarvestStress = NewCellData.HarvestStress;
            Cell->Memory        = NewCellData.Memory;
        }
    }

    // 2. Если это эффект зелья, запускаем распространение на соседние клетки
	if (Delta.bIsPotionEffect && Delta.WorldChanges.Num() > 0)
	{
		const auto& FirstPair = *Delta.WorldChanges.CreateConstIterator();
		const FIntPoint& Center = FirstPair.Key;
		const FGridCell& NewCellData = FirstPair.Value;

		FStateDelta PropagationDelta;
		PropagatePotionEffect(Center, NewCellData.State, PropagationDepth, PropagationDelta);
		UE_LOG(LogHerbalist, Warning, TEXT("PropagationDelta has %d changes"), PropagationDelta.WorldChanges.Num());

		for (const auto& Pair : PropagationDelta.WorldChanges)
		{
			FGridCell* Cell = GetCell(Pair.Key.X, Pair.Key.Y);
			if (Cell)
			{
				UE_LOG(LogHerbalist, Warning, TEXT("ApplyStateDelta: updating cell (%d,%d) Dist %.3f -> %.3f"),
					Pair.Key.X, Pair.Key.Y, Cell->State.Meta.Distortion, Pair.Value.State.Meta.Distortion);
				*Cell = Pair.Value;
			}
		}
	}
}

// ============================================================================
// ПРОПАГАЦИЯ ЭФФЕКТА ЗЕЛЬЯ НА СОСЕДНИЕ КЛЕТКИ
// ============================================================================

void AGridWorldManager::PropagatePotionEffect(const FIntPoint& Center, const FRealState& PotionState, int32 Depth, FStateDelta& OutDelta)
{
    UE_LOG(LogHerbalist, Warning, TEXT("PropagatePotionEffect: center=(%d,%d), Depth=%d"), Center.X, Center.Y, Depth);
    if (Depth <= 0) return;

    struct FQueueItem { FIntPoint Coord; float Strength; };
    TQueue<FQueueItem> Queue;
    TSet<FIntPoint> Visited;

    Queue.Enqueue({Center, 1.0f});
    Visited.Add(Center);

    while (!Queue.IsEmpty())
    {
        FQueueItem Item;
        Queue.Dequeue(Item);
        if (Item.Strength < 0.05f) continue;

        // ЛОГ 1: текущая обрабатываемая клетка
        UE_LOG(LogHerbalist, Warning, TEXT("Propagation: processing cell (%d,%d), strength=%.2f"), Item.Coord.X, Item.Coord.Y, Item.Strength);

        if (Item.Coord != Center)
        {
            FGridCell* Cell = GetCell(Item.Coord.X, Item.Coord.Y);
            if (Cell)
            {
                // ЛОГ 2: состояние до изменения
                UE_LOG(LogHerbalist, Warning, TEXT("Cell (%d,%d) before: Dist=%.3f, Purity=%.3f"), 
                    Item.Coord.X, Item.Coord.Y, Cell->State.Meta.Distortion, Cell->State.Meta.Purity);

                FGridCell Modified = *Cell;
                Modified.State.Magnitude = FMath::Lerp(Cell->State.Magnitude, PotionState.Magnitude, Item.Strength);
                Modified.State.Meta.Distortion = FMath::Lerp(Cell->State.Meta.Distortion, PotionState.Meta.Distortion, Item.Strength);
                // ... (остальные параметры)

                OutDelta.WorldChanges.Add(Item.Coord, Modified);

                // ЛОГ 3: состояние после (берём из Modified)
                UE_LOG(LogHerbalist, Warning, TEXT("Cell (%d,%d) after : Dist=%.3f, Purity=%.3f (strength=%.2f)"),
                    Item.Coord.X, Item.Coord.Y, Modified.State.Meta.Distortion, Modified.State.Meta.Purity, Item.Strength);
            }
        }

        int32 CurrentDist = FMath::Abs(Item.Coord.X - Center.X) + FMath::Abs(Item.Coord.Y - Center.Y);
        if (CurrentDist >= Depth) continue;

        FIntPoint Neighbors[4] = {
            {Item.Coord.X + 1, Item.Coord.Y},
            {Item.Coord.X - 1, Item.Coord.Y},
            {Item.Coord.X, Item.Coord.Y + 1},
            {Item.Coord.X, Item.Coord.Y - 1}
        };
        float NextStrength = Item.Strength * 0.5f;
        for (const FIntPoint& Neighbor : Neighbors)
        {
            if (Neighbor.X >= 0 && Neighbor.X < GridSizeX && Neighbor.Y >= 0 && Neighbor.Y < GridSizeY && !Visited.Contains(Neighbor))
            {
                Visited.Add(Neighbor);
                Queue.Enqueue({Neighbor, NextStrength});
                // ЛОГ 4: добавляем соседа в очередь
                UE_LOG(LogHerbalist, Warning, TEXT("Propagation: adding neighbor (%d,%d) with strength=%.2f"), Neighbor.X, Neighbor.Y, NextStrength);
            }
        }
    }
}

// ============================================================================
// COMMAND ALGEBRA
// ============================================================================

void AGridWorldManager::QueueCommand(const FCommandEntry& Cmd)
{
    PendingCommands.Add(Cmd);
}

// ============================================================================
// ВОСПРИЯТИЕ
// ============================================================================

const FPerceivedWorld* AGridWorldManager::GetPerceivedWorld() const
{
    return PerceptionComponent ? &PerceptionComponent->GetPerceivedWorld() : nullptr;
}

// ============================================================================
// ОТЛАДОЧНАЯ ОТРИСОВКА
// ============================================================================

#if WITH_EDITOR
void AGridWorldManager::DrawGridDebug()
{
    if (!bEnableDebugDraw) return;
    for (const FGridCell& Cell : Cells)
    {
        FVector Center = GetCellWorldPosition(Cell.X, Cell.Y);
        FVector Extent = FVector(CellSize / 2.0f, CellSize / 2.0f, CellHeight / 2.0f);
        FColor Color;
        if (Cell.bIsWater)
        {
            Color = FColor::White;
        }
        else
        {
            float Distortion = Cell.State.Meta.Distortion;
            Color = FLinearColor::LerpUsingHSV(FLinearColor::Green, FLinearColor::Red, Distortion).ToFColor(false);
        }
        DrawDebugBox(GetWorld(), Center, Extent, Color, false, 0.0f, 0, BorderThickness);
    }
}
#endif

// ============================================================================
// ЭКОЛОГИЯ: ВОССТАНОВЛЕНИЕ ПАРАМЕТРОВ КЛЕТОК
// ============================================================================

void AGridWorldManager::RegenerateCellParameters(float DeltaTime)
{
    const float RegenerationRate = 0.0005f;
    const float DeltaRegen = RegenerationRate * DeltaTime;

    for (FGridCell& Cell : Cells)
    {
        // Distortion
        if (Cell.State.Meta.Distortion > Cell.TargetState.Meta.Distortion)
            Cell.State.Meta.Distortion = FMath::Max(Cell.State.Meta.Distortion - DeltaRegen, Cell.TargetState.Meta.Distortion);
        else if (Cell.State.Meta.Distortion < Cell.TargetState.Meta.Distortion)
            Cell.State.Meta.Distortion = FMath::Min(Cell.State.Meta.Distortion + DeltaRegen, Cell.TargetState.Meta.Distortion);
        
        // Purity
        if (Cell.State.Meta.Purity < Cell.TargetState.Meta.Purity)
            Cell.State.Meta.Purity = FMath::Min(Cell.State.Meta.Purity + DeltaRegen, Cell.TargetState.Meta.Purity);
        else if (Cell.State.Meta.Purity > Cell.TargetState.Meta.Purity)
            Cell.State.Meta.Purity = FMath::Max(Cell.State.Meta.Purity - DeltaRegen, Cell.TargetState.Meta.Purity);
        
        // Stability
        if (Cell.State.Meta.Stability < Cell.TargetState.Meta.Stability)
            Cell.State.Meta.Stability = FMath::Min(Cell.State.Meta.Stability + DeltaRegen, Cell.TargetState.Meta.Stability);
        else if (Cell.State.Meta.Stability > Cell.TargetState.Meta.Stability)
            Cell.State.Meta.Stability = FMath::Max(Cell.State.Meta.Stability - DeltaRegen, Cell.TargetState.Meta.Stability);
        
        // Magnitude
        if (Cell.State.Magnitude < Cell.TargetState.Magnitude)
            Cell.State.Magnitude = FMath::Min(Cell.State.Magnitude + DeltaRegen, Cell.TargetState.Magnitude);
        else if (Cell.State.Magnitude > Cell.TargetState.Magnitude)
            Cell.State.Magnitude = FMath::Max(Cell.State.Magnitude - DeltaRegen, Cell.TargetState.Magnitude);
        
        // Спад HarvestStress
        Cell.HarvestStress = FMath::Max(Cell.HarvestStress - DeltaRegen * 2.0f, 0.0f);
        
        // Восстановление осей к TargetState.Direction
        const FDirection& TargetDir = Cell.TargetState.Direction;
        Cell.State.Direction.Body   = FMath::Clamp(Cell.State.Direction.Body   + (TargetDir.Body   - Cell.State.Direction.Body) * 0.01f * DeltaTime, 0.0f, 1.0f);
        Cell.State.Direction.Mind   = FMath::Clamp(Cell.State.Direction.Mind   + (TargetDir.Mind   - Cell.State.Direction.Mind) * 0.01f * DeltaTime, 0.0f, 1.0f);
        Cell.State.Direction.Spirit = FMath::Clamp(Cell.State.Direction.Spirit + (TargetDir.Spirit - Cell.State.Direction.Spirit) * 0.01f * DeltaTime, 0.0f, 1.0f);
        Cell.State.Direction.Nature = FMath::Clamp(Cell.State.Direction.Nature + (TargetDir.Nature - Cell.State.Direction.Nature) * 0.01f * DeltaTime, 0.0f, 1.0f);
        Cell.State.Direction.NormalizeSum();
        
        // Синхронизация памяти
        Cell.Memory.AccumulatedDistortion = Cell.State.Meta.Distortion;
    }
}

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
            const FVector* ToPos   = Centers.Find(Edge.ToBiome);
            if (FromPos && ToPos)
            {
                DrawDebugLine(World, *FromPos, *ToPos, FColor::Yellow, false, 0.0f, 0, 2.0f);
            }
        }

        for (const auto& Pair : Nodes)
        {
            const FVector* Pos = Centers.Find(Pair.Key);
            if (Pos)
            {
                const FBiomeGraphNode& Node = Pair.Value;
                FColor Color = FLinearColor::LerpUsingHSV(FLinearColor::Green, FLinearColor::Red, Node.MorokField).ToFColor(false);
                DrawDebugSphere(World, *Pos, 30.0f, 12, Color, false, 0.0f, 0, 2.0f);
                DrawDebugString(World, *Pos + FVector(0, 0, 50.0f), Pair.Key.ToString(), nullptr, FColor::White, 0.0f, true, 1.2f);
            }
        }
    }

    if (bShowCellDistortion || bShowCellInfluence)
    {
        for (const FGridCell& Cell : Cells)
        {
            FVector Pos = GetCellWorldPositionFlat(Cell.X, Cell.Y);
            Pos.Z = GetCellHeight(Cell.X, Cell.Y) + 30.0f;
            if (bShowCellDistortion)
            {
                FColor Color = FLinearColor::LerpUsingHSV(FLinearColor::Green, FLinearColor::Red, Cell.State.Meta.Distortion).ToFColor(false);
                DrawDebugString(World, Pos, FString::Printf(TEXT("%.2f"), Cell.State.Meta.Distortion), nullptr, Color, 0.0f, true);
            }
            if (bShowCellInfluence)
            {
                float Influence = Cell.TargetState.Meta.Distortion - Cell.State.Meta.Distortion;
                FColor Color = Influence > 0 ? FColor::Red : (Influence < 0 ? FColor::Blue : FColor::White);
                DrawDebugString(World, Pos + FVector(0, 0, 30.0f), FString::Printf(TEXT("Δ%.2f"), Influence), nullptr, Color, 0.0f, true);
            }
        }
    }
#endif
}

bool AGridWorldManager::ApplyDelta(const FWorldDelta& Delta, FString& OutError)
{
    if (UWorld* World = GetWorld())
    {
        if (UWorldStateSubsystem* WorldState = World->GetSubsystem<UWorldStateSubsystem>())
        {
            return WorldState->ApplyWorldDelta(Delta, OutError);
        }
    }

    OutError = TEXT("WorldStateSubsystem is unavailable");
    return false;
}
