// GridWorldManager.h
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/Types/HerbalistCoreTypes.h"
#include "Core/Types/BiomeTypes.h"
#include "Core/Inventory/HerbalistInventoryComponent.h"
#include "Math/RandomStream.h"
#include "Core/BiomeGraph/BiomeGraphTypes.h"
#include "GridWorldManager.generated.h"

class UHarvestService;

UCLASS()
class PROJECTHERBALIST_API AGridWorldManager : public AActor
{
    GENERATED_BODY()

public:
    AGridWorldManager();

    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World")
    int32 GridSizeX = 20;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World")
    int32 GridSizeY = 20;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World")
    float CellSize = 100.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World")
    float CellHeight = 10.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Harvest")
    float ResourceRegrowthTime = 10.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Harvest")
    bool bHarvestAffectsBiome = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Harvest")
    float HarvestStressIncrement = 0.1f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Harvest")
    float HarvestStressDecayRate = 0.05f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Harvest")
    float HarvestStressThreshold = 0.3f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Harvest")
    float MaxHarvestImpactOnDistortion = 0.3f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Harvest")
    float MaxHarvestImpactOnMagnitude = 0.2f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interpolation")
    float StateInterpolationSpeed = 2.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Propagation")
    int32 PropagationDepth = 2;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug")
    bool bEnableDebugDraw = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug")
    float BorderThickness = 2.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recovery")
    bool bEnableRecovery = true;

    FGridCell* GetCell(int32 X, int32 Y);
    const FGridCell* GetCellConst(int32 X, int32 Y) const;
    void SetTargetState(int32 X, int32 Y, const FRealState& NewState);

    void ApplyAlchemyResult(int32 X, int32 Y, const TArray<FInventoryItem>& Ingredients, const FIntent& Intent, FRngState& Rng);
    void ApplyAlchemyResult(int32 X, int32 Y, const TArray<FRealState>& Ingredients, const FIntent& Intent, FRngState& Rng);
    void PropagateToNeighbors(int32 X, int32 Y, const FRealState& Delta, float Falloff, int32 Depth);

    FRealState HarvestFromCell(int32 X, int32 Y, const FConditionModifier& Conditions = FConditionModifier());
    FRealState HarvestFromCellSimple(int32 X, int32 Y);

    void ApplyPotionToCell(int32 X, int32 Y, const FRealState& PotionState);

    void SelectCell(int32 X, int32 Y);
    FString GetSelectedCellInfo() const;
    UFUNCTION(BlueprintCallable, Category = "Debug")
    void GetSelectedCellInfoBP(int32& X, int32& Y, FString& ResourceName, float& RegrowthTimer, float& Distortion, float& HarvestStress);

    UFUNCTION(Exec, BlueprintCallable, Category = "Test")
    void HarvestTest(int32 X, int32 Y);
    UFUNCTION(Exec, BlueprintCallable, Category = "Test")
    void MassHarvestTest(int32 X, int32 Y, int32 Count);
    UFUNCTION(Exec, BlueprintCallable, Category = "Test")
    void ApplyTest(int32 X, int32 Y);
    UFUNCTION(Exec, BlueprintCallable, Category = "Test")
    void ShowInventory();

#if WITH_EDITOR
    bool bShowBiomeGraph = false;
    bool bShowCellDistortion = false;
    bool bShowCellInfluence = false;
#endif

    void DrawBiomeGraphDebug();

    TArray<FGridBiomeSample> GetBiomeSamples() const;
    TMap<FName, FVector> GetBiomeCenters() const;
    void ApplyBiomeInfluences(const TMap<FName, float>& MorokFields, const TMap<FName, float>& ZaryanaFields, float GlobalScale);

    template<typename TFunc>
    void ForEachCell(TFunc&& Func)
    {
        for (FGridCell& Cell : Cells) Func(Cell);
    }

    template<typename TFunc>
    void ForEachCell(TFunc&& Func) const
    {
        for (const FGridCell& Cell : Cells) Func(Cell);
    }

    FVector GetCellWorldPosition(int32 X, int32 Y) const;

protected:
    TArray<FGridCell> Cells;
    FRandomStream WorldRNG;

    TSet<int32> DirtyCells;
    TSet<int32> RegrowingCells;
    TSet<int32> StressCells;
    bool bInterpolationActive = false;

    TMap<int32, float> LastHarvestTimeMap;
    const float HarvestCooldown = 0.2f;

    UPROPERTY()
    UHarvestService* HarvestService;

    UFUNCTION(BlueprintCallable, Category = "World|Init")
    void InitializeCells();

    void RegenerateCellResource(FGridCell& Cell);
    void InterpolateCell(FGridCell& Cell, float DeltaTime);
    void UpdateMemory(FMemoryState& Memory, const FRealState& NewState, float Rate);
    void RecalculateDistortionFromHarvestStress(FGridCell& Cell);
    void RedrawDebugBoxes();

    void MarkDirty(int32 X, int32 Y) { DirtyCells.Add(Y * GridSizeX + X); }
    void MarkRegrowing(int32 X, int32 Y) { RegrowingCells.Add(Y * GridSizeX + X); }
    void UnmarkRegrowing(int32 X, int32 Y) { RegrowingCells.Remove(Y * GridSizeX + X); }
    void MarkStress(int32 X, int32 Y) { StressCells.Add(Y * GridSizeX + X); }

    inline int32 GetCellIndex(int32 X, int32 Y) const { return Y * GridSizeX + X; }

private:
    int32 SelectedX = -1, SelectedY = -1;
    FTimerHandle DebugDrawTimer;
};
