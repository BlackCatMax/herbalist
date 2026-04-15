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
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = false;
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
            EBiomeType biome = EBiomeType::MixedForest; // можно изменить на разные биомы
            Cells[Index].Biome = biome;
            FRealState defaultState = FBiomeDefaults::GetDefaultState(biome);
            FEnvironment defaultEnv = FBiomeDefaults::GetDefaultEnvironment(biome);
            Cells[Index].State = defaultState;
            Cells[Index].TargetState = defaultState;
            Cells[Index].Environment = defaultEnv;
            Cells[Index].Memory = FMemoryState();
            Cells[Index].TargetMemory = FMemoryState();
            Cells[Index].X = X;
            Cells[Index].Y = Y;
            Cells[Index].HarvestStress = 0.0f;
            Cells[Index].bEntityTriggered = false;
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

void AGridWorldManager::SetTargetState(int32 X, int32 Y, const FRealState& NewState, const FMemoryState& NewMemory)
{
    FGridCell* Cell = GetCell(X, Y);
    if (Cell)
    {
        Cell->TargetState = NewState;
        Cell->TargetMemory = NewMemory;
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

    Cell.TargetState.Meta.Distortion = FMath::Clamp(Cell.TargetState.Meta.Distortion + DistortionIncrease, 0.0f, 1.0f);
    Cell.TargetState.Magnitude = FMath::Clamp(Cell.TargetState.Magnitude - MagnitudeDecrease, 0.0f, 1.0f);
    UpdateMemory(Cell.TargetMemory, Cell.TargetState, 0.1f);
}

FRealState AGridWorldManager::HarvestFromCell(int32 X, int32 Y, EResourceType ResourceType, const FConditionModifier& Conditions)
{
    FGridCell* Cell = GetCell(X, Y);
    if (!Cell) return FRealState();

    FRealState Resource = FHerbalistHarvest::Harvest(ResourceType, Cell->State, Conditions);

    if (bHarvestAffectsBiome)
    {
        Cell->HarvestStress += HarvestStressIncrement;
        Cell->HarvestStress = FMath::Clamp(Cell->HarvestStress, 0.0f, 1.0f);
        RecalculateDistortionFromHarvestStress(*Cell);
        if (!bInterpolationActive)
        {
            bInterpolationActive = true;
            SetActorTickEnabled(true);
        }
    }
    return Resource;
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

    FRealState Delta;
    Delta.Magnitude = NewState.Magnitude - OldState.Magnitude;
    Delta.Direction.Body = NewState.Direction.Body - OldState.Direction.Body;
    Delta.Direction.Mind = NewState.Direction.Mind - OldState.Direction.Mind;
    Delta.Direction.Spirit = NewState.Direction.Spirit - OldState.Direction.Spirit;
    Delta.Direction.Nature = NewState.Direction.Nature - OldState.Direction.Nature;
    Delta.Meta.Distortion = NewState.Meta.Distortion - OldState.Meta.Distortion;
    Delta.Meta.Stability = NewState.Meta.Stability - OldState.Meta.Stability;
    Delta.Meta.Purity = NewState.Meta.Purity - OldState.Meta.Purity;

    FMemoryState NewMemory = Cell->Memory;
    UpdateMemory(NewMemory, NewState, 0.1f);
    SetTargetState(X, Y, NewState, NewMemory);

    FMemoryState MemoryDelta;
    MemoryDelta.AccumulatedDistortion = NewMemory.AccumulatedDistortion - Cell->Memory.AccumulatedDistortion;
    MemoryDelta.StabilityMemory = NewMemory.StabilityMemory - Cell->Memory.StabilityMemory;
    MemoryDelta.HistoryPurity = NewMemory.HistoryPurity - Cell->Memory.HistoryPurity;

    PropagateToNeighbors(X, Y, Delta, MemoryDelta, 0.5f, 1);
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

            FRealState NewTargetState = Neighbor->TargetState;
            NewTargetState.Direction.Body += WeakDelta.Direction.Body;
            NewTargetState.Direction.Mind += WeakDelta.Direction.Mind;
            NewTargetState.Direction.Spirit += WeakDelta.Direction.Spirit;
            NewTargetState.Direction.Nature += WeakDelta.Direction.Nature;
            NewTargetState.Magnitude += WeakDelta.Magnitude;
            NewTargetState.Meta.Distortion += WeakDelta.Meta.Distortion;
            NewTargetState.Meta.Stability += WeakDelta.Meta.Stability;
            NewTargetState.Meta.Purity += WeakDelta.Meta.Purity;

            float Len = FMath::Sqrt(NewTargetState.Direction.Body * NewTargetState.Direction.Body +
                NewTargetState.Direction.Mind * NewTargetState.Direction.Mind +
                NewTargetState.Direction.Spirit * NewTargetState.Direction.Spirit +
                NewTargetState.Direction.Nature * NewTargetState.Direction.Nature);
            if (Len > KINDA_SMALL_NUMBER)
            {
                NewTargetState.Direction.Body /= Len;
                NewTargetState.Direction.Mind /= Len;
                NewTargetState.Direction.Spirit /= Len;
                NewTargetState.Direction.Nature /= Len;
            }
            else
            {
                NewTargetState.Direction.Body = NewTargetState.Direction.Mind = NewTargetState.Direction.Spirit = NewTargetState.Direction.Nature = 0.25f;
            }
            NewTargetState.Magnitude = FMath::Clamp(NewTargetState.Magnitude, 0.0f, 1.0f);
            NewTargetState.Meta.Distortion = FMath::Clamp(NewTargetState.Meta.Distortion, 0.0f, 1.0f);
            NewTargetState.Meta.Stability = FMath::Clamp(NewTargetState.Meta.Stability, 0.0f, 1.0f);
            NewTargetState.Meta.Purity = FMath::Clamp(NewTargetState.Meta.Purity, 0.0f, 1.0f);

            FMemoryState NewTargetMemory = Neighbor->TargetMemory;
            NewTargetMemory.AccumulatedDistortion += WeakMemoryDelta.AccumulatedDistortion;
            NewTargetMemory.StabilityMemory += WeakMemoryDelta.StabilityMemory;
            NewTargetMemory.HistoryPurity += WeakMemoryDelta.HistoryPurity;
            NewTargetMemory.AccumulatedDistortion = FMath::Clamp(NewTargetMemory.AccumulatedDistortion, 0.0f, 1.0f);
            NewTargetMemory.StabilityMemory = FMath::Clamp(NewTargetMemory.StabilityMemory, 0.0f, 1.0f);
            NewTargetMemory.HistoryPurity = FMath::Clamp(NewTargetMemory.HistoryPurity, 0.0f, 1.0f);

            Neighbor->TargetState = NewTargetState;
            Neighbor->TargetMemory = NewTargetMemory;

            if (!bInterpolationActive)
            {
                bInterpolationActive = true;
                SetActorTickEnabled(true);
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

    float Len = FMath::Sqrt(Cur.Direction.Body * Cur.Direction.Body + Cur.Direction.Mind * Cur.Direction.Mind +
        Cur.Direction.Spirit * Cur.Direction.Spirit + Cur.Direction.Nature * Cur.Direction.Nature);
    if (Len > KINDA_SMALL_NUMBER)
    {
        Cur.Direction.Body /= Len;
        Cur.Direction.Mind /= Len;
        Cur.Direction.Spirit /= Len;
        Cur.Direction.Nature /= Len;
    }
    else
    {
        Cur.Direction.Body = Cur.Direction.Mind = Cur.Direction.Spirit = Cur.Direction.Nature = 0.25f;
    }

    FMemoryState& Mem = Cell.Memory;
    const FMemoryState& TargetMem = Cell.TargetMemory;
    Mem.AccumulatedDistortion = FMath::FInterpTo(Mem.AccumulatedDistortion, TargetMem.AccumulatedDistortion, DeltaTime, StateInterpolationSpeed);
    Mem.StabilityMemory = FMath::FInterpTo(Mem.StabilityMemory, TargetMem.StabilityMemory, DeltaTime, StateInterpolationSpeed);
    Mem.HistoryPurity = FMath::FInterpTo(Mem.HistoryPurity, TargetMem.HistoryPurity, DeltaTime, StateInterpolationSpeed);
}

void AGridWorldManager::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (bEnableRecovery)
    {
        for (FGridCell& Cell : Cells)
        {
            if (Cell.HarvestStress > 0.0f)
            {
                float OldStress = Cell.HarvestStress;
                Cell.HarvestStress = FMath::Max(0.0f, Cell.HarvestStress - HarvestStressDecayRate * DeltaTime);
                if (OldStress != Cell.HarvestStress)
                {
                    RecalculateDistortionFromHarvestStress(Cell);
                    if (!bInterpolationActive)
                    {
                        bInterpolationActive = true;
                        SetActorTickEnabled(true);
                    }
                }
            }
        }
    }

    bool bAnyRemaining = false;
    for (FGridCell& Cell : Cells)
    {
        bool bStateNear = FMath::IsNearlyEqual(Cell.State.Magnitude, Cell.TargetState.Magnitude, 0.001f) &&
            FMath::IsNearlyEqual(Cell.State.Meta.Distortion, Cell.TargetState.Meta.Distortion, 0.001f) &&
            FMath::IsNearlyEqual(Cell.Memory.AccumulatedDistortion, Cell.TargetMemory.AccumulatedDistortion, 0.001f);
        if (!bStateNear)
        {
            InterpolateCell(Cell, DeltaTime);
            bAnyRemaining = true;
        }
        else
        {
            Cell.State = Cell.TargetState;
            Cell.Memory = Cell.TargetMemory;
        }
    }

    for (int32 i = 0; i < Cells.Num(); ++i)
    {
        UpdateCellColor(Cells[i].X, Cells[i].Y);
    }

    if (!bAnyRemaining)
    {
        bInterpolationActive = false;
        SetActorTickEnabled(false);
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
    return FString::Printf(TEXT("Cell (%d,%d): Mag=%.2f, Dist=%.2f, Stress=%.3f"),
        SelectedX, SelectedY, Cell->State.Magnitude, Cell->State.Meta.Distortion, Cell->HarvestStress);
}

void AGridWorldManager::UI_HarvestSelectedCell(int32 ResourceType)
{
    if (SelectedX < 0 || SelectedY < 0)
    {
        UE_LOG(LogHerbalist, Warning, TEXT("No cell selected for harvest"));
        return;
    }
    HarvestTest(SelectedX, SelectedY, ResourceType);
}

void AGridWorldManager::UI_ApplyAlchemyToSelectedCell()
{
    if (SelectedX < 0 || SelectedY < 0)
    {
        UE_LOG(LogHerbalist, Warning, TEXT("No cell selected for alchemy"));
        return;
    }
    ApplyTest(SelectedX, SelectedY);
}

// ========== Тестовые команды ==========
void AGridWorldManager::HarvestTest(int32 X, int32 Y, int32 ResourceType)
{
    EResourceType Type = static_cast<EResourceType>(ResourceType);
    FRealState Res = HarvestFromCellSimple(X, Y, Type);
    AProjectHerbalistGameModeBase* GM = Cast<AProjectHerbalistGameModeBase>(GetWorld()->GetAuthGameMode());
    if (GM) GM->AddToInventory(Res);
    FGridCell* Cell = GetCell(X, Y);
    UE_LOG(LogHerbalist, Log, TEXT("Harvested from (%d,%d): Mag=%.2f Dist=%.2f Stress=%.3f"),
        X, Y, Res.Magnitude, Res.Meta.Distortion, Cell ? Cell->HarvestStress : -1.0f);
}

void AGridWorldManager::MassHarvestTest(int32 X, int32 Y, int32 ResourceType, int32 Count)
{
    for (int32 i = 0; i < Count; ++i) HarvestTest(X, Y, ResourceType);
    UE_LOG(LogHerbalist, Log, TEXT("Mass harvest %d times at (%d,%d)"), Count, X, Y);
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
        else UE_LOG(LogHerbalist, Warning, TEXT("[%d] NULL pointer"), i);
    }
}