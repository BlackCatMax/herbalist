#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Core/Types/HerbalistCoreTypes.h"
#include "WaterTypeRow.generated.h"

UENUM()
enum class EWaterSpecialEffect : uint8
{
    None        UMETA(DisplayName = "None"),
    Cleanse     UMETA(DisplayName = "Cleanse"),
    Corrupt     UMETA(DisplayName = "Corrupt"),
    Amplify     UMETA(DisplayName = "Amplify"),
    Stabilize   UMETA(DisplayName = "Stabilize")
};

USTRUCT(BlueprintType)
struct PROJECTHERBALIST_API FWaterTypeRow : public FTableRowBase
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Water")
    FName WaterTypeID;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Water")
    FText DisplayName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Water")
    TArray<EBiomeType> AllowedBiomes;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float BasePurity = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float BaseDistortion = 0.3f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float BaseStability = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float BasePotency = 0.3f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float BaseCorruption = 0.1f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Water")
    EWaterSpecialEffect SpecialEffect = EWaterSpecialEffect::None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Water", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float Rarity = 1.0f;
};
