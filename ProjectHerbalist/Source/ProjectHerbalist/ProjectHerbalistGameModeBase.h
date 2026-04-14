#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "Core/World/HerbalistWorldConfig.h"
#include "Core/Types/HerbalistCoreTypes.h"

#include "ProjectHerbalistGameModeBase.generated.h"

UCLASS()
class PROJECTHERBALIST_API AProjectHerbalistGameModeBase : public AGameModeBase
{
    GENERATED_BODY()

public:

    virtual void BeginPlay() override;

    // =========================
    // CONFIG
    // =========================
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Config")
    UHerbalistWorldConfig* WorldConfig;

    // =========================
    // STATE (НЕ ДУБЛИРОВАТЬ В CPP)
    // =========================
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FRealState A;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FRealState B;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FRngState Rng;
};