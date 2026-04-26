#include "AlchemySubsystem.h"
#include "Core/Data/IngredientRegistry.h"
#include "Core/Data/WaterTypeRegistry.h"
#include "Engine/DataTable.h"
#include "ProjectHerbalist.h"

// Пути к таблицам (должны совпадать с вашими)
static constexpr const TCHAR* IngredientTablePath = TEXT("/Game/Herbalist/Data/DT_IngredientClass.DT_IngredientClass");
static constexpr const TCHAR* WaterTypeTablePath = TEXT("/Game/Herbalist/Data/DT_WaterTypes.DT_WaterTypes");

void UAlchemySubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    UE_LOG(LogHerbalist, Log, TEXT("[Herbalist] UAlchemySubsystem::Initialize"));

    UDataTable* IngredientTable = LoadObject<UDataTable>(nullptr, IngredientTablePath);
    if (!IngredientTable)
    {
        UE_LOG(LogHerbalist, Error, TEXT("[Herbalist] Failed to load DT_IngredientClass. Check path: %s"), IngredientTablePath);
    }
    else
    {
        FIngredientRegistry::Initialize(IngredientTable);
    }

    UDataTable* WaterTypeTable = LoadObject<UDataTable>(nullptr, WaterTypeTablePath);
    if (!WaterTypeTable)
    {
        UE_LOG(LogHerbalist, Error, TEXT("[Herbalist] Failed to load DT_WaterTypes. Check path: %s"), WaterTypeTablePath);
    }
    else
    {
        FWaterTypeRegistry::Initialize(WaterTypeTable);
    }
}

void UAlchemySubsystem::Deinitialize()
{
    UE_LOG(LogHerbalist, Log, TEXT("[Herbalist] UAlchemySubsystem::Deinitialize"));
    FIngredientRegistry::Reset();
    FWaterTypeRegistry::Reset();
    Super::Deinitialize();
}