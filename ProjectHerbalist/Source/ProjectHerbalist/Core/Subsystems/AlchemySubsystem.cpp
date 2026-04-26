#include "AlchemySubsystem.h"
#include "Core/Subsystems/IngredientRegistrySubsystem.h"
#include "Core/Subsystems/WaterTypeRegistrySubsystem.h"
#include "Engine/DataTable.h"
#include "ProjectHerbalist.h"

void UAlchemySubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    UE_LOG(LogHerbalist, Log, TEXT("[Herbalist] UAlchemySubsystem::Initialize"));

    UGameInstance* GameInstance = GetGameInstance();
    if (!GameInstance) return;

    UIngredientRegistrySubsystem* IngredientSub = GameInstance->GetSubsystem<UIngredientRegistrySubsystem>();
    UWaterTypeRegistrySubsystem* WaterSub = GameInstance->GetSubsystem<UWaterTypeRegistrySubsystem>();

    UDataTable* IngredientTable = LoadObject<UDataTable>(nullptr, IngredientTablePath);
    if (IngredientTable && IngredientSub)
    {
        IngredientSub->LoadFromDataTable(IngredientTable);
    }
    else
    {
        UE_LOG(LogHerbalist, Error, TEXT("Failed to load DT_IngredientClass or subsystem missing"));
    }

    UDataTable* WaterTypeTable = LoadObject<UDataTable>(nullptr, WaterTypeTablePath);
    if (WaterTypeTable && WaterSub)
    {
        WaterSub->LoadFromDataTable(WaterTypeTable);
    }
    else
    {
        UE_LOG(LogHerbalist, Error, TEXT("Failed to load DT_WaterTypes or subsystem missing"));
    }
}

void UAlchemySubsystem::Deinitialize()
{
    UE_LOG(LogHerbalist, Log, TEXT("[Herbalist] UAlchemySubsystem::Deinitialize"));
    // Подсистемы сами сбросятся при уничтожении GameInstance
    Super::Deinitialize();
}