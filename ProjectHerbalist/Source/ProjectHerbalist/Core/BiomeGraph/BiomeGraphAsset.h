// BiomeGraphAsset.h
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "BiomeGraphTypes.h"
#include "BiomeGraphAsset.generated.h"

UCLASS(BlueprintType)
class PROJECTHERBALIST_API UBiomeGraphAsset : public UDataAsset
{
    GENERATED_BODY()

public:
    // Узлы графа – теперь с явным BiomeID для редактора
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Biome Graph|Nodes")
    TArray<FBiomeGraphNodeEntry> Nodes;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Biome Graph|Edges")
    TArray<FBiomeGraphEdge> Edges;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Biome Graph|Simulation", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float GlobalMorokDecay = 0.01f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Biome Graph|Simulation", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float GlobalZaryanaDecay = 0.005f;

    // Затухание Memory.Instability/AxisDrift (2026-09-06, аудит на аудит --
    // найдено фоновым агентом сразу после фикса GlobalMorokDecay в этом же
    // UpdateMemories: обе строки ниже decay'ились голыми множителями за ШАГ
    // (0.995f/0.98f), без домножения на StepDeltaTime, той же болезнью, что
    // уже была у MorokField/GnilnikiNudgeRate/HistoryPurity выше по проекту.
    // Значения посчитаны ОБРАТНО из старых констант при боевом FixedTimeStep
    // (0.2с), чтобы фикс не менял текущий баланс, только чинил
    // масштабируемость: (1-0.995)/0.2=0.025, (1-0.98)/0.2=0.1.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Biome Graph|Simulation", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float InstabilityDecay = 0.025f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Biome Graph|Simulation", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float AxisDriftDecay = 0.1f;

    // Порог Bifurcation ОДНОЙ ВАРКИ (05_Systems.md, Collapse/Purification) —
    // переименовано из CollapseThreshold: старое имя не говорило, что это
    // порог именно зелья, а не биома, хотя это два разных по масштабу и
    // канону явления (13_World_Pipeline vs 14_Biome_Graph §14.7/§14.9,
    // META_AUDIT §8).
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Biome Graph|Collapse", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float PotionCollapseThreshold = 0.85f;

    // Порог коллапса/возрождения ЦЕЛОГО БИОМА (14_Biome_Graph §14.7 —
    // "зарезервировано, но не активировано"). Заведён заранее, отдельно от
    // PotionCollapseThreshold: варка и судьба целого биома — события разного
    // масштаба и драматичности, не должны срабатывать от одного числа.
    // Пока не читается нигде — механика Collapse/Rebirth не реализована.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Biome Graph|Collapse", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float BiomeCollapseThreshold = 0.9f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Biome Graph|Simulation", meta = (ClampMin = "0.05", ClampMax = "2.0"))
    float FixedTimeStep = 0.2f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Biome Graph|Simulation", meta = (ClampMin = "0.0", ClampMax = "2.0"))
    float GlobalInfluenceScale = 1.0f;

    // Насколько сильно средний Distortion/Zaryana по клеткам биома "перетягивает"
    // MorokField/ZaryanaField узла на каждом шаге: 0 = поле живёт только за счёт
    // PropagateWaves (полная память, среднее по гриду не влияет), 1 = поведение
    // до фикса (поле каждый шаг полностью перезаписывается средним, без памяти).
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Biome Graph|Simulation", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float GridBlendFactor = 0.3f;

    UPROPERTY()
    int32 GraphVersion = 1;
};
