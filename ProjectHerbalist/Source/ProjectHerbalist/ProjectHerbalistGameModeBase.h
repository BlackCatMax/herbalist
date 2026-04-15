#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "Core/World/HerbalistWorldConfig.h"
#include "Core/Types/HerbalistCoreTypes.h"
#include "ProjectHerbalistGameModeBase.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnInventoryChanged);

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

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Config")
    UHerbalistWorldConfig* WorldConfig;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Test")
    FRngState Rng;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Test")
    bool bEnableRandomResourceMutation = false;

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

    TArray<FRealState*> Inventory;

    UPROPERTY(BlueprintReadOnly, Category = "World")
    AGridWorldManager* WorldManager = nullptr;

    UPROPERTY(BlueprintAssignable, Category = "Inventory")
    FOnInventoryChanged OnInventoryChanged;

    void AddToInventory(const FRealState& Resource);
    void RemoveFromInventory(int32 Index);
    TArray<FRealState*> GetInventory() const { return Inventory; }

    float GetStateInterpolationSpeed() const { return StateInterpolationSpeed; }

protected:
    void SpawnWorldManager();
};