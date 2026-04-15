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

    FRealState NewState = HerbalistCore::Pipeline::ApplyMorok(
        Ingredients,
        Cell->State,
        Cell->Environment,
        FMemoryState(),
        Intent,
        Rng
    );
    UpdateCellState(X, Y, NewState);
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
                UpdateCellColor(X, Y);   // цвет сразу по состоянию
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
    UE_LOG(LogHerbalist, Verbose, TEXT("Cell (%d,%d) Dist=%.2f Color=(%.2f,%.2f,%.2f)"), X, Y, Dist, Color.R, Color.G, Color.B);
}

void AGridWorldManager::HarvestTest(int32 X, int32 Y, int32 ResourceType)
{
    EResourceType Type = static_cast<EResourceType>(ResourceType);
    FRealState Res = HarvestFromCellSimple(X, Y, Type);

    UE_LOG(LogHerbalist, Log, TEXT("HarvestTest: Res before add: Mag=%.2f, Dist=%.2f"), Res.Magnitude, Res.Meta.Distortion);

    if (Res.Magnitude < 0.01f && Res.Meta.Distortion < 0.01f)
    {
        UE_LOG(LogHerbalist, Warning, TEXT("Harvested resource seems invalid (Mag=%.2f), skipping"), Res.Magnitude);
        return;
    }

    AProjectHerbalistGameModeBase* GM = Cast<AProjectHerbalistGameModeBase>(GetWorld()->GetAuthGameMode());
    if (GM)
    {
        GM->AddToInventory(Res);
    }
    else
    {
        UE_LOG(LogHerbalist, Error, TEXT("GameMode is null or wrong type"));
    }
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
    Ingredients.Add(*Inventory[0]);  // разыменовываем указатель
    Ingredients.Add(*Inventory[1]);
    FIntent Intent;
    Intent.Coherence = 0.5f;
    FRngState Rng;
    Rng.Seed = 12345;
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
        FRealState* Res = Inventory[i];
        if (Res)
        {
            UE_LOG(LogHerbalist, Log, TEXT("[%d] Mag: %.2f, Dist: %.2f, Dir: (%.2f,%.2f,%.2f,%.2f)"),
                i, Res->Magnitude, Res->Meta.Distortion,
                Res->Direction.Body, Res->Direction.Mind, Res->Direction.Spirit, Res->Direction.Nature);
        }
        else
        {
            UE_LOG(LogHerbalist, Warning, TEXT("[%d] NULL pointer"), i);
        }
    }
}