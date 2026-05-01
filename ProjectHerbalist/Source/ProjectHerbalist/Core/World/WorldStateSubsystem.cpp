#include "Core/World/WorldStateSubsystem.h"

void UWorldStateSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    DeterministicRng.Initialize(12345);
}

void UWorldStateSubsystem::Deinitialize()
{
    Cells.Reset();
    FullCells.Reset();
    MorokField.Reset();
    ZaryanaField.Reset();
    Super::Deinitialize();
}

void UWorldStateSubsystem::InitializeGrid(int32 InSizeX, int32 InSizeY, float InCellSize)
{
    GridSizeX = FMath::Max(0, InSizeX);
    GridSizeY = FMath::Max(0, InSizeY);
    CellSize = FMath::Max(0.0f, InCellSize);

    const int32 Total = GridSizeX * GridSizeY;
    Cells.SetNum(Total);
    MorokField.Init(0.0f, Total);
    ZaryanaField.Init(0.0f, Total);
    FullCells.Reset();
}

bool UWorldStateSubsystem::IsCellAccessible(int32 X, int32 Y) const
{
    if (!IsValidCoord(X, Y)) return false;
    return Cells[GetCellIndex(X, Y)].bAccessible != 0;
}

uint16 UWorldStateSubsystem::GetCellResource(int32 X, int32 Y) const
{
    if (!IsValidCoord(X, Y)) return 0;
    return Cells[GetCellIndex(X, Y)].ResourceID;
}

uint8 UWorldStateSubsystem::GetGrowthStage(int32 X, int32 Y) const
{
    if (!IsValidCoord(X, Y)) return 0;
    return Cells[GetCellIndex(X, Y)].GrowthStage;
}

const FRealState* UWorldStateSubsystem::GetCellState(int32 X, int32 Y) const
{
    if (const FGridCellFull* Full = FullCells.Find(FIntPoint(X, Y)))
    {
        return &Full->State;
    }
    return nullptr;
}

float UWorldStateSubsystem::GetMorokField(int32 X, int32 Y) const
{
    if (!IsValidCoord(X, Y)) return 0.0f;
    return MorokField[GetCellIndex(X, Y)];
}

float UWorldStateSubsystem::GetZaryanaField(int32 X, int32 Y) const
{
    if (!IsValidCoord(X, Y)) return 0.0f;
    return ZaryanaField[GetCellIndex(X, Y)];
}

EBiomeType UWorldStateSubsystem::GetBiomeAtCell(int32 X, int32 Y) const
{
    if (!IsValidCoord(X, Y) || !BiomeMaskTexture)
    {
        return EBiomeType::MixedForest;
    }
    return EBiomeType::MixedForest;
}

bool UWorldStateSubsystem::ApplyWorldDelta(const FWorldDelta& Delta, FString& OutError)
{
    if (Delta.AffectedCells.Num() != Delta.NewCellData.Num())
    {
        OutError = TEXT("AffectedCells/NewCellData size mismatch");
        return false;
    }

    for (int32 i = 0; i < Delta.AffectedCells.Num(); ++i)
    {
        const FIntPoint& Cell = Delta.AffectedCells[i];
        const FGridCellFull& NewData = Delta.NewCellData[i];
        if (!IsValidCoord(Cell.X, Cell.Y))
        {
            OutError = FString::Printf(TEXT("Invalid cell (%d,%d)"), Cell.X, Cell.Y);
            return false;
        }

        if (NewData.ResourceID == 0)
        {
            ClearCellResource(Cell.X, Cell.Y);
            continue;
        }

        FullCells.Add(Cell, NewData);
        FGridCellLight& Light = Cells[GetCellIndex(Cell.X, Cell.Y)];
        Light.ResourceID = NewData.ResourceID;
        Light.GrowthStage = NewData.GrowthStage;
        Light.bAccessible = NewData.bAccessible;
    }

    OutError.Empty();
    return true;
}

bool UWorldStateSubsystem::ApplyInventoryDelta(const FInventoryDelta& Delta)
{
    (void)Delta;
    return true;
}

void UWorldStateSubsystem::SaveSnapshot(FSimulationSnapshot& Out) const
{
    Out.GridSizeX = GridSizeX;
    Out.GridSizeY = GridSizeY;
    Out.CellSize = CellSize;
    Out.Cells = Cells;
    Out.FullCells = FullCells;
    Out.MorokField = MorokField;
    Out.ZaryanaField = ZaryanaField;
    Out.RngState = DeterministicRng;
}

void UWorldStateSubsystem::LoadSnapshot(const FSimulationSnapshot& In)
{
    GridSizeX = In.GridSizeX;
    GridSizeY = In.GridSizeY;
    CellSize = In.CellSize;
    Cells = In.Cells;
    FullCells = In.FullCells;
    MorokField = In.MorokField;
    ZaryanaField = In.ZaryanaField;
    DeterministicRng = In.RngState;
}

void UWorldStateSubsystem::SetMorokField(int32 X, int32 Y, float Value)
{
    if (IsValidCoord(X, Y)) MorokField[GetCellIndex(X, Y)] = Value;
}

void UWorldStateSubsystem::SetZaryanaField(int32 X, int32 Y, float Value)
{
    if (IsValidCoord(X, Y)) ZaryanaField[GetCellIndex(X, Y)] = Value;
}

void UWorldStateSubsystem::SetCellResource(int32 X, int32 Y, uint16 ResourceID, uint8 GrowthStage, const FRealState& InitialState)
{
    if (!IsValidCoord(X, Y)) return;

    FGridCellLight& Light = Cells[GetCellIndex(X, Y)];
    Light.ResourceID = ResourceID;
    Light.GrowthStage = GrowthStage;
    Light.bAccessible = 1;

    FGridCellFull Full;
    Full.ResourceID = ResourceID;
    Full.GrowthStage = GrowthStage;
    Full.bAccessible = 1;
    Full.State = InitialState;
    FullCells.Add(FIntPoint(X, Y), Full);
}

void UWorldStateSubsystem::ClearCellResource(int32 X, int32 Y)
{
    if (!IsValidCoord(X, Y)) return;
    FGridCellLight& Light = Cells[GetCellIndex(X, Y)];
    Light.ResourceID = 0;
    Light.GrowthStage = 0;
    FullCells.Remove(FIntPoint(X, Y));
}
