#include "AlchemySubsystem.h"
#include "Core/Data/IngredientRegistry.h"
#include "Engine/DataTable.h"

void UAlchemySubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    UE_LOG(LogTemp, Log, TEXT("[Herbalist] UAlchemySubsystem::Initialize — initializing ingredient registry"));

    UDataTable* IngredientTable = LoadObject<UDataTable>(nullptr, IngredientTablePath);

    if (!IngredientTable)
    {
        UE_LOG(LogTemp, Error, TEXT("[Herbalist] Failed to load ingredient DataTable from path: %s. All ingredients will be Unknown."), IngredientTablePath);
    }

    FIngredientRegistry::Initialize(IngredientTable);
}

void UAlchemySubsystem::Deinitialize()
{
    UE_LOG(LogTemp, Log, TEXT("[Herbalist] UAlchemySubsystem::Deinitialize"));
    FIngredientRegistry::Reset();
    Super::Deinitialize();
}