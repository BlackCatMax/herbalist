// AlchemyTypes.h
#pragma once

#include "CoreMinimal.h"
#include "Core/Types/HerbalistCoreTypes.h"
#include "Core/Data/IngredientTableRow.h"
#include "AlchemyTypes.generated.h"

// --- Atom Origin ---
UENUM()
enum class EAtomOrigin : uint8
{
    Harvest         UMETA(DisplayName = "Harvest"),
    Decomposition   UMETA(DisplayName = "Decomposition"),
    Spawn           UMETA(DisplayName = "Spawn"),
    Unknown         UMETA(DisplayName = "Unknown")
};

// --- Alchemy Atom ---
USTRUCT(BlueprintType)
struct PROJECTHERBALIST_API FAlchemyAtom
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Herbalist|Alchemy")
    bool bIsWater = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Herbalist|Alchemy")
    FName SourceID;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Herbalist|Alchemy")
    FRealState State;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Herbalist|Alchemy")
    FGuid AtomUID;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Herbalist|Alchemy")
    EIngredientClass Class = EIngredientClass::Unknown;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Herbalist|Alchemy")
    EAtomOrigin OriginContext = EAtomOrigin::Unknown;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Herbalist|Alchemy")
    float DistortionAtCollection = 0.3f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Herbalist|Alchemy")
    float TimeOfCreation = 0.0f;

    // Конструкторы
    FAlchemyAtom();

    FAlchemyAtom(FName InSourceID, bool bInIsWater, const FRealState& InState,
                 EIngredientClass InClass, EAtomOrigin InOrigin,
                 float InDistortionAtCollection, float InTimeOfCreation);
};