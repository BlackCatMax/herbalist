#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Core/Types/HerbalistCoreTypes.h"
#include "ItemTooltipWidget.generated.h"

class UTextBlock;

UCLASS()
class PROJECTHERBALIST_API UItemTooltipWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    void SetItem(const FInventoryItem& Item, float GlobalDistortion);

protected:
    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> NameText;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> TypeText;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> MagnitudeText;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> DistortionText;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> PurityText;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> StabilityText;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> PotencyText;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> ResonanceText;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> CorruptionText;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> DirectionBodyText;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> DirectionMindText;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> DirectionSpiritText;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> DirectionNatureText;
};
