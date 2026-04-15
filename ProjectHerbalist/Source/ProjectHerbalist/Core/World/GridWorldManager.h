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
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FRealState State;          // текущее (интерполируемое) состояние
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FEnvironment Environment;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FMemoryState Memory;       // текущая (интерполируемая) память
    UPROPERTY(EditAnywhere, BlueprintReadWrite) EBiomeType Biome;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 X = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Y = 0;

    // Целевые значения (к ним стремимся)
    FRealState TargetState;
    FMemoryState TargetMemory;

    UPROPERTY() float HarvestStress = 0.0f;
    UPROPERTY() bool bEntityTriggered = false;

    // Конструктор с явной инициализацией всех полей
    FGridCell()
        : State()
        , Environment()
        , Memory()
        , Biome()
        , X(0)
        , Y(0)
        , TargetState()
        , TargetMemory()
        , HarvestStress(0.0f)
        , bEntityTriggered(false)
    {
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

    // Размер сетки
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid") int32 GridSizeX = 5;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid") int32 GridSizeY = 5;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid") float CellSize = 100.0f;

    // Скорость интерполяции (будет установлена из GameMode)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visuals")
    float StateInterpolationSpeed = 0.05f;

    // Параметры экологии (будут установлены из GameMode)
    bool bHarvestAffectsBiome = true;
    float HarvestStressIncrement = 0.001f;
    float HarvestStressThreshold = 0.3f;
    float MaxHarvestImpactOnDistortion = 0.2f;
    float MaxHarvestImpactOnMagnitude = 0.1f;
    float HarvestStressDecayRate = 0.0005f;
    bool bEnableRecovery = true;

    // Доступ к ячейке
    FGridCell* GetCell(int32 X, int32 Y);
    const FGridCell* GetCellConst(int32 X, int32 Y) const;

    // Обновление целевых значений
    void SetTargetState(int32 X, int32 Y, const FRealState& NewState, const FMemoryState& NewMemory);

    // Сбор ресурса
    FRealState HarvestFromCell(int32 X, int32 Y, EResourceType ResourceType, const FConditionModifier& Conditions);
    UFUNCTION(BlueprintCallable, Category = "Harvest") FRealState HarvestFromCellSimple(int32 X, int32 Y, EResourceType ResourceType);

    // Алхимия
    UFUNCTION(BlueprintCallable, Category = "Alchemy") void ApplyAlchemyResult(int32 X, int32 Y, const TArray<FRealState>& Ingredients, const FIntent& Intent, FRngState& Rng);

    // Визуализация
    UFUNCTION(BlueprintCallable, Category = "Visuals") void SpawnVisuals();
    void UpdateCellColor(int32 X, int32 Y);

    // Распространение
    void PropagateToNeighbors(int32 X, int32 Y, const FRealState& Delta, const FMemoryState& MemoryDelta, float Falloff = 0.5f, int32 Depth = 1);

    // UI и выбор ячейки
    void SelectCell(int32 X, int32 Y);
    int32 GetSelectedX() const { return SelectedX; }
    int32 GetSelectedY() const { return SelectedY; }
    FString GetSelectedCellInfo() const;

    UFUNCTION(BlueprintCallable, Category = "UI")
    void UI_HarvestSelectedCell(int32 ResourceType);

    UFUNCTION(BlueprintCallable, Category = "UI")
    void UI_ApplyAlchemyToSelectedCell();

    // Тестовые команды (exec, вызываются из PlayerController)
    void HarvestTest(int32 X, int32 Y, int32 ResourceType);
    void ApplyTest(int32 X, int32 Y);
    void ShowInventory();
    void MassHarvestTest(int32 X, int32 Y, int32 ResourceType, int32 Count);

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
};