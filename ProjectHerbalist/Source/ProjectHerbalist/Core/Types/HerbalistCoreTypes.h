#pragma once

#include "CoreMinimal.h"
#include "HerbalistCoreTypes.generated.h"

USTRUCT(BlueprintType)
struct FDirection
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Body = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Mind = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Spirit = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Nature = 0.f;
};

USTRUCT(BlueprintType)
struct FMeta
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Distortion = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Stability = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Purity = 0.f;
};

USTRUCT(BlueprintType)
struct FRealState
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Magnitude = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FDirection Direction;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FMeta Meta;
};

USTRUCT(BlueprintType)
struct FEnvironment
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Toxicity = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Fertility = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Moisture = 0.f;
};

USTRUCT(BlueprintType)
struct FMemoryState
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float AccumulatedDistortion = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float StabilityMemory = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float HistoryPurity = 0.f;
};

USTRUCT(BlueprintType)
struct FIntent
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Coherence = 0.f;
};

USTRUCT(BlueprintType)
struct FRngState
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 Seed = 12345;

};

// HerbalistCoreTypes.h (добавить в конец перед последней скобкой)

USTRUCT(BlueprintType)
struct FWorldState
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FEnvironment Env;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FMemoryState Memory;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FIntent Intent;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FRealState CurrentState;
};