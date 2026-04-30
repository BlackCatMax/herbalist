#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "Core/HerbalistAssetCatalog.h"
#include "HerbalistDeveloperSettings.generated.h"

UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="Herbalist Settings"))
class PROJECTHERBALIST_API UHerbalistDeveloperSettings : public UDeveloperSettings
{
    GENERATED_BODY()

public:
    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="Herbalist|Assets")
    TSoftObjectPtr<UHerbalistAssetCatalog> AssetCatalog;

    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="Herbalist|Debug")
    bool bEnableSimulationLogs = false;
};
