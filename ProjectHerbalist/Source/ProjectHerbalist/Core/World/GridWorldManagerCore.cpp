// GridWorldManagerCore.cpp
#include "GridWorldManager.h"
#include "ProjectHerbalist.h"
#include "Core/Types/BiomeTypes.h"
#include "Core/Harvest/HerbalistHarvest.h"
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

    TArray<EBiomeType> AllBiomes = FBiomeDefaults::GetAllBiomeTypes();
    if (AllBiomes.Num() == 0)
    {
        // fallback
        AllBiomes = {
            EBiomeType::Tundra, EBiomeType::ForestTundra, EBiomeType::NorthernTaiga,
            EBiomeType::MiddleTaiga, EBiomeType::SouthernTaiga, EBiomeType::MixedForest,
            EBiomeType::BroadleafForest, EBiomeType::ForestSteppe, EBiomeType::Steppe,
            EBiomeType::SemiDesert, EBiomeType::Floodplain, EBiomeType::RaisedBog,
            EBiomeType::LowlandBog
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
            Cells[Index].AvailableResource = FBiomeDefaults::GetRandomResourceForBiome(biome, WorldRNG, ESeasonMask::Summer, ETimeOfDayMask::Day);
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
    Cell.AvailableResource = FBiomeDefaults::GetRandomResourceForBiome(Cell.Biome, WorldRNG, ESeasonMask::Summer, ETimeOfDayMask::Day);
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
    Cell.TargetState.Meta.Distortion = FMath::Clamp(Cell.State.Meta.Distortion + DistortionIncrease, 0.0f, 1.0f);
    Cell.TargetState.Magnitude = FMath::Clamp(Cell.TargetState.Magnitude - MagnitudeDecrease, 0.0f, 1.0f);
    MarkDirty(Cell.X, Cell.Y);
}