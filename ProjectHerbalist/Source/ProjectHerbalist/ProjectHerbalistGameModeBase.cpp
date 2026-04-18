// ProjectHerbalistGameModeBase.cpp
#include "ProjectHerbalistGameModeBase.h"
#include "ProjectHerbalist.h"
#include "Player/HerbalistPlayerController.h"
#include "Core/World/GridWorldManager.h"
#include "Core/Data/ResourceDataManager.h"
#include "Engine/World.h"

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

    // Инициализация ResourceDataManager
    UDataTable* ResourceBalanceTable = LoadObject<UDataTable>(nullptr, TEXT("/Game/Data/DT_ResourceBalance"));
    UDataTable* BiomeTable = LoadObject<UDataTable>(nullptr, TEXT("/Game/Data/DT_BiomeDefaults"));
    UDataTable* WaterTable = LoadObject<UDataTable>(nullptr, TEXT("/Game/Data/DT_WaterTypes")); // опционально

    if (!ResourceBalanceTable || !BiomeTable)
    {
        UE_LOG(LogHerbalist, Error, TEXT("Failed to load required DataTables! Game may not work correctly."));
    }
    else
    {
        UResourceDataManager* Manager = NewObject<UResourceDataManager>(this);
        Manager->Initialize(ResourceBalanceTable, BiomeTable, WaterTable);
        UE_LOG(LogHerbalist, Log, TEXT("ResourceDataManager initialized successfully."));
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