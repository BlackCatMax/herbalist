#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "Core/World/HerbalistWorldConfig.h"
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

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Config")
    UHerbalistWorldConfig* WorldConfig;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Test")
    FRngState Rng;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Test")
    bool bEnableRandomResourceMutation = false;

    // Скорость интерполяции параметров мира (чем меньше, тем медленнее)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World")
    float StateInterpolationSpeed = 0.05f;

    // Инвентарь – без UPROPERTY, храним указатели
    TArray<FRealState*> Inventory;

    void AddToInventory(const FRealState& Resource);
    void RemoveFromInventory(int32 Index);
    TArray<FRealState*> GetInventory() const { return Inventory; }

    UPROPERTY(BlueprintReadOnly, Category = "World")
    AGridWorldManager* WorldManager = nullptr;

    float GetStateInterpolationSpeed() const { return StateInterpolationSpeed; }

protected:
    void SpawnWorldManager();
};