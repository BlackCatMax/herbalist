// ProjectHerbalistGameModeBase.h
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

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Config")
    UHerbalistWorldConfig* WorldConfig;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Test")
    FRngState Rng;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Test")
    bool bEnableRandomResourceMutation = false;
};