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

    const int32 CellCount = GridSizeX * GridSizeY;
    Cells.SetNum(CellCount);
    MorokField.Init(0.0f, CellCount);
    ZaryanaField.Init(0.0f, CellCount);

    for (int32 Y = 0; Y < GridSizeY; ++Y)
    {
        for (int32 X = 0; X < GridSizeX; ++X)
        {
            FGridCellLight& Cell = Cells[GetCellIndex(X, Y)];
            Cell.X = X;
            Cell.Y = Y;
        }
    }
}

const FGridCellLight& UWorldStateSubsystem::GetCell(int32 X, int32 Y) const
{
    static const FGridCellLight InvalidCell;
    const int32 Index = GetCellIndex(X, Y);
    return Cells.IsValidIndex(Index) ? Cells[Index] : InvalidCell;
}

const FGridCellFull* UWorldStateSubsystem::GetCellFull(int32 X, int32 Y) const
{
    return FullCells.Find(FIntPoint(X, Y));
}

float UWorldStateSubsystem::GetMorokField(int32 X, int32 Y) const
{
    const int32 Index = GetCellIndex(X, Y);
    return MorokField.IsValidIndex(Index) ? MorokField[Index] : 0.0f;
}

float UWorldStateSubsystem::GetZaryanaField(int32 X, int32 Y) const
{
    const int32 Index = GetCellIndex(X, Y);
    return ZaryanaField.IsValidIndex(Index) ? ZaryanaField[Index] : 0.0f;
}

bool UWorldStateSubsystem::ApplyDelta(const FWorldDelta& Delta, FValidationReport& OutReport)
{
    if (!ValidateDelta(Delta, OutReport))
    {
        return false;
    }

    ApplyWorldDelta(Delta);
    OutReport.bValid = true;
    OutReport.Error.Empty();
    return true;
}

bool UWorldStateSubsystem::ApplyInventoryDelta(const FInventoryDelta& Delta)
{
    ApplyInventoryDeltaInternal(Delta);
    return true;
}

void UWorldStateSubsystem::SaveSnapshot(FSimulationSnapshot& OutSnapshot) const
{
    OutSnapshot.GridSizeX = GridSizeX;
    OutSnapshot.GridSizeY = GridSizeY;
    OutSnapshot.CellSize = CellSize;
    OutSnapshot.Cells = Cells;
    OutSnapshot.MorokField = MorokField;
    OutSnapshot.ZaryanaField = ZaryanaField;
}

void UWorldStateSubsystem::LoadSnapshot(const FSimulationSnapshot& InSnapshot)
{
    GridSizeX = InSnapshot.GridSizeX;
    GridSizeY = InSnapshot.GridSizeY;
    CellSize = InSnapshot.CellSize;
    Cells = InSnapshot.Cells;
    MorokField = InSnapshot.MorokField;
    ZaryanaField = InSnapshot.ZaryanaField;
}

void UWorldStateSubsystem::ApplyWorldDelta(const FWorldDelta& Delta)
{
    for (const FGridCellLight& UpdatedCell : Delta.UpdatedCells)
    {
        const int32 Index = GetCellIndex(UpdatedCell.X, UpdatedCell.Y);
        if (Cells.IsValidIndex(Index))
        {
            Cells[Index] = UpdatedCell;
        }
    }
}

void UWorldStateSubsystem::ApplyInventoryDeltaInternal(const FInventoryDelta& Delta)
{
    (void)Delta;
}

bool UWorldStateSubsystem::ValidateDelta(const FWorldDelta& Delta, FValidationReport& OutReport) const
{
    for (const FGridCellLight& UpdatedCell : Delta.UpdatedCells)
    {
        if (!Cells.IsValidIndex(GetCellIndex(UpdatedCell.X, UpdatedCell.Y)))
        {
            OutReport.bValid = false;
            OutReport.Error = FString::Printf(TEXT("Invalid cell coordinates (%d, %d)"), UpdatedCell.X, UpdatedCell.Y);
            return false;
        }
    }

    return true;
}

int32 UWorldStateSubsystem::GetCellIndex(int32 X, int32 Y) const
{
    return Y * GridSizeX + X;
}
