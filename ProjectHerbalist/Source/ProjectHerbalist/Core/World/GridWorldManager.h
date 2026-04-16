// GridWorldManager.h
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/Types/HerbalistCoreTypes.h"
#include "Core/Inventory/HerbalistInventoryComponent.h" // для FInventoryItem
#include "GridWorldManager.generated.h"

UCLASS()
class PROJECTHERBALIST_API AGridWorldManager : public AActor
{
    GENERATED_BODY()

public:
    AGridWorldManager();

    // Параметры сетки
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grid")
    int32 GridSizeX = 40;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grid")
    int32 GridSizeY = 40;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grid")
    float CellSize = 100.0f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grid")
    float CellHeight = 50.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Simulation")
    float StateInterpolationSpeed = 5.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Simulation")
    int32 PropagationDepth = 1;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Harvest")
    bool bHarvestAffectsBiome = true;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Harvest")
    float HarvestStressIncrement = 0.1f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Harvest")
    float HarvestStressThreshold = 0.5f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Harvest")
    float MaxHarvestImpactOnDistortion = 0.3f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Harvest")
    float MaxHarvestImpactOnMagnitude = 0.2f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Harvest")
    float HarvestStressDecayRate = 0.5f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Harvest")
    bool bEnableRecovery = true;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Regrowth")
    float ResourceRegrowthTime = 5.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Debug")
    float BorderThickness = 5.0f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Debug")
    bool bEnableDebugDraw = true;

    FGridCell* GetCell(int32 X, int32 Y);
    const FGridCell* GetCellConst(int32 X, int32 Y) const;

    UFUNCTION(BlueprintCallable, Category = "Grid")
    void SetTargetState(int32 X, int32 Y, const FRealState& NewState);

    UFUNCTION(BlueprintCallable, Category = "Harvest")
    FRealState HarvestFromCell(int32 X, int32 Y, const FConditionModifier& Conditions);

    UFUNCTION(BlueprintCallable, Category = "Harvest")
    FRealState HarvestFromCellSimple(int32 X, int32 Y);

    // Новая перегрузка для алхимии с FInventoryItem
    void ApplyAlchemyResult(int32 X, int32 Y, const TArray<FInventoryItem>& Ingredients, const FIntent& Intent, FRngState& Rng);

    // Старая перегрузка для обратной совместимости
    UFUNCTION(BlueprintCallable, Category = "Alchemy")
    void ApplyAlchemyResult(int32 X, int32 Y, const TArray<FRealState>& Ingredients, const FIntent& Intent, FRngState& Rng);

    UFUNCTION(BlueprintCallable, Category = "Selection")
    void SelectCell(int32 X, int32 Y);

    UFUNCTION(BlueprintCallable, Category = "Selection")
    FString GetSelectedCellInfo() const;

    UFUNCTION(BlueprintCallable, Category = "Selection")
    void GetSelectedCellInfoBP(int32& X, int32& Y, FString& ResourceName, float& RegrowthTimer, float& Distortion, float& HarvestStress);

    UFUNCTION(BlueprintCallable, Category = "Test")
    void HarvestTest(int32 X, int32 Y);

    UFUNCTION(BlueprintCallable, Category = "Test")
    void MassHarvestTest(int32 X, int32 Y, int32 Count);

    UFUNCTION(BlueprintCallable, Category = "Test")
    void ApplyTest(int32 X, int32 Y);

    UFUNCTION(BlueprintCallable, Category = "Test")
    void ShowInventory();

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;

private:
    TArray<FGridCell> Cells;
    TSet<int32> DirtyCells;
    TSet<int32> RegrowingCells;
    TSet<int32> StressCells;
    bool bInterpolationActive = false;
    int32 SelectedX = -1, SelectedY = -1;
    FTimerHandle DebugDrawTimer;

    FRandomStream WorldRNG;

    void InitializeCells();
    void RegenerateCellResource(FGridCell& Cell);
    void UpdateMemory(FMemoryState& Memory, const FRealState& NewState, float Rate);
    void RecalculateDistortionFromHarvestStress(FGridCell& Cell);
    void PropagateToNeighbors(int32 X, int32 Y, const FRealState& Delta, float Falloff, int32 Depth);
    void InterpolateCell(FGridCell& Cell, float DeltaTime);
    void RedrawDebugBoxes();

    inline int32 GetCellIndex(int32 X, int32 Y) const { return Y * GridSizeX + X; }
    inline void MarkDirty(int32 X, int32 Y) { DirtyCells.Add(GetCellIndex(X, Y)); }
    inline void MarkStress(int32 X, int32 Y) { StressCells.Add(GetCellIndex(X, Y)); }
    inline void MarkRegrowing(int32 X, int32 Y) { RegrowingCells.Add(GetCellIndex(X, Y)); }
    inline void UnmarkRegrowing(int32 X, int32 Y) { RegrowingCells.Remove(GetCellIndex(X, Y)); }
};