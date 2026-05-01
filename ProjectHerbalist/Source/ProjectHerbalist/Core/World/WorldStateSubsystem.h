#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Math/RandomStream.h"
#include "Core/Types/HerbalistCoreTypes.h"
#include "WorldStateSubsystem.generated.h"

USTRUCT(BlueprintType)
struct PROJECTHERBALIST_API FGridCellLight
{
    GENERATED_BODY()

    UPROPERTY()
    uint16 ResourceID = 0;

    UPROPERTY()
    uint8 GrowthStage = 0;

    UPROPERTY()
    uint8 bAccessible = 0;
};

USTRUCT()
struct PROJECTHERBALIST_API FGridCellFull
{
    GENERATED_BODY()

    UPROPERTY()
    FRealState State;

    UPROPERTY()
    uint16 ResourceID = 0;

    UPROPERTY()
    uint8 GrowthStage = 0;

    UPROPERTY()
    uint8 bAccessible = 0;
};

USTRUCT()
struct PROJECTHERBALIST_API FWorldDelta
{
    GENERATED_BODY()

    UPROPERTY()
    TArray<FIntPoint> AffectedCells;

    UPROPERTY()
    TArray<FGridCellFull> NewCellData;
};

USTRUCT()
struct PROJECTHERBALIST_API FInventoryDelta
{
    GENERATED_BODY()
};

USTRUCT()
struct PROJECTHERBALIST_API FSimulationSnapshot
{
    GENERATED_BODY()

    UPROPERTY()
    int32 GridSizeX = 0;

    UPROPERTY()
    int32 GridSizeY = 0;

    UPROPERTY()
    float CellSize = 0.0f;

    UPROPERTY()
    TArray<FGridCellLight> Cells;

    UPROPERTY()
    TMap<FIntPoint, FGridCellFull> FullCells;

    UPROPERTY()
    TArray<float> MorokField;

    UPROPERTY()
    TArray<float> ZaryanaField;

    UPROPERTY()
    FRandomStream RngState;
};

UCLASS()
class PROJECTHERBALIST_API UWorldStateSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()
public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    void InitializeGrid(int32 InSizeX, int32 InSizeY, float InCellSize);

    bool IsCellAccessible(int32 X, int32 Y) const;
    uint16 GetCellResource(int32 X, int32 Y) const;
    uint8 GetGrowthStage(int32 X, int32 Y) const;
    const FRealState* GetCellState(int32 X, int32 Y) const;
    float GetMorokField(int32 X, int32 Y) const;
    float GetZaryanaField(int32 X, int32 Y) const;
    EBiomeType GetBiomeAtCell(int32 X, int32 Y) const;

    bool ApplyWorldDelta(const FWorldDelta& Delta, FString& OutError);
    bool ApplyInventoryDelta(const FInventoryDelta& Delta);

    void SaveSnapshot(FSimulationSnapshot& Out) const;
    void LoadSnapshot(const FSimulationSnapshot& In);

    void SetMorokField(int32 X, int32 Y, float Value);
    void SetZaryanaField(int32 X, int32 Y, float Value);
    const TArray<float>& GetMorokFieldArray() const { return MorokField; }
    const TArray<float>& GetZaryanaFieldArray() const { return ZaryanaField; }

    void SetCellResource(int32 X, int32 Y, uint16 ResourceID, uint8 GrowthStage, const FRealState& InitialState);
    void ClearCellResource(int32 X, int32 Y);

    void SetBiomeMaskTexture(UTexture2D* InBiomeMaskTexture) { BiomeMaskTexture = InBiomeMaskTexture; }

private:
    int32 GridSizeX = 0;
    int32 GridSizeY = 0;
    float CellSize = 0.0f;
    TArray<FGridCellLight> Cells;
    TMap<FIntPoint, FGridCellFull> FullCells;
    TArray<float> MorokField;
    TArray<float> ZaryanaField;

    UPROPERTY(Transient)
    TObjectPtr<UTexture2D> BiomeMaskTexture = nullptr;

    FRandomStream DeterministicRng;

    int32 GetCellIndex(int32 X, int32 Y) const { return Y * GridSizeX + X; }
    bool IsValidCoord(int32 X, int32 Y) const { return X >= 0 && X < GridSizeX && Y >= 0 && Y < GridSizeY; }
};
