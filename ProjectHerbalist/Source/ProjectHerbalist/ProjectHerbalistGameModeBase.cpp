// ProjectHerbalistGameModeBase.cpp
#include "ProjectHerbalistGameModeBase.h"
#include "ProjectHerbalist.h"
#include "Player/HerbalistPlayerController.h"
#include "Core/World/GridWorldManager.h"
#include "Core/BiomeGraph/BiomeGraphSubsystem.h"
#include "Core/BiomeGraph/BiomeGraphAsset.h"
#include "Core/Types/BiomeTypes.h"
#include "Core/Data/IngredientRegistry.h"
#include "Engine/World.h"
#include "Engine/DataTable.h"

AProjectHerbalistGameModeBase::AProjectHerbalistGameModeBase()
{
    PlayerControllerClass = AHerbalistPlayerController::StaticClass();
}

AProjectHerbalistGameModeBase::~AProjectHerbalistGameModeBase()
{
}

void AProjectHerbalistGameModeBase::BeginPlay()
{
    Super::BeginPlay();

    // Инициализация статического реестра ингредиентов (должна быть до любого использования)
    FIngredientRegistry::Initialize();

    // Инициализация Biome Graph
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

    // Инициализация таблицы биомов
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

    SpawnWorldManager();
}

void AProjectHerbalistGameModeBase::SpawnWorldManager()
{
    if (!WorldManager)
    {
        FActorSpawnParameters SpawnParams;
        SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        WorldManager = GetWorld()->SpawnActor<AGridWorldManager>(SpawnParams);
        if (WorldManager)
        {
            WorldManager->StateInterpolationSpeed = StateInterpolationSpeed;
            WorldManager->bHarvestAffectsBiome = bHarvestAffectsBiome;
            WorldManager->HarvestStressIncrement = HarvestStressIncrement;
            WorldManager->HarvestStressThreshold = HarvestStressThreshold;
            WorldManager->MaxHarvestImpactOnDistortion = MaxHarvestImpactOnDistortion;
            WorldManager->MaxHarvestImpactOnMagnitude = MaxHarvestImpactOnMagnitude;
            WorldManager->HarvestStressDecayRate = HarvestStressDecayRate;
            WorldManager->bEnableRecovery = bEnableEcologyRecovery;

            UE_LOG(LogHerbalist, Log, TEXT("WorldManager spawned with interpolation speed %.3f"), StateInterpolationSpeed);
        }
    }
}