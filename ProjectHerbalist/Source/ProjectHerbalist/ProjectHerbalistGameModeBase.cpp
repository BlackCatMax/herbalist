// ProjectHerbalistGameModeBase.cpp
#include "ProjectHerbalistGameModeBase.h"
#include "ProjectHerbalist.h"
#include "Player/HerbalistPlayerController.h"
#include "Core/BiomeGraph/BiomeGraphSubsystem.h"
#include "Core/BiomeGraph/BiomeGraphAsset.h"
#include "Core/Subsystems/IngredientRegistrySubsystem.h"
#include "Core/Subsystems/WaterTypeRegistrySubsystem.h"
#include "Core/Types/BiomeTypes.h"
#include "Engine/DataTable.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Core/HerbalistSettings.h"

AProjectHerbalistGameModeBase::AProjectHerbalistGameModeBase()
{
    PlayerControllerClass = AHerbalistPlayerController::StaticClass();
}

void AProjectHerbalistGameModeBase::BeginPlay()
{
    const UHerbalistSettings* Settings = GetDefault<UHerbalistSettings>();

    // Загрузка реестров ДО всего остального
    if (UGameInstance* GameInstance = GetGameInstance())
    {
        if (UIngredientRegistrySubsystem* IngredientSubsystem = GameInstance->GetSubsystem<UIngredientRegistrySubsystem>())
        {
            UDataTable* IngredientTable = Settings ? Settings->IngredientTableAsset.LoadSynchronous() : nullptr;
            if (IngredientTable)
            {
                IngredientSubsystem->LoadFromDataTable(IngredientTable);
            }
        }

        if (UWaterTypeRegistrySubsystem* WaterSubsystem = GameInstance->GetSubsystem<UWaterTypeRegistrySubsystem>())
        {
            UDataTable* WaterTypeTable = Settings ? Settings->WaterTypeTableAsset.LoadSynchronous() : nullptr;
            if (WaterTypeTable)
            {
                WaterSubsystem->LoadFromDataTable(WaterTypeTable);
            }
        }
    }

    Super::BeginPlay();

    if (UBiomeGraphSubsystem* Graph = GetWorld()->GetSubsystem<UBiomeGraphSubsystem>())
    {
        if (!Graph->IsInitialized())
        {
            UBiomeGraphAsset* Asset = Settings ? Settings->BiomeGraphAsset.LoadSynchronous() : nullptr;
            if (Asset)
            {
                Graph->InitializeFromAsset(Asset);
                UE_LOG(LogHerbalist, Log, TEXT("BiomeGraph initialized from DA_BiomeGraph"));
            }
            else
            {
                UE_LOG(LogHerbalist, Warning, TEXT("BiomeGraph asset is not set or failed to load in Herbalist Settings"));
            }
        }
    }

    UDataTable* BiomeTable = Settings ? Settings->BiomeDefaultsTableAsset.LoadSynchronous() : nullptr;
    if (BiomeTable)
    {
        FBiomeDefaults::SetBiomeTable(BiomeTable);
        UE_LOG(LogHerbalist, Log, TEXT("Biome table initialized successfully."));
    }
    else
    {
        UE_LOG(LogHerbalist, Error, TEXT("Failed to load BiomeDefaultsTableAsset from Herbalist Settings! Biomes will not work correctly."));
    }
}