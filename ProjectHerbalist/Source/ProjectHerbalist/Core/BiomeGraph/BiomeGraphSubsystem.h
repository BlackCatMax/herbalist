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

    // Персистентность накопленных полей (AUDIT_AND_REFACTORING_PLAN.md §7.1,
    // "Правильная" альтернатива — реализовано 2026-09-06, решение
    // пользователя: граф должен переживать сохранение, не сбрасываться к
    // дефолтам DA_BiomeGraph при каждой загрузке). MorokField/ZaryanaField
    // помечены Transient (BiomeGraphTypes.h) — не участвуют в обычной
    // UPROPERTY-сериализации намеренно, поэтому нужен явный путь. Восстанавливает
    // ТОЛЬКО динамическую часть узла (MorokField/ZaryanaField/Memory) поверх
    // уже загруженных статических параметров дизайна (MorokAffinity/
    // ZaryanaAffinity/Stability из DA_BiomeGraph, InitializeFromAsset должен
    // отработать раньше) — не перезаписывает узел целиком, на случай если
    // DA_BiomeGraph когда-нибудь изменится между сессиями. Узлы, отсутствующие
    // в InNodes (например, новый биом добавлен в граф уже после этого сейва),
    // тихо остаются на дефолтах InitializeFromAsset — не ошибка.
    void RestoreNodeFieldState(const TMap<FName, FBiomeGraphNode>& InNodes);

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

protected:
    UPROPERTY()
    TMap<FName, FBiomeGraphNode> Nodes;

    UPROPERTY()
    TArray<FBiomeGraphEdge> Edges;

    float GlobalMorokDecay = 0.01f;
    float GlobalZaryanaDecay = 0.005f;
    float InstabilityDecay = 0.025f;
    float AxisDriftDecay = 0.1f;
    float PotionCollapseThreshold = 0.85f;
    float BiomeCollapseThreshold = 0.9f;   // не читается нигде — 14_Biome_Graph §14.7, резерв
    float FixedTimeStep = 0.2f;
    float GlobalInfluenceScale = 1.0f;
    float GridBlendFactor = 0.3f;

    bool bInitialized = false;
    float TimeAccumulator = 0.f;

    mutable TWeakObjectPtr<AGridWorldManager> CachedGridWorldManager;

    AGridWorldManager* FindGridWorldManager() const;

    TMap<FName, FVector> CachedBiomeCenters;
    float CenterCacheTimer = 0.f;
    static constexpr float CenterCacheUpdateInterval = 2.0f;
    void UpdateBiomeCenters(AGridWorldManager* Grid);

    void InternalStep(float StepDeltaTime);
    void RecalculateFieldsFromGrid(AGridWorldManager* Grid);
    void PropagateWaves(AGridWorldManager* Grid);
    void ApplyFieldsToGrid(AGridWorldManager* Grid, float StepDeltaTime);
    void UpdateMemories(float StepDeltaTime);
};