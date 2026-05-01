#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Core/Types/HerbalistCoreTypes.h"
#include "WorldStateSubsystem.generated.h"

USTRUCT(BlueprintType)
struct PROJECTHERBALIST_API FGridCellLight
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 X = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 Y = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EBiomeType Biome = EBiomeType::MixedForest;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bIsWater = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float HarvestStress = 0.0f;
};

USTRUCT(BlueprintType)
struct PROJECTHERBALIST_API FGridCellFull
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FGridCell Cell;
};

USTRUCT(BlueprintType)
struct PROJECTHERBALIST_API FValidationReport
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bValid = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString Error;
};

USTRUCT(BlueprintType)
struct PROJECTHERBALIST_API FWorldDelta
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FGridCellLight> UpdatedCells;
};

USTRUCT(BlueprintType)
struct PROJECTHERBALIST_API FInventoryDelta
{
    GENERATED_BODY()
};

USTRUCT(BlueprintType)
struct PROJECTHERBALIST_API FSimulationSnapshot
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 GridSizeX = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 GridSizeY = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float CellSize = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FGridCellLight> Cells;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<float> MorokField;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<float> ZaryanaField;
};

UCLASS()
class PROJECTHERBALIST_API UWorldStateSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    void InitializeGrid(int32 InSizeX, int32 InSizeY, float InCellSize);

    const FGridCellLight& GetCell(int32 X, int32 Y) const;
    const FGridCellFull* GetCellFull(int32 X, int32 Y) const;
    float GetMorokField(int32 X, int32 Y) const;
    float GetZaryanaField(int32 X, int32 Y) const;

    bool ApplyDelta(const FWorldDelta& Delta, FValidationReport& OutReport);
    bool ApplyInventoryDelta(const FInventoryDelta& Delta);

    void SaveSnapshot(FSimulationSnapshot& OutSnapshot) const;
    void LoadSnapshot(const FSimulationSnapshot& InSnapshot);

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

    void ApplyWorldDelta(const FWorldDelta& Delta);
    void ApplyInventoryDeltaInternal(const FInventoryDelta& Delta);
    bool ValidateDelta(const FWorldDelta& Delta, FValidationReport& OutReport) const;

    int32 GetCellIndex(int32 X, int32 Y) const;
};
