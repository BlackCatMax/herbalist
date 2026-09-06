// BiomeGraphTypes.h
#pragma once

#include "CoreMinimal.h"
#include "BiomeGraphTypes.generated.h"

// Контракт для получения данных от Grid
USTRUCT(BlueprintType)
struct FGridBiomeSample
{
    GENERATED_BODY()

    UPROPERTY()
    FName BiomeID;

    UPROPERTY()
    float MorokValue = 0.f;     // Distortion

    UPROPERTY()
    // Знаковое ОТКЛОНЕНИЕ Stability клетки от дефолта её биома, [-1,1]
    // (2026-09-07, вариант "а" по выбору пользователя; было `1 - Distortion`,
    // из-за чего Заряна была зеркалом Морока, а Purity/Stability уезжали от
    // собственных дефолтов биома -- MATH_REFERENCE.md §6.2).
    float ZaryanaValue = 0.f;
};

USTRUCT(BlueprintType)
struct FBiomeMemory
{
    GENERATED_BODY()

    // Накопленный дрейф осей (4-осевая модель: Body, Mind, Spirit, Nature)
    UPROPERTY()
    FVector4 AxisDrift = FVector4(0.25f, 0.25f, 0.25f, 0.25f);

    UPROPERTY()
    float MorokHistory = 0.f;

    UPROPERTY()
    float ZaryanaHistory = 0.f;

    UPROPERTY()
    float Instability = 0.f;
};

USTRUCT(BlueprintType)
struct FBiomeGraphNode
{
    GENERATED_BODY()

    // BiomeID ТОЛЬКО как ключ TMap, в структуре не хранится

    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float MorokAffinity = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float ZaryanaAffinity = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float Stability = 0.5f;

    UPROPERTY(Transient)
    float MorokField = 0.f;

    UPROPERTY(Transient)
    float ZaryanaField = 0.f;

    UPROPERTY()
    FBiomeMemory Memory;
};

// Запись для DataAsset – связывает BiomeID с параметрами узла
USTRUCT(BlueprintType)
struct FBiomeGraphNodeEntry
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FName BiomeID;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FBiomeGraphNode Node;
};

USTRUCT(BlueprintType)
struct FBiomeGraphEdge
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FName FromBiome;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FName ToBiome;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float MorokLeak = 0.1f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float ZaryanaFlow = 0.1f;
};
