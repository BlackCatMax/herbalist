// AlchemySubsystem.cpp
#include "AlchemySubsystem.h"
#include "Core/Subsystems/IngredientRegistrySubsystem.h"
#include "Core/Subsystems/WaterTypeRegistrySubsystem.h"
#include "Engine/DataTable.h"
#include "Engine/GameInstance.h"
#include "ProjectHerbalist.h"
#include "HerbalistLogChannels.h"

void UAlchemySubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    UE_LOG(LogHerbalistAlchemy, Log, TEXT("[Herbalist] UAlchemySubsystem::Initialize"));

    UGameInstance* GameInstance = GetGameInstance();
    if (!GameInstance)
    {
        UE_LOG(LogHerbalistAlchemy, Error, TEXT("[Herbalist] AlchemySubsystem: GetGameInstance() returned null"));
        return;
    }

    UIngredientRegistrySubsystem* IngredientSubsystem = GameInstance->GetSubsystem<UIngredientRegistrySubsystem>();
    if (!IngredientSubsystem)
    {
        UE_LOG(LogHerbalistAlchemy, Error, TEXT("[Herbalist] UIngredientRegistrySubsystem not found"));
    }
    else
    {
        UDataTable* IngredientTable = LoadObject<UDataTable>(nullptr, IngredientTablePath);
        if (!IngredientTable)
        {
            UE_LOG(LogHerbalistAlchemy, Error, TEXT("[Herbalist] Failed to load DT_IngredientClass from path: %s"), IngredientTablePath);
        }
        else
        {
            UE_LOG(LogHerbalistAlchemy, Log, TEXT("[Herbalist] DT_IngredientClass loaded, row count: %d"), IngredientTable->GetRowMap().Num());
            IngredientSubsystem->LoadFromDataTable(IngredientTable);
        }
    }

    UWaterTypeRegistrySubsystem* WaterSubsystem = GameInstance->GetSubsystem<UWaterTypeRegistrySubsystem>();
    if (!WaterSubsystem)
    {
        UE_LOG(LogHerbalistAlchemy, Error, TEXT("[Herbalist] UWaterTypeRegistrySubsystem not found"));
    }
    else
    {
        UDataTable* WaterTypeTable = LoadObject<UDataTable>(nullptr, WaterTypeTablePath);
        if (!WaterTypeTable)
        {
            UE_LOG(LogHerbalistAlchemy, Error, TEXT("[Herbalist] Failed to load DT_WaterTypes from path: %s"), WaterTypeTablePath);
        }
        else
        {
            UE_LOG(LogHerbalistAlchemy, Log, TEXT("[Herbalist] DT_WaterTypes loaded, row count: %d"), WaterTypeTable->GetRowMap().Num());
            WaterSubsystem->LoadFromDataTable(WaterTypeTable);
        }
    }
}

void UAlchemySubsystem::Deinitialize()
{
    UE_LOG(LogHerbalistAlchemy, Log, TEXT("[Herbalist] UAlchemySubsystem::Deinitialize"));

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