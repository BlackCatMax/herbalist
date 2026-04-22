// HerbalistSettings.h
#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "HerbalistSettings.generated.h"

UCLASS(config = Game, defaultconfig, meta = (DisplayName = "Herbalist Settings"))
class PROJECTHERBALIST_API UHerbalistSettings : public UDeveloperSettings
{
    GENERATED_BODY()

public:
    UHerbalistSettings();

    // --- Pipeline coefficients ---

    // Коэффициент влияния BiomeMorokField на Distortion
    UPROPERTY(config, EditAnywhere, Category = "Pipeline|Biome Context", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float BiomeMorokInfluence = 0.3f;

    // Коэффициент влияния BiomeZaryanaField на ZaryanaStrength
    UPROPERTY(config, EditAnywhere, Category = "Pipeline|Biome Context", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float BiomeZaryanaInfluence = 0.3f;

    // Вес влияния BiomeAxisDrift на Delta.Direction
    UPROPERTY(config, EditAnywhere, Category = "Pipeline|Biome Context", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float BiomeAxisDriftWeight = 0.1f;

    // --- Water blending ---

    // Максимальная доля воды до применения штрафа
    UPROPERTY(config, EditAnywhere, Category = "Pipeline|Water", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float MaxWaterRatio = 0.8f;

    // Штраф разбавления при превышении MaxWaterRatio
    UPROPERTY(config, EditAnywhere, Category = "Pipeline|Water", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float WaterDilutionPenalty = 0.2f;

    // --- Fold ---
    UPROPERTY(config, EditAnywhere, Category = "Pipeline|Fold", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float FoldWeightDecay = 0.8f;

    // --- Morok ---
    UPROPERTY(config, EditAnywhere, Category = "Pipeline|Morok", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float MorokMixStrengthFactor = 0.5f;

    // --- Zaryana ---
    UPROPERTY(config, EditAnywhere, Category = "Pipeline|Zaryana", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float ZaryanaBoostFactor = 0.5f;

    UPROPERTY(config, EditAnywhere, Category = "Pipeline|Zaryana", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float ZaryanaSuppressFactor = 0.3f;

    // --- Distortion ---
    UPROPERTY(config, EditAnywhere, Category = "Pipeline|Distortion", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float EnvironmentToxicityWeight = 0.5f;

    // --- Bifurcation ---
    UPROPERTY(config, EditAnywhere, Category = "Pipeline|Bifurcation", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float BifurcationThreshold = 0.85f;
};

// Доступ к настройкам
UHerbalistSettings* GetHerbalistSettings();