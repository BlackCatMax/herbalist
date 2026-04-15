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
    UPROPERTY(EditAnywhere, BlueprintReadWrite) EBiomeType Biome;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 X = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Y = 0;
};

UCLASS()
class PROJECTHERBALIST_API AGridWorldManager : public AActor
{
    GENERATED_BODY()
public:
    AGridWorldManager();
    virtual void BeginPlay() override;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid") int32 GridSizeX = 5;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid") int32 GridSizeY = 5;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid") float CellSize = 100.0f;

    FGridCell* GetCell(int32 X, int32 Y);
    const FGridCell* GetCellConst(int32 X, int32 Y) const;
    void UpdateCellState(int32 X, int32 Y, const FRealState& NewState);
    FRealState HarvestFromCell(int32 X, int32 Y, EResourceType ResourceType, const FConditionModifier& Conditions);
    UFUNCTION(BlueprintCallable, Category = "Harvest") FRealState HarvestFromCellSimple(int32 X, int32 Y, EResourceType ResourceType);
    UFUNCTION(BlueprintCallable, Category = "Alchemy") void ApplyAlchemyResult(int32 X, int32 Y, const TArray<FRealState>& Ingredients, const FIntent& Intent, FRngState& Rng);
    UFUNCTION(BlueprintCallable, Category = "Visuals") void SpawnVisuals();
    void UpdateCellColor(int32 X, int32 Y);
    void PropagateToNeighbors(int32 X, int32 Y, const FRealState& Delta, float Falloff = 0.5f, int32 Depth = 2);

    // Тестовые команды
    void HarvestTest(int32 X, int32 Y, int32 ResourceType);
    void ApplyTest(int32 X, int32 Y);
    void ShowInventory();

protected:
    TArray<FGridCell> Cells;
    TArray<UStaticMeshComponent*> VisualMeshes;
    UPROPERTY() UStaticMesh* CubeMesh = nullptr;
    UPROPERTY() UMaterial* CubeMaterial = nullptr;
    void InitializeCells();
};