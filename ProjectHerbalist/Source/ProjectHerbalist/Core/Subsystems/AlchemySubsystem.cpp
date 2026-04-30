// AlchemySubsystem.cpp
#include "AlchemySubsystem.h"
#include "Core/Subsystems/IngredientRegistrySubsystem.h"
#include "Core/Subsystems/WaterTypeRegistrySubsystem.h"
#include "Engine/DataTable.h"
#include "Engine/GameInstance.h"
#include "ProjectHerbalist.h"
#include "Core/HerbalistSettings.h"

void UAlchemySubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    UE_LOG(LogHerbalist, Log, TEXT("[Herbalist] UAlchemySubsystem::Initialize"));

    UGameInstance* GameInstance = GetGameInstance();
    if (!GameInstance)
    {
        UE_LOG(LogHerbalist, Error, TEXT("[Herbalist] AlchemySubsystem: GetGameInstance() returned null"));
        return;
    }

    UIngredientRegistrySubsystem* IngredientSubsystem = GameInstance->GetSubsystem<UIngredientRegistrySubsystem>();
    if (!IngredientSubsystem)
    {
        UE_LOG(LogHerbalist, Error, TEXT("[Herbalist] UIngredientRegistrySubsystem not found"));
    }
    else
    {
        const UHerbalistSettings* Settings = GetDefault<UHerbalistSettings>();
        UDataTable* IngredientTable = Settings ? Settings->IngredientTableAsset.LoadSynchronous() : nullptr;
        if (!IngredientTable)
        {
            UE_LOG(LogHerbalist, Error, TEXT("[Herbalist] Failed to load IngredientTableAsset from Herbalist Settings"));
        }
        else
        {
            UE_LOG(LogHerbalist, Log, TEXT("[Herbalist] DT_IngredientClass loaded, row count: %d"), IngredientTable->GetRowMap().Num());
            IngredientSubsystem->LoadFromDataTable(IngredientTable);
        }
    }

    UWaterTypeRegistrySubsystem* WaterSubsystem = GameInstance->GetSubsystem<UWaterTypeRegistrySubsystem>();
    if (!WaterSubsystem)
    {
        UE_LOG(LogHerbalist, Error, TEXT("[Herbalist] UWaterTypeRegistrySubsystem not found"));
    }
    else
    {
        const UHerbalistSettings* Settings = GetDefault<UHerbalistSettings>();
        UDataTable* WaterTypeTable = Settings ? Settings->WaterTypeTableAsset.LoadSynchronous() : nullptr;
        if (!WaterTypeTable)
        {
            UE_LOG(LogHerbalist, Error, TEXT("[Herbalist] Failed to load WaterTypeTableAsset from Herbalist Settings"));
        }
        else
        {
            UE_LOG(LogHerbalist, Log, TEXT("[Herbalist] DT_WaterTypes loaded, row count: %d"), WaterTypeTable->GetRowMap().Num());
            WaterSubsystem->LoadFromDataTable(WaterTypeTable);
        }
    }
}

void UAlchemySubsystem::Deinitialize()
{
    UE_LOG(LogHerbalist, Log, TEXT("[Herbalist] UAlchemySubsystem::Deinitialize"));

    UGameInstance* GameInstance = GetGameInstance();
    if (GameInstance)
    {
        if (UIngredientRegistrySubsystem* IngredientSubsystem = GameInstance->GetSubsystem<UIngredientRegistrySubsystem>())
        {
            IngredientSubsystem->Reset();
        }
        if (UWaterTypeRegistrySubsystem* WaterSubsystem = GameInstance->GetSubsystem<UWaterTypeRegistrySubsystem>())
        {
            WaterSubsystem->Reset();
        }
    }

    Super::Deinitialize();
}