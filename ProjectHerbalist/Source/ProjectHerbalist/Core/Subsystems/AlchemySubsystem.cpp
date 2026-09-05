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
        // Не ошибка, а нормальный порядок инициализации: UAlchemySubsystem
        // САМ является UGameInstanceSubsystem (не подсистемой мира, как
        // утверждала более ранняя версия этого комментария) -- порядок
        // Initialize() между подсистемами-сиблингами одного GameInstance не
        // гарантирован движком без явного Collection.InitializeDependency<...>(),
        // которого здесь нет. До 2026-09-03 это писалось как Error и
        // выглядело причиной поломок, которых тут нет: реестр всё равно
        // загрузится сам при первом чтении (EnsureLoaded), а раньше --
        // AProjectHerbalistGameModeBase::BeginPlay.
        UE_LOG(LogHerbalistAlchemy, Log, TEXT("[Herbalist] UIngredientRegistrySubsystem ещё недоступен (мировая подсистема стартует раньше GameInstance) -- реестр загрузится сам при первом чтении"));
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
        UE_LOG(LogHerbalistAlchemy, Log, TEXT("[Herbalist] UWaterTypeRegistrySubsystem ещё недоступен -- см. довод выше, реестр самозагрузится"));
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