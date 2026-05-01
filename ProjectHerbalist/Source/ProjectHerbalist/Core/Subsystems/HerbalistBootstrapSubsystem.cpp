#include "Core/Subsystems/HerbalistBootstrapSubsystem.h"
#include "Core/HerbalistDeveloperSettings.h"
#include "Core/Subsystems/IngredientRegistrySubsystem.h"
#include "Core/Subsystems/WaterTypeRegistrySubsystem.h"
#include "ProjectHerbalist.h"
#include "Core/World/WorldStateSubsystem.h"
#include "Core/HerbalistSettings.h"
#include "Engine/World.h"

void UHerbalistBootstrapSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    if (!LoadCatalog())
    {
        UE_LOG(LogHerbalist, Error, TEXT("Bootstrap: failed to load asset catalog"));
        return;
    }

    InitializeRegistries();

    if (UWorld* World = GetWorld())
    {
        if (UWorldStateSubsystem* WorldState = World->GetSubsystem<UWorldStateSubsystem>())
        {
            const UHerbalistSettings* HerbalistSettings = GetDefault<UHerbalistSettings>();
            WorldState->InitializeGrid(
                HerbalistSettings ? HerbalistSettings->WorldGridSizeX : 20,
                HerbalistSettings ? HerbalistSettings->WorldGridSizeY : 20,
                HerbalistSettings ? HerbalistSettings->WorldCellSize : 100.0f);

            if (HerbalistSettings)
            {
                WorldState->SetBiomeMaskTexture(HerbalistSettings->WorldBiomeMaskTexture.LoadSynchronous());
            }
        }
    }
    bBootstrapped = true;
}

bool UHerbalistBootstrapSubsystem::LoadCatalog()
{
    const UHerbalistDeveloperSettings* Settings = GetDefault<UHerbalistDeveloperSettings>();
    if (!Settings || Settings->AssetCatalog.IsNull())
    {
        UE_LOG(LogHerbalist, Error, TEXT("Bootstrap: AssetCatalog is not set in DeveloperSettings"));
        return false;
    }

    LoadedCatalog = Settings->AssetCatalog.LoadSynchronous();
    return LoadedCatalog != nullptr;
}

void UHerbalistBootstrapSubsystem::InitializeRegistries()
{
    if (!LoadedCatalog) return;

    if (UIngredientRegistrySubsystem* Ingredient = GetGameInstance()->GetSubsystem<UIngredientRegistrySubsystem>())
    {
        if (UDataTable* IngredientTable = LoadedCatalog->IngredientTable.LoadSynchronous())
        {
            Ingredient->LoadFromDataTable(IngredientTable);
        }
    }

    if (UWaterTypeRegistrySubsystem* Water = GetGameInstance()->GetSubsystem<UWaterTypeRegistrySubsystem>())
    {
        if (UDataTable* WaterTable = LoadedCatalog->WaterTypeTable.LoadSynchronous())
        {
            Water->LoadFromDataTable(WaterTable);
        }
    }
}
