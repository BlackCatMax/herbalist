// ProjectHerbalistGameModeBase.cpp
#include "ProjectHerbalistGameModeBase.h"
#include "ProjectHerbalist.h"
#include "Player/HerbalistPlayerController.h"
#include "Core/BiomeGraph/BiomeGraphSubsystem.h"
#include "Core/BiomeGraph/BiomeGraphAsset.h"
#include "Core/Subsystems/IngredientRegistrySubsystem.h"
#include "Core/Subsystems/WaterTypeRegistrySubsystem.h"
#include "Core/Types/BiomeTypes.h"
#include "Core/Entities/AmbientEntityTypes.h"
#include "Core/Entities/LandmarkTypes.h"
#include "Core/Entities/LegendaryEntityTypes.h"
#include "Core/Entities/ArtifactTypes.h"
#include "Core/Dialogue/HerbalistDialogueTypes.h"
#include "Core/Zaryana/MemoryFragmentDefinitions.h"
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

    // Прогрев кэша бестиария Низшего ранга (2026-09-02, миграция на
    // DataTable) -- необязательно для работы (GetAmbientEntityDefinitions()
    // сама лениво грузит DT_AmbientEntities при первом обращении, см.
    // AmbientEntityTypes.h), но так пропавший ассет громко ловится на
    // старте игры, а не на первом тике UpdateEntityManifestations, и это
    // единообразно с остальными тремя таблицами выше.
    if (GetAmbientEntityDefinitions().Num() == 0)
    {
        UE_LOG(LogHerbalist, Error, TEXT("GetAmbientEntityDefinitions() вернул пустой реестр -- DT_AmbientEntities не найден или пуст, Низший ранг бестиария не будет проявляться."));
    }

    // Тот же прогрев кэша, что и у Низшего ранга выше (2026-09-02, юнит 2/3).
    if (GetLandmarkDefinitions().Num() == 0)
    {
        UE_LOG(LogHerbalist, Error, TEXT("GetLandmarkDefinitions() вернул пустой реестр -- DT_Landmarks не найден или пуст, Основной ранг бестиария (хозяева) не будет проявляться."));
    }

    // Тот же прогрев кэша, юнит 3/3 (последний). Берегиня не входит --
    // отдельный, per-клеточный путь, не в этом реестре.
    if (GetLegendaryEntityDefinitions().Num() == 0)
    {
        UE_LOG(LogHerbalist, Error, TEXT("GetLegendaryEntityDefinitions() вернул пустой реестр -- DT_LegendaryEntities не найден или пуст, Легендарный ранг бестиария (кроме Берегини) не будет проявляться."));
    }

    // Тот же прогрев кэша, Units 4-6 (2026-09-02): артефакты, диалоги,
    // фрагменты памяти Зарянки -- тот же паттерн, что бестиарий выше.
    if (GetArtifactDefinitions().Num() == 0)
    {
        UE_LOG(LogHerbalist, Error, TEXT("GetArtifactDefinitions() вернул пустой реестр -- DT_Artifacts не найден или пуст, артефакты Легендарных сущностей не будут доступны."));
    }
    if (GetDialogueDefinitions().Num() == 0)
    {
        UE_LOG(LogHerbalist, Error, TEXT("GetDialogueDefinitions() вернул пустой реестр -- DT_Dialogue не найден или пуст, диалоговые реплики не будут доступны."));
    }
    if (HerbalistCore::Zaryana::GetAllMemoryFragmentDefinitions().Num() == 0)
    {
        UE_LOG(LogHerbalist, Error, TEXT("GetAllMemoryFragmentDefinitions() вернул пустой реестр -- DT_MemoryFragments не найден или пуст, фрагменты памяти Зарянки не будут доступны."));
    }
}