// Core/World/GridWorldManagerCore.cpp
#include "Core/World/GridWorldManager.h"
#include "Core/BiomeGraph/BiomeGraphSubsystem.h"
#include "Core/Subsystems/WaterTypeRegistrySubsystem.h"
#include "Core/Subsystems/IngredientRegistrySubsystem.h"
#include "Core/Harvest/HarvestService.h"
#include "Core/Pipeline/AlchemyWorldStateApplier.h"
#include "Core/Types/BiomeTypes.h"
#include "Core/Resources/AHerbalistResourceActor.h"
#include "Player/HerbalistPlayerController.h"
#include "Core/Inventory/HerbalistInventoryComponent.h"
#include "Engine/World.h"
#include "DrawDebugHelpers.h"
#include "ProjectHerbalist.h"
#include "TimerManager.h"

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
        MarkDirty(Cell.X, Cell.Y);
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

    if (Cells.Num() == 0)
    {
        InitializeCells();
    }
}

// -----------------------------------------------------------------------------
// НОВЫЕ ФУНКЦИИ
// -----------------------------------------------------------------------------

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
        FVector SpawnPos = this->GetCellWorldPosition(Cell.X, Cell.Y) + Offset;

        const FIngredientTableRow* Row = IngredientSubsystem ? IngredientSubsystem->GetRow(IngredientID) : nullptr;
        if (!Row) continue;

        AHerbalistResourceActor* NewActor = GetWorld()->SpawnActor<AHerbalistResourceActor>(AHerbalistResourceActor::StaticClass(), SpawnPos, FRotator::ZeroRotator);
        if (NewActor)
        {
            NewActor->Init(IngredientID, Row->DisplayName, Row->ResourceMesh, Row->BaseState, SpawnPos, this, Cell.X, Cell.Y);
            Cell.ResourceActors.Add(NewActor);
            UE_LOG(LogHerbalist, Verbose, TEXT("Spawned %s at cell (%d,%d)"), *IngredientID.ToString(), Cell.X, Cell.Y);
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

    FVector SpawnPos = GetCellWorldPosition(X, Y) + Offset;

    AHerbalistResourceActor* NewActor = GetWorld()->SpawnActor<AHerbalistResourceActor>(AHerbalistResourceActor::StaticClass(), SpawnPos, FRotator::ZeroRotator);
    if (NewActor)
    {
        NewActor->Init(IngredientID, Row->DisplayName, Row->ResourceMesh, Row->BaseState, SpawnPos, this, X, Y);
        Cell->ResourceActors.Add(NewActor);
        UE_LOG(LogHerbalist, Verbose, TEXT("SpawnResourceActor: %s at cell (%d,%d)"), *IngredientID.ToString(), X, Y);
    }
}

void AGridWorldManager::StartRegeneration(FGridCell& Cell)
{
    FTimerHandle TimerHandle;
    GetWorldTimerManager().SetTimer(TimerHandle, [this, &Cell]()
        {
            if (Cell.bIsWater) return;
            SpawnResourcesInCell(Cell);
        }, ResourceRegrowthTime, false);
}

// -----------------------------------------------------------------------------
// ИНИЦИАЛИЗАЦИЯ МИРА
// -----------------------------------------------------------------------------

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
            int32 BlockX = X / BlockSize;
            int32 BlockY = Y / BlockSize;
            int32 BlockIndex = (BlockY * BlocksX + BlockX) % AllBiomes.Num();
            EBiomeType biome = AllBiomes[BlockIndex];

            FRealState defaultState = FBiomeDefaults::GetDefaultState(biome);
            FEnvironment defaultEnv = FBiomeDefaults::GetDefaultEnvironment(biome);
            FGridCell& Cell = Cells[Index];
            Cell.Biome = biome;
            Cell.State = defaultState;
            Cell.TargetState = defaultState;
            Cell.Environment = defaultEnv;
            Cell.Memory = FMemoryState();
            Cell.X = X;
            Cell.Y = Y;
            Cell.HarvestStress = 0.0f;
            Cell.bEntityTriggered = false;
            Cell.bIsWater = false;
            Cell.WaterTypeID = NAME_None;
        }
    }

    // Размещение воды (затирает ресурсы в водных клетках)
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
                Cell.WaterTypeID = WaterSubsystem ? WaterSubsystem->GetRandomWaterType(Cell.Biome, WorldRNG) : NAME_None;
                FRealState waterState = FBiomeDefaults::GetDefaultWaterState(Cell.Biome);
                if (WaterSubsystem)
                {
                    if (const FWaterTypeRow* WaterRow = WaterSubsystem->GetWaterType(Cell.WaterTypeID))
                    {
                        waterState.Meta.Purity = WaterRow->BasePurity;
                        waterState.Meta.Distortion = WaterRow->BaseDistortion;
                        waterState.Meta.Stability = WaterRow->BaseStability;
                        waterState.Meta.Potency = WaterRow->BasePotency;
                        waterState.Meta.Corruption = WaterRow->BaseCorruption;
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

    // Спавним ресурсы во всех не-водных клетках
    for (FGridCell& Cell : Cells)
    {
        if (!Cell.bIsWater)
        {
            SpawnResourcesInCell(Cell);
        }
    }

    SetActorTickEnabled(true);
}

void AGridWorldManager::OnResourceCollected(AHerbalistResourceActor* Actor)
{
    if (!Actor) return;

    FGridCell* Cell = GetCell(Actor->GetGridX(), Actor->GetGridY());
    if (!Cell) return;

    // Удаляем актор из списка клетки
    Cell->ResourceActors.Remove(Actor);
    UE_LOG(LogHerbalist, Log, TEXT("Resource collected at cell (%d,%d), remaining: %d"), Cell->X, Cell->Y, Cell->ResourceActors.Num());

    // Добавляем предмет в инвентарь (сам сбор)
    FName IngredientID = Actor->GetIngredientID();
    FRealState ResourceState = HarvestService->Harvest(IngredientID, Cell->State, FConditionModifier());
    AHerbalistPlayerController* PC = Cast<AHerbalistPlayerController>(GetWorld()->GetFirstPlayerController());
    if (PC && PC->InventoryComponent && (ResourceState.Magnitude > 0.01f || ResourceState.Meta.Distortion > 0.01f))
    {
        FInventoryItem Item;
        Item.IngredientID = IngredientID;
        Item.State = ResourceState;
        Item.Count = 1;
        Item.CreationTime = GetWorld()->GetTimeSeconds();
        Item.bSubjectToDecay = true;
        PC->InventoryComponent->AddItem(Item, 1);
    }

    // Если ресурсов больше нет, запускаем регенерацию
    if (Cell->ResourceActors.Num() == 0 && !Cell->bIsWater)
    {
        StartRegeneration(*Cell);
    }

    // Обновляем стресс клетки
    if (bHarvestAffectsBiome)
    {
        Cell->HarvestStress += HarvestStressIncrement;
        Cell->HarvestStress = FMath::Clamp(Cell->HarvestStress, 0.0f, 1.0f);
        MarkStress(Cell->X, Cell->Y);
        RecalculateDistortionFromHarvestStress(*Cell);
    }
}

FRealState AGridWorldManager::CollectWater(int32 X, int32 Y)
{
    FGridCell* Cell = GetCell(X, Y);
    if (!Cell || !Cell->bIsWater || !HarvestService) return FRealState();

    FRealState WaterState = HarvestService->HarvestWater(*Cell, FConditionModifier());

    if (bHarvestAffectsBiome)
    {
        Cell->HarvestStress += HarvestStressIncrement;
        Cell->HarvestStress = FMath::Clamp(Cell->HarvestStress, 0.0f, 1.0f);
        MarkStress(X, Y);
        RecalculateDistortionFromHarvestStress(*Cell);
    }
    return WaterState;
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
    Memory.StabilityMemory = FMath::FInterpTo(Memory.StabilityMemory, NewState.Meta.Stability, 0.05f, Rate);
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

// ---------------------- ОТРИСОВКА ----------------------
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