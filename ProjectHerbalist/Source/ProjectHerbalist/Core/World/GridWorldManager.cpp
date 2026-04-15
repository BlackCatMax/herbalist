#include "GridWorldManager.h"
#include "ProjectHerbalist.h"
#include "Core/Pipeline/HerbalistPipeline.h"
#include "ProjectHerbalistGameModeBase.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

AGridWorldManager::AGridWorldManager()
{
    PrimaryActorTick.bCanEverTick = false;
    static ConstructorHelpers::FObjectFinder<UStaticMesh> MeshFinder(TEXT("/Engine/BasicShapes/Cube"));
    if (MeshFinder.Succeeded()) CubeMesh = MeshFinder.Object;
    static ConstructorHelpers::FObjectFinder<UMaterial> MatFinder(TEXT("/Game/Materials/M_CubeColor"));
    if (MatFinder.Succeeded()) CubeMaterial = MatFinder.Object;
}

void AGridWorldManager::BeginPlay()
{
    Super::BeginPlay();
    InitializeCells();
    SpawnVisuals();
}

void AGridWorldManager::InitializeCells()
{
    Cells.SetNum(GridSizeX * GridSizeY);
    for (int32 Y = 0; Y < GridSizeY; Y++)
    {
        for (int32 X = 0; X < GridSizeX; X++)
        {
            int32 Index = Y * GridSizeX + X;
            EBiomeType biome = EBiomeType::MixedForest;
            Cells[Index].Biome = biome;
            Cells[Index].State = FBiomeDefaults::GetDefaultState(biome);
            Cells[Index].Environment = FBiomeDefaults::GetDefaultEnvironment(biome);
            Cells[Index].Memory = FMemoryState();
            Cells[Index].X = X;
            Cells[Index].Y = Y;
        }
    }
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

void AGridWorldManager::UpdateCellState(int32 X, int32 Y, const FRealState& NewState, const FMemoryState& NewMemory)
{
    FGridCell* Cell = GetCell(X, Y);
    if (Cell)
    {
        Cell->State = NewState;
        Cell->Memory = NewMemory;
        UpdateCellColor(X, Y);
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

FRealState AGridWorldManager::HarvestFromCell(int32 X, int32 Y, EResourceType ResourceType, const FConditionModifier& Conditions)
{
    FGridCell* Cell = GetCell(X, Y);
    if (!Cell) return FRealState();
    return FHerbalistHarvest::Harvest(ResourceType, Cell->State, Conditions);
}

FRealState AGridWorldManager::HarvestFromCellSimple(int32 X, int32 Y, EResourceType ResourceType)
{
    return HarvestFromCell(X, Y, ResourceType, FConditionModifier());
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

    // Дельта состояния
    FRealState Delta;
    Delta.Magnitude = NewState.Magnitude - OldState.Magnitude;
    Delta.Direction.Body = NewState.Direction.Body - OldState.Direction.Body;
    Delta.Direction.Mind = NewState.Direction.Mind - OldState.Direction.Mind;
    Delta.Direction.Spirit = NewState.Direction.Spirit - OldState.Direction.Spirit;
    Delta.Direction.Nature = NewState.Direction.Nature - OldState.Direction.Nature;
    Delta.Meta.Distortion = NewState.Meta.Distortion - OldState.Meta.Distortion;
    Delta.Meta.Stability = NewState.Meta.Stability - OldState.Meta.Stability;
    Delta.Meta.Purity = NewState.Meta.Purity - OldState.Meta.Purity;

    // Сохраняем старую память, создаём новую и обновляем
    FMemoryState OldMemory = Cell->Memory;
    FMemoryState NewMemory = OldMemory;
    UpdateMemory(NewMemory, NewState, 0.1f);

    // Обновляем ячейку
    UpdateCellState(X, Y, NewState, NewMemory);

    // Дельта памяти (приращение)
    FMemoryState MemoryDelta;
    MemoryDelta.AccumulatedDistortion = NewMemory.AccumulatedDistortion - OldMemory.AccumulatedDistortion;
    MemoryDelta.StabilityMemory = NewMemory.StabilityMemory - OldMemory.StabilityMemory;
    MemoryDelta.HistoryPurity = NewMemory.HistoryPurity - OldMemory.HistoryPurity;

    UE_LOG(LogHerbalist, Warning, TEXT("MemoryDelta=%.3f"), MemoryDelta.AccumulatedDistortion);

    // Распространение на соседей
    PropagateToNeighbors(X, Y, Delta, MemoryDelta, 0.3f, 3);
}

void AGridWorldManager::PropagateToNeighbors(int32 X, int32 Y, const FRealState& Delta, const FMemoryState& MemoryDelta, float Falloff, int32 Depth)
{
    if (Depth <= 0) return;
    for (int32 dx = -1; dx <= 1; dx++)
    {
        for (int32 dy = -1; dy <= 1; dy++)
        {
            if (dx == 0 && dy == 0) continue;
            FGridCell* Neighbor = GetCell(X + dx, Y + dy);
            if (!Neighbor) continue;

            // Ослабленные дельты
            FRealState WeakDelta = Delta;
            WeakDelta.Magnitude *= Falloff;
            WeakDelta.Direction.Body *= Falloff;
            WeakDelta.Direction.Mind *= Falloff;
            WeakDelta.Direction.Spirit *= Falloff;
            WeakDelta.Direction.Nature *= Falloff;
            WeakDelta.Meta.Distortion *= Falloff;
            WeakDelta.Meta.Stability *= Falloff;
            WeakDelta.Meta.Purity *= Falloff;

            FMemoryState WeakMemoryDelta = MemoryDelta;
            WeakMemoryDelta.AccumulatedDistortion *= Falloff;
            WeakMemoryDelta.StabilityMemory *= Falloff;
            WeakMemoryDelta.HistoryPurity *= Falloff;

            UE_LOG(LogHerbalist, Warning, TEXT("Propagate to (%d,%d): WeakMemoryDelta=%.3f"), X + dx, Y + dy, WeakMemoryDelta.AccumulatedDistortion);

            // Новое состояние соседа
            FRealState NewNeighborState = Neighbor->State;
            NewNeighborState.Direction.Body += WeakDelta.Direction.Body;
            NewNeighborState.Direction.Mind += WeakDelta.Direction.Mind;
            NewNeighborState.Direction.Spirit += WeakDelta.Direction.Spirit;
            NewNeighborState.Direction.Nature += WeakDelta.Direction.Nature;
            NewNeighborState.Magnitude += WeakDelta.Magnitude;
            NewNeighborState.Meta.Distortion += WeakDelta.Meta.Distortion;
            NewNeighborState.Meta.Stability += WeakDelta.Meta.Stability;
            NewNeighborState.Meta.Purity += WeakDelta.Meta.Purity;

            // Нормализация направления
            float Len = FMath::Sqrt(NewNeighborState.Direction.Body * NewNeighborState.Direction.Body +
                NewNeighborState.Direction.Mind * NewNeighborState.Direction.Mind +
                NewNeighborState.Direction.Spirit * NewNeighborState.Direction.Spirit +
                NewNeighborState.Direction.Nature * NewNeighborState.Direction.Nature);
            if (Len > KINDA_SMALL_NUMBER)
            {
                NewNeighborState.Direction.Body /= Len;
                NewNeighborState.Direction.Mind /= Len;
                NewNeighborState.Direction.Spirit /= Len;
                NewNeighborState.Direction.Nature /= Len;
            }
            else
            {
                NewNeighborState.Direction.Body = NewNeighborState.Direction.Mind = NewNeighborState.Direction.Spirit = NewNeighborState.Direction.Nature = 0.25f;
            }
            NewNeighborState.Magnitude = FMath::Clamp(NewNeighborState.Magnitude, 0.0f, 1.0f);
            NewNeighborState.Meta.Distortion = FMath::Clamp(NewNeighborState.Meta.Distortion, 0.0f, 1.0f);
            NewNeighborState.Meta.Stability = FMath::Clamp(NewNeighborState.Meta.Stability, 0.0f, 1.0f);
            NewNeighborState.Meta.Purity = FMath::Clamp(NewNeighborState.Meta.Purity, 0.0f, 1.0f);

            // Обновляем память соседа
            FMemoryState NewNeighborMemory = Neighbor->Memory;
            NewNeighborMemory.AccumulatedDistortion += WeakMemoryDelta.AccumulatedDistortion;
            NewNeighborMemory.StabilityMemory += WeakMemoryDelta.StabilityMemory;
            NewNeighborMemory.HistoryPurity += WeakMemoryDelta.HistoryPurity;
            NewNeighborMemory.AccumulatedDistortion = FMath::Clamp(NewNeighborMemory.AccumulatedDistortion, 0.0f, 1.0f);
            NewNeighborMemory.StabilityMemory = FMath::Clamp(NewNeighborMemory.StabilityMemory, 0.0f, 1.0f);
            NewNeighborMemory.HistoryPurity = FMath::Clamp(NewNeighborMemory.HistoryPurity, 0.0f, 1.0f);

            Neighbor->State = NewNeighborState;
            Neighbor->Memory = NewNeighborMemory;
            UpdateCellColor(Neighbor->X, Neighbor->Y);

            UE_LOG(LogHerbalist, Warning, TEXT("Neighbor (%d,%d) new Memory=%.3f"), X + dx, Y + dy, NewNeighborMemory.AccumulatedDistortion);
        }
    }
}

void AGridWorldManager::SpawnVisuals()
{
    if (!CubeMesh) return;
    VisualMeshes.SetNum(GridSizeX * GridSizeY);
    for (int32 Y = 0; Y < GridSizeY; Y++)
    {
        for (int32 X = 0; X < GridSizeX; X++)
        {
            FVector Location = FVector(X * CellSize, Y * CellSize, 0.0f);
            UStaticMeshComponent* MeshComp = NewObject<UStaticMeshComponent>(this);
            MeshComp->SetStaticMesh(CubeMesh);
            MeshComp->SetWorldLocation(Location);
            MeshComp->SetWorldScale3D(FVector(CellSize / 100.0f, CellSize / 100.0f, 0.2f));
            MeshComp->RegisterComponent();
            VisualMeshes[Y * GridSizeX + X] = MeshComp;

            if (CubeMaterial)
            {
                UMaterialInstanceDynamic* Mat = UMaterialInstanceDynamic::Create(CubeMaterial, this);
                MeshComp->SetMaterial(0, Mat);
                UpdateCellColor(X, Y);
            }
            else
            {
                UE_LOG(LogHerbalist, Warning, TEXT("CubeMaterial is null!"));
            }
        }
    }
}

void AGridWorldManager::UpdateCellColor(int32 X, int32 Y)
{
    int32 Index = Y * GridSizeX + X;
    if (!VisualMeshes.IsValidIndex(Index)) return;
    UStaticMeshComponent* Mesh = VisualMeshes[Index];
    if (!Mesh) return;

    UMaterialInstanceDynamic* Mat = Cast<UMaterialInstanceDynamic>(Mesh->GetMaterial(0));
    if (!Mat && CubeMaterial)
    {
        Mat = UMaterialInstanceDynamic::Create(CubeMaterial, this);
        Mesh->SetMaterial(0, Mat);
    }
    if (!Mat) return;

    const FGridCell* Cell = GetCellConst(X, Y);
    if (!Cell) return;

    float Dist = Cell->Memory.AccumulatedDistortion;
    FLinearColor Color = FLinearColor::LerpUsingHSV(FLinearColor::Green, FLinearColor::Red, Dist);
    Mat->SetVectorParameterValue("Color", Color);
    UE_LOG(LogHerbalist, Warning, TEXT("Cell (%d,%d) Dist=%.3f -> Color=(%.2f,%.2f,%.2f)"), X, Y, Dist, Color.R, Color.G, Color.B);
}

void AGridWorldManager::HarvestTest(int32 X, int32 Y, int32 ResourceType)
{
    EResourceType Type = static_cast<EResourceType>(ResourceType);
    FRealState Res = HarvestFromCellSimple(X, Y, Type);
    if (Res.Magnitude < 0.01f && Res.Meta.Distortion < 0.01f)
    {
        UE_LOG(LogHerbalist, Warning, TEXT("Harvested resource invalid (Mag=%.2f), skipping"), Res.Magnitude);
        return;
    }
    AProjectHerbalistGameModeBase* GM = Cast<AProjectHerbalistGameModeBase>(GetWorld()->GetAuthGameMode());
    if (GM) GM->AddToInventory(Res);
    UE_LOG(LogHerbalist, Log, TEXT("Harvested from (%d,%d): Mag=%.2f Dist=%.2f"), X, Y, Res.Magnitude, Res.Meta.Distortion);
}

void AGridWorldManager::ApplyTest(int32 X, int32 Y)
{
    AProjectHerbalistGameModeBase* GM = Cast<AProjectHerbalistGameModeBase>(GetWorld()->GetAuthGameMode());
    if (!GM) return;
    TArray<FRealState*> Inventory = GM->GetInventory();
    if (Inventory.Num() < 2)
    {
        UE_LOG(LogHerbalist, Warning, TEXT("Need at least 2 resources in inventory"));
        return;
    }
    TArray<FRealState> Ingredients;
    Ingredients.Add(*Inventory[0]);
    Ingredients.Add(*Inventory[1]);
    FIntent Intent; Intent.Coherence = 0.5f;
    FRngState Rng; Rng.Seed = 12345;
    ApplyAlchemyResult(X, Y, Ingredients, Intent, Rng);
    UE_LOG(LogHerbalist, Log, TEXT("Applied alchemy to (%d,%d)"), X, Y);
}

void AGridWorldManager::ShowInventory()
{
    AProjectHerbalistGameModeBase* GM = Cast<AProjectHerbalistGameModeBase>(GetWorld()->GetAuthGameMode());
    if (!GM) return;
    TArray<FRealState*> Inventory = GM->GetInventory();
    UE_LOG(LogHerbalist, Log, TEXT("=== INVENTORY (%d items) ==="), Inventory.Num());
    for (int32 i = 0; i < Inventory.Num(); ++i)
    {
        if (Inventory[i])
        {
            const FRealState& Res = *Inventory[i];
            UE_LOG(LogHerbalist, Log, TEXT("[%d] Mag: %.2f, Dist: %.2f, Dir: (%.2f,%.2f,%.2f,%.2f)"),
                i, Res.Magnitude, Res.Meta.Distortion,
                Res.Direction.Body, Res.Direction.Mind, Res.Direction.Spirit, Res.Direction.Nature);
        }
        else
        {
            UE_LOG(LogHerbalist, Warning, TEXT("[%d] NULL pointer"), i);
        }
    }
}