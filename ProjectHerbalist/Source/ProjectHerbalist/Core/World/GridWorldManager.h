#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/Types/HerbalistCoreTypes.h"
#include "Core/Types/BiomeTypes.h"
#include "Core/Harvest/HerbalistHarvest.h"
#include "GridWorldManager.generated.h"

USTRUCT(BlueprintType)
struct FGridCell
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FRealState State;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FEnvironment Environment;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FMemoryState Memory;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) EBiomeType Biome;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 X = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Y = 0;

    FRealState TargetState;
    FMemoryState TargetMemory;

    UPROPERTY() float HarvestStress = 0.0f;
    UPROPERTY() bool bEntityTriggered = false;

    EResourceType AvailableResource = EResourceType::Nettle;
    float ResourceRegrowthTimer = 0.0f;

    FGridCell()
        : State(), Environment(), Memory(), Biome(), X(0), Y(0),
        TargetState(), TargetMemory(), HarvestStress(0.0f), bEntityTriggered(false),
        AvailableResource(EResourceType::Nettle), ResourceRegrowthTimer(0.0f) {
    }
};

UCLASS(BlueprintType)
class PROJECTHERBALIST_API AGridWorldManager : public AActor
{
    GENERATED_BODY()
public:
    AGridWorldManager();
    virtual void Tick(float DeltaTime) override;
    virtual void BeginPlay() override;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid") int32 GridSizeX = 25;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid") int32 GridSizeY = 25;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid") float CellSize = 100.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visuals")
    float StateInterpolationSpeed = 0.05f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Harvest")
    float ResourceRegrowthTime = 30.0f;

    bool bHarvestAffectsBiome = true;
    float HarvestStressIncrement = 0.001f;
    float HarvestStressThreshold = 0.3f;
    float MaxHarvestImpactOnDistortion = 0.2f;
    float MaxHarvestImpactOnMagnitude = 0.1f;
    float HarvestStressDecayRate = 0.0005f;
    bool bEnableRecovery = true;

    FGridCell* GetCell(int32 X, int32 Y);
    const FGridCell* GetCellConst(int32 X, int32 Y) const;
    void SetTargetState(int32 X, int32 Y, const FRealState& NewState, const FMemoryState& NewMemory);
    FRealState HarvestFromCell(int32 X, int32 Y, const FConditionModifier& Conditions = FConditionModifier());
    UFUNCTION(BlueprintCallable, Category = "Harvest") FRealState HarvestFromCellSimple(int32 X, int32 Y);
    UFUNCTION(BlueprintCallable, Category = "Alchemy") void ApplyAlchemyResult(int32 X, int32 Y, const TArray<FRealState>& Ingredients, const FIntent& Intent, FRngState& Rng);
    UFUNCTION(BlueprintCallable, Category = "Visuals") void SpawnVisuals();
    void UpdateCellColor(int32 X, int32 Y);
    void PropagateToNeighbors(int32 X, int32 Y, const FRealState& Delta, const FMemoryState& MemoryDelta, float Falloff = 0.5f, int32 Depth = 1);
    void SelectCell(int32 X, int32 Y);
    int32 GetSelectedX() const { return SelectedX; }
    int32 GetSelectedY() const { return SelectedY; }
    FString GetSelectedCellInfo() const;

    void HarvestTest(int32 X, int32 Y);
    void ApplyTest(int32 X, int32 Y);
    void ShowInventory();
    void MassHarvestTest(int32 X, int32 Y, int32 Count);

protected:
    TArray<FGridCell> Cells;
    TArray<UStaticMeshComponent*> VisualMeshes;
    bool bInterpolationActive = false;
    int32 SelectedX = -1;
    int32 SelectedY = -1;
    UPROPERTY() UStaticMesh* CubeMesh = nullptr;
    UPROPERTY() UMaterial* CubeMaterial = nullptr;

    void InitializeCells();
    void UpdateMemory(FMemoryState& Memory, const FRealState& NewState, float Rate = 0.1f);
    void RecalculateDistortionFromHarvestStress(FGridCell& Cell);
    void InterpolateCell(FGridCell& Cell, float DeltaTime);
    void RegenerateCellResource(FGridCell& Cell, FRandomStream& Rng);
};