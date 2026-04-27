// IngredientTableRow.h
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Core/Types/HerbalistCoreTypes.h"
#include "IngredientTableRow.generated.h"

UENUM(BlueprintType)
enum class EIngredientClass : uint8
{
    Water       UMETA(DisplayName = "Water"),
    Plant       UMETA(DisplayName = "Plant"),
    Mineral     UMETA(DisplayName = "Mineral"),
    Fungus      UMETA(DisplayName = "Fungus"),
    Catalyst    UMETA(DisplayName = "Catalyst"),
    Essence     UMETA(DisplayName = "Essence"),
    Unknown     UMETA(DisplayName = "Unknown")
};

USTRUCT(BlueprintType)
struct PROJECTHERBALIST_API FIngredientTableRow : public FTableRowBase
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Display")
    FText DisplayName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Display", meta = (MultiLine = true))
    FText Description;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Display")
    TSoftObjectPtr<UTexture2D> Icon;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visual")
    UStaticMesh* ResourceMesh = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
    FRealState BaseState;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
    EIngredientClass Class = EIngredientClass::Unknown;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
    bool bIsWater = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawning")
    TArray<EBiomeType> AllowedBiomes;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawning", meta = (ClampMin = "1"))
    int32 RarityWeight = 1;

    // Множитель скорости порчи в инвентаре (1.0 = стандартная скорость)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gameplay", meta = (ClampMin = "0.0", ClampMax = "10.0"))
    float DecayRate = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gameplay")
    FName Element;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gameplay")
    TArray<FName> Tags;
};