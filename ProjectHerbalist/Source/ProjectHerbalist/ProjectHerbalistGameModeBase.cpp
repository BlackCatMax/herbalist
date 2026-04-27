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

AProjectHerbalistGameModeBase::AProjectHerbalistGameModeBase()
{
    PlayerControllerClass = AHerbalistPlayerController::StaticClass();
}

void AProjectHerbalistGameModeBase::BeginPlay()
{
    // Загрузка реестров ДО всего остального
    if (UGameInstance* GameInstance = GetGameInstance())
    {
        if (UIngredientRegistrySubsystem* IngredientSubsystem = GameInstance->GetSubsystem<UIngredientRegistrySubsystem>())
        {
            UDataTable* IngredientTable = LoadObject<UDataTable>(nullptr, TEXT("/Game/Herbalist/Data/DT_IngredientClass"));
            if (IngredientTable)
            {
                IngredientSubsystem->LoadFromDataTable(IngredientTable);
            }
        }

        if (UWaterTypeRegistrySubsystem* WaterSubsystem = GameInstance->GetSubsystem<UWaterTypeRegistrySubsystem>())
        {
            UDataTable* WaterTypeTable = LoadObject<UDataTable>(nullptr, TEXT("/Game/Herbalist/Data/DT_WaterTypes"));
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
            UBiomeGraphAsset* Asset = LoadObject<UBiomeGraphAsset>(nullptr, TEXT("/Game/Data/DA_BiomeGraph"));
            if (Asset)
            {
                Graph->InitializeFromAsset(Asset);
                UE_LOG(LogHerbalist, Log, TEXT("BiomeGraph initialized from DA_BiomeGraph"));
            }
            else
            {
                UE_LOG(LogHerbalist, Warning, TEXT("DA_BiomeGraph not found at /Game/Data/DA_BiomeGraph"));
            }
        }
    }

    UDataTable* BiomeTable = LoadObject<UDataTable>(nullptr, TEXT("/Game/Data/DT_BiomeDefaults"));
    if (BiomeTable)
    {
        FBiomeDefaults::SetBiomeTable(BiomeTable);
        UE_LOG(LogHerbalist, Log, TEXT("Biome table initialized successfully."));
    }
    else
    {
        UE_LOG(LogHerbalist, Error, TEXT("Failed to load DT_BiomeDefaults! Biomes will not work correctly."));
    }
}