#include "AlchemySubsystem.h"
#include "Core/Data/IngredientRegistry.h"
#include "Core/Data/WaterTypeRegistry.h"
#include "Engine/DataTable.h"
#include "ProjectHerbalist.h"

void UAlchemySubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    UE_LOG(LogHerbalist, Log, TEXT("[Herbalist] UAlchemySubsystem::Initialize"));

    // Ingredient Registry
    UDataTable* IngredientTable = LoadObject<UDataTable>(nullptr, IngredientTablePath);
    if (!IngredientTable) { UE_LOG(LogHerbalist, Error, TEXT("Failed to load DT_IngredientClass")); }
    if (!IngredientTable)
    {
        UE_LOG(LogHerbalist, Error, TEXT("[Herbalist] Failed to load DT_IngredientClass"));
    }
    FIngredientRegistry::Initialize(IngredientTable);

    // Water Type Registry
    UDataTable* WaterTypeTable = LoadObject<UDataTable>(nullptr, WaterTypeTablePath);
    if (!WaterTypeTable)
    {
        UE_LOG(LogHerbalist, Error, TEXT("[Herbalist] Failed to load DT_WaterTypes"));
    }
    FWaterTypeRegistry::Initialize(WaterTypeTable);
}

void UAlchemySubsystem::Deinitialize()
{
    UE_LOG(LogHerbalist, Log, TEXT("[Herbalist] UAlchemySubsystem::Deinitialize"));
    FIngredientRegistry::Reset();
    FWaterTypeRegistry::Reset();
    Super::Deinitialize();
}
