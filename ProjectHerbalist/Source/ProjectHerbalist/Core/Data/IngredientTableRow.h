#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "IngredientTableRow.generated.h"

UENUM()
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

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Herbalist")
    EIngredientClass Class = EIngredientClass::Unknown;
};