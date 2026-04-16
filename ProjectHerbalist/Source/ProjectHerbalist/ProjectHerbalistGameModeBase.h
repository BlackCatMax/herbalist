// Copyright Project Herbalist. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "Core/Types/HerbalistCoreTypes.h"
#include "ProjectHerbalistGameModeBase.generated.h"

class AGridWorldManager;
class AHerbalistPlayerController;

UCLASS()
class PROJECTHERBALIST_API AProjectHerbalistGameModeBase : public AGameModeBase
{
    GENERATED_BODY()

public:
    AProjectHerbalistGameModeBase();
    virtual ~AProjectHerbalistGameModeBase();
    virtual void BeginPlay() override;

    // Параметры тестов
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Test")
    FRngState Rng;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Test")
    bool bEnableRandomResourceMutation = false;

    // Параметры мира (передаются в GridWorldManager)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World")
    float StateInterpolationSpeed = 0.05f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ecology")
    bool bHarvestAffectsBiome = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ecology")
    float HarvestStressIncrement = 0.001f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ecology")
    float HarvestStressThreshold = 0.3f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ecology")
    float MaxHarvestImpactOnDistortion = 0.2f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ecology")
    float MaxHarvestImpactOnMagnitude = 0.1f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ecology")
    float HarvestStressDecayRate = 0.0005f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ecology")
    bool bEnableEcologyRecovery = true;

    UPROPERTY(BlueprintReadOnly, Category = "World")
    AGridWorldManager* WorldManager = nullptr;

    float GetStateInterpolationSpeed() const { return StateInterpolationSpeed; }

protected:
    void SpawnWorldManager();
};