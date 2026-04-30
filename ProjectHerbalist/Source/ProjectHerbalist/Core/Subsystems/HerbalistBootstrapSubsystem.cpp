#include "Core/Subsystems/HerbalistBootstrapSubsystem.h"
#include "Core/HerbalistDeveloperSettings.h"
#include "Core/Subsystems/IngredientRegistrySubsystem.h"
#include "Core/Subsystems/WaterTypeRegistrySubsystem.h"
#include "ProjectHerbalist.h"

void UHerbalistBootstrapSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    if (!LoadCatalog())
    {
        UE_LOG(LogHerbalist, Error, TEXT("Bootstrap: failed to load asset catalog"));
        return;
    }

    InitializeRegistries();
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
