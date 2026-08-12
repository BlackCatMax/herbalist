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
    // Item — уже искажённая (S_perceived) версия предмета, см.
    // AGridWorldManager::GetPerceivedInventory(). Виджет ничего сам не
    // искажает и не имеет доступа к реальному значению — по дизайну
    // (01_Introduction.md: игрок никогда не видит S_real напрямую).
    void SetItem(const FInventoryItem& Item);

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