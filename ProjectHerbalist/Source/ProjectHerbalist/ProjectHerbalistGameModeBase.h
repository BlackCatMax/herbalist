#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "Core/Types/HerbalistCoreTypes.h"
#include "ProjectHerbalistGameModeBase.generated.h"

UCLASS()
class PROJECTHERBALIST_API AProjectHerbalistGameModeBase : public AGameModeBase
{
    GENERATED_BODY()

public:
    AProjectHerbalistGameModeBase();

protected:
    virtual void BeginPlay() override;

public:

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Test")
    bool bUseEditorInputs = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Test")
    FRealState A;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Test")
    FRealState B;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Test")
    FEnvironment Env;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Test")
    FMemoryState Memory;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Test")
    FIntent Intent;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Test")
    FRngState Rng;
};