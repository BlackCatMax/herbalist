// HerbalistCoreTypes.h
#pragma once

#include "CoreMinimal.h"
#include "HerbalistCoreTypes.generated.h"

// ========== Enum'ы ==========
UENUM(BlueprintType)
enum class EBiomeType : uint8
{
    MixedForest,
    Swamp,
    Steppe,
    Floodplain
};

UENUM(BlueprintType)
enum class EResourceType : uint8
{
    None = 0,
    Nettle,
    Fern,
    Mushroom,
    BirchBark,
    Moss,
    Cranberry,
    BogOre
};

// ========== Базовые структуры ==========
USTRUCT(BlueprintType)
struct PROJECTHERBALIST_API FDirection
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite) float Body = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float Mind = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float Spirit = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float Nature = 0.f;

    // Нормализация по сумме (все компоненты ≥ 0, сумма = 1)
    void NormalizeSum()
    {
        Body = FMath::Max(0.0f, Body);
        Mind = FMath::Max(0.0f, Mind);
        Spirit = FMath::Max(0.0f, Spirit);
        Nature = FMath::Max(0.0f, Nature);

        float Sum = Body + Mind + Spirit + Nature;
        if (Sum > KINDA_SMALL_NUMBER)
        {
            Body /= Sum;
            Mind /= Sum;
            Spirit /= Sum;
            Nature /= Sum;
        }
        else
        {
            Body = Mind = Spirit = Nature = 0.25f;
        }
    }
};

USTRUCT(BlueprintType)
struct PROJECTHERBALIST_API FMeta
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float Distortion = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float Stability = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float Purity = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float Potency = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float Resonance = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float Corruption = 0.f;
};

USTRUCT(BlueprintType)
struct PROJECTHERBALIST_API FRealState
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float Magnitude = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FDirection Direction;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FMeta Meta;
};

USTRUCT(BlueprintType)
struct PROJECTHERBALIST_API FEnvironment
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float Toxicity = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float Fertility = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float Moisture = 0.f;
};

USTRUCT(BlueprintType)
struct PROJECTHERBALIST_API FMemoryState
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float AccumulatedDistortion = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float StabilityMemory = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float HistoryPurity = 0.f;
};

USTRUCT(BlueprintType)
struct PROJECTHERBALIST_API FIntent
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float Coherence = 0.f;
};

USTRUCT(BlueprintType)
struct PROJECTHERBALIST_API FRngState
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Seed = 12345;
};

USTRUCT(BlueprintType)
struct PROJECTHERBALIST_API FWorldState
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FEnvironment Env;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FMemoryState Memory;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FIntent Intent;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FRealState CurrentState;
};

// ========== Структура клетки (без TargetMemory) ==========
USTRUCT(BlueprintType)
struct PROJECTHERBALIST_API FGridCell
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 X = 0, Y = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) EBiomeType Biome = EBiomeType::MixedForest;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FRealState State;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FRealState TargetState;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FEnvironment Environment;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FMemoryState Memory;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float HarvestStress = 0.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bEntityTriggered = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) EResourceType AvailableResource = EResourceType::None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float ResourceRegrowthTimer = 0.0f;
};

USTRUCT(BlueprintType)
struct PROJECTHERBALIST_API FConditionModifier
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite) FDirection DeltaDirection;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float DeltaMagnitude = 0.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float DeltaDistortion = 0.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float DeltaStability = 0.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float DeltaPurity = 0.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float DeltaPotency = 0.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float DeltaResonance = 0.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float DeltaCorruption = 0.0f;

    FConditionModifier()
    {
        DeltaDirection.Body = 0.0f;
        DeltaDirection.Mind = 0.0f;
        DeltaDirection.Spirit = 0.0f;
        DeltaDirection.Nature = 0.0f;
    }
};

// ========== Вспомогательный класс ==========
struct PROJECTHERBALIST_API FAlatyr
{
    static const FRealState S0;
};