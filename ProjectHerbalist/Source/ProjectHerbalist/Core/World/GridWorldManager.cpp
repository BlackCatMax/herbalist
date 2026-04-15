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
            EBiomeType biome;
            int32 pattern = (X + Y) % 4;
            if (pattern == 0) biome = EBiomeType::MixedForest;
            else if (pattern == 1) biome = EBiomeType::Swamp;
            else if (pattern == 2) biome = EBiomeType::Steppe;
            else biome = EBiomeType::Floodplain;

            Cells[Index].Biome = biome;
            Cells[Index].State = FBiomeDefaults::GetDefaultState(biome);
            Cells[Index].Environment = FBiomeDefaults::GetDefaultEnvironment(biome);
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

void AGridWorldManager::UpdateCellState(int32 X, int32 Y, const FRealState& NewState)
{
    FGridCell* Cell = GetCell(X, Y);
    if (Cell)
    {
        Cell->State = NewState;
        UpdateCellColor(X, Y);
    }
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
        FMemoryState(),
        Intent,
        Rng
    );

    FRealState Delta;
    Delta.Magnitude = NewState.Magnitude - OldState.Magnitude;
    Delta.Direction.Body = NewState.Direction.Body - OldState.Direction.Body;
    Delta.Direction.Mind = NewState.Direction.Mind - OldState.Direction.Mind;
    Delta.Direction.Spirit = NewState.Direction.Spirit - OldState.Direction.Spirit;
    Delta.Direction.Nature = NewState.Direction.Nature - OldState.Direction.Nature;
    Delta.Meta.Distortion = NewState.Meta.Distortion - OldState.Meta.Distortion;
    Delta.Meta.Stability = NewState.Meta.Stability - OldState.Meta.Stability;
    Delta.Meta.Purity = NewState.Meta.Purity - OldState.Meta.Purity;

    UpdateCellState(X, Y, NewState);

    // Распространение на соседей (без рекурсии, чтобы избежать возврата)
    TArray<TPair<FGridCell*, FRealState>> PendingUpdates;
    for (int32 dx = -1; dx <= 1; dx++)
    {
        for (int32 dy = -1; dy <= 1; dy++)
        {
            if (dx == 0 && dy == 0) continue;
            FGridCell* Neighbor = GetCell(X + dx, Y + dy);
            if (!Neighbor) continue;

            FRealState WeakDelta = Delta;
            WeakDelta.Magnitude *= 0.5f;
            WeakDelta.Direction.Body *= 0.5f;
            WeakDelta.Direction.Mind *= 0.5f;
            WeakDelta.Direction.Spirit *= 0.5f;
            WeakDelta.Direction.Nature *= 0.5f;
            WeakDelta.Meta.Distortion *= 0.5f;
            WeakDelta.Meta.Stability *= 0.5f;
            WeakDelta.Meta.Purity *= 0.5f;

            PendingUpdates.Add(TPair<FGridCell*, FRealState>(Neighbor, WeakDelta));
        }
    }

    for (auto& Update : PendingUpdates)
    {
        FGridCell* Neighbor = Update.Key;
        FRealState WeakDelta = Update.Value;
        FRealState NewNeighborState = Neighbor->State;
        NewNeighborState.Direction.Body += WeakDelta.Direction.Body;
        NewNeighborState.Direction.Mind += WeakDelta.Direction.Mind;
        NewNeighborState.Direction.Spirit += WeakDelta.Direction.Spirit;
        NewNeighborState.Direction.Nature += WeakDelta.Direction.Nature;
        NewNeighborState.Magnitude += WeakDelta.Magnitude;
        NewNeighborState.Meta.Distortion += WeakDelta.Meta.Distortion;
        NewNeighborState.Meta.Stability += WeakDelta.Meta.Stability;
        NewNeighborState.Meta.Purity += WeakDelta.Meta.Purity;

        float Len = FMath::Sqrt(NewNeighborState.Direction.Body * NewNeighborState.Direction.Body + NewNeighborState.Direction.Mind * NewNeighborState.Direction.Mind + NewNeighborState.Direction.Spirit * NewNeighborState.Direction.Spirit + NewNeighborState.Direction.Nature * NewNeighborState.Direction.Nature);
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

        Neighbor->State = NewNeighborState;
        UpdateCellColor(Neighbor->X, Neighbor->Y);
    }
}

void AGridWorldManager::PropagateToNeighbors(int32 X, int32 Y, const FRealState& Delta, float Falloff, int32 Depth)
{
    if (Depth <= 0) return;
    for (int32 dx = -1; dx <= 1; dx++)
    {
        for (int32 dy = -1; dy <= 1; dy++)
        {
            if (dx == 0 && dy == 0) continue;
            int32 NX = X + dx;
            int32 NY = Y + dy;
            FGridCell* Neighbor = GetCell(NX, NY);
            if (!Neighbor) continue;

            FRealState WeakDelta = Delta;
            WeakDelta.Magnitude *= Falloff;
            WeakDelta.Direction.Body *= Falloff;
            WeakDelta.Direction.Mind *= Falloff;
            WeakDelta.Direction.Spirit *= Falloff;
            WeakDelta.Direction.Nature *= Falloff;
            WeakDelta.Meta.Distortion *= Falloff;
            WeakDelta.Meta.Stability *= Falloff;
            WeakDelta.Meta.Purity *= Falloff;

            FRealState NewState = Neighbor->State;
            NewState.Direction.Body += WeakDelta.Direction.Body;
            NewState.Direction.Mind += WeakDelta.Direction.Mind;
            NewState.Direction.Spirit += WeakDelta.Direction.Spirit;
            NewState.Direction.Nature += WeakDelta.Direction.Nature;
            NewState.Magnitude += WeakDelta.Magnitude;
            NewState.Meta.Distortion += WeakDelta.Meta.Distortion;
            NewState.Meta.Stability += WeakDelta.Meta.Stability;
            NewState.Meta.Purity += WeakDelta.Meta.Purity;

            float Len = FMath::Sqrt(NewState.Direction.Body * NewState.Direction.Body + NewState.Direction.Mind * NewState.Direction.Mind + NewState.Direction.Spirit * NewState.Direction.Spirit + NewState.Direction.Nature * NewState.Direction.Nature);
            if (Len > KINDA_SMALL_NUMBER)
            {
                NewState.Direction.Body /= Len;
                NewState.Direction.Mind /= Len;
                NewState.Direction.Spirit /= Len;
                NewState.Direction.Nature /= Len;
            }
            else
            {
                NewState.Direction.Body = NewState.Direction.Mind = NewState.Direction.Spirit = NewState.Direction.Nature = 0.25f;
            }
            NewState.Magnitude = FMath::Clamp(NewState.Magnitude, 0.0f, 1.0f);
            NewState.Meta.Distortion = FMath::Clamp(NewState.Meta.Distortion, 0.0f, 1.0f);
            NewState.Meta.Stability = FMath::Clamp(NewState.Meta.Stability, 0.0f, 1.0f);
            NewState.Meta.Purity = FMath::Clamp(NewState.Meta.Purity, 0.0f, 1.0f);

            Neighbor->State = NewState;
            UpdateCellColor(NX, NY);
            PropagateToNeighbors(NX, NY, WeakDelta, Falloff, Depth - 1);
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

    float Dist = Cell->State.Meta.Distortion;
    FLinearColor Color = FLinearColor::LerpUsingHSV(FLinearColor::Green, FLinearColor::Red, Dist);
    Mat->SetVectorParameterValue("Color", Color);
    UE_LOG(LogHerbalist, Warning, TEXT("Cell (%d,%d) Dist=%.3f Color=(%.2f,%.2f,%.2f)"), X, Y, Dist, Color.R, Color.G, Color.B);
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