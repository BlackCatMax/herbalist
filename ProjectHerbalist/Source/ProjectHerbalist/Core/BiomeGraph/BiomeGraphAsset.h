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

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Biome Graph|Collapse", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float CollapseThreshold = 0.85f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Biome Graph|Simulation", meta = (ClampMin = "0.05", ClampMax = "2.0"))
    float FixedTimeStep = 0.2f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Biome Graph|Simulation", meta = (ClampMin = "0.0", ClampMax = "2.0"))
    float GlobalInfluenceScale = 1.0f;

    UPROPERTY()
    int32 GraphVersion = 1;
};
