// BiomeGraphSubsystem.h
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "BiomeGraphTypes.h"
#include "BiomeGraphSubsystem.generated.h"

class UBiomeGraphAsset;
class AGridWorldManager;
class UMaterialParameterCollection;   // forward declaration
struct FBiomeSnapshot;
struct FStateDelta;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnBiomeGraphStep, float);

UCLASS()
class PROJECTHERBALIST_API UBiomeGraphSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()

public:
    virtual void OnWorldBeginPlay(UWorld& InWorld) override;
    virtual void Deinitialize() override;
	void ApplyStateDelta(const FStateDelta& Delta);

    void InitializeFromAsset(UBiomeGraphAsset* Asset);
    bool IsInitialized() const { return bInitialized; }

    void StepSimulation(float DeltaTime);
    void ForceStep();

    void RecordFootprint(FName BiomeID, float MorokImpact, float ZaryanaImpact, const FVector4& AxisDelta, float DeltaTime);

    const TMap<FName, FBiomeGraphNode>& GetNodes() const { return Nodes; }
    const TArray<FBiomeGraphEdge>& GetEdges() const { return Edges; }

    const FBiomeGraphNode* GetNode(FName BiomeID) const;
    FBiomeGraphNode* GetMutableNode(FName BiomeID);

    void DebugPrintNodes() const;
    void ResetGraph();
	
	FBiomeSnapshot CaptureState() const;

    const TMap<FName, FVector>& GetCachedBiomeCenters() const { return CachedBiomeCenters; }

    // Визуализация через Material Parameter Collection
    UFUNCTION(BlueprintCallable, Category = "BiomeGraph")
    void UpdateVisualization();

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visualization")
    TSoftObjectPtr<UMaterialParameterCollection> VisualizationMPC;

    FOnBiomeGraphStep OnStepExecuted;

protected:
    UPROPERTY()
    TMap<FName, FBiomeGraphNode> Nodes;

    UPROPERTY()
    TArray<FBiomeGraphEdge> Edges;

    float GlobalMorokDecay = 0.01f;
    float GlobalZaryanaDecay = 0.005f;
    float CollapseThreshold = 0.85f;
    float FixedTimeStep = 0.2f;
    float GlobalInfluenceScale = 1.0f;

    bool bInitialized = false;
    float TimeAccumulator = 0.f;

    mutable TWeakObjectPtr<AGridWorldManager> CachedGridWorldManager;

    AGridWorldManager* FindGridWorldManager() const;
    bool IsGridValid() const;

    TMap<FName, TArray<int32>> AdjacencyList;
    void BuildAdjacencyList();

    TMap<FName, FVector> CachedBiomeCenters;
    float CenterCacheTimer = 0.f;
    static constexpr float CenterCacheUpdateInterval = 2.0f;
    void UpdateBiomeCenters(AGridWorldManager* Grid);

    enum class EBiomeGraphStepStage : uint8
    {
        GridToGraph,
        Propagation,
        GraphToGrid,
        MemoryUpdate
    };

    void InternalStep(float StepDeltaTime);
    void RecalculateFieldsFromGrid(AGridWorldManager* Grid);
    void PropagateWaves();
    void ApplyFieldsToGrid(AGridWorldManager* Grid);
    void UpdateMemories(float StepDeltaTime);
};