#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Core/Types/HerbalistCoreTypes.h"
#include "HerbalistWorldConfig.generated.h"

UCLASS(BlueprintType)
class PROJECTHERBALIST_API UHerbalistWorldConfig : public UDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World")
    FEnvironment Environment;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World")
    FMemoryState InitialMemory;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World")
    FIntent Intent;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World")
    FRealState InitialBiomeState;   // <-- НОВОЕ ПОЛЕ

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Test")
    FRealState InputA;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Test")
    FRealState InputB;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Simulation")
    float MemoryAccumulationRate = 0.1f;
};