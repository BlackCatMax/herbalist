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

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FRealState State;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FEnvironment Environment;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EBiomeType Biome;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 X = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 Y = 0;
};

UCLASS()
class PROJECTHERBALIST_API AGridWorldManager : public AActor
{
    GENERATED_BODY()

public:
    AGridWorldManager();

    virtual void BeginPlay() override;

    // Размер сетки
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid")
    int32 GridSizeX = 5;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid")
    int32 GridSizeY = 5;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid")
    float CellSize = 100.0f;

    // Доступ к ячейке
    FGridCell* GetCell(int32 X, int32 Y);
    const FGridCell* GetCellConst(int32 X, int32 Y) const;

    // Обновление состояния ячейки
    UFUNCTION(BlueprintCallable, Category = "Grid")
    void UpdateCellState(int32 X, int32 Y, const FRealState& NewState);

    // Сбор ресурса (C++ версия с условиями)
    FRealState HarvestFromCell(int32 X, int32 Y, EResourceType ResourceType, const FConditionModifier& Conditions);

    // Упрощённая версия для Blueprint (без условий)
    UFUNCTION(BlueprintCallable, Category = "Harvest")
    FRealState HarvestFromCellSimple(int32 X, int32 Y, EResourceType ResourceType);

    // Применить результат алхимии к ячейке
    UFUNCTION(BlueprintCallable, Category = "Alchemy")
    void ApplyAlchemyResult(int32 X, int32 Y, const TArray<FRealState>& Ingredients, const FIntent& Intent, FRngState& Rng);

    // Визуализация
    UFUNCTION(BlueprintCallable, Category = "Visuals")
    void SpawnVisuals();

    // Обновить цвет ячейки
    void UpdateCellColor(int32 X, int32 Y);

    // Тестовые методы (вызываются из PlayerController)
    void HarvestTest(int32 X, int32 Y, int32 ResourceType);
    void ApplyTest(int32 X, int32 Y);
    void ShowInventory();

protected:
    UPROPERTY()
    TArray<FGridCell> Cells;

    UPROPERTY()
    TArray<UStaticMeshComponent*> VisualMeshes;

    UPROPERTY()
    UStaticMesh* CubeMesh = nullptr;

    UPROPERTY()
    UMaterial* CubeMaterial = nullptr;

    void InitializeCells();
};