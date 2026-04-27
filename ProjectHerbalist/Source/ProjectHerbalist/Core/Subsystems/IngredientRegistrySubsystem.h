#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Core/Data/IngredientTableRow.h"
#include "Math/RandomStream.h"
#include "IngredientRegistrySubsystem.generated.h"

UCLASS()
class PROJECTHERBALIST_API UIngredientRegistrySubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    // Загрузка из DataTable
    void LoadFromDataTable(UDataTable* IngredientTable);

    // Доступ к данным
    const FIngredientTableRow* GetRow(FName IngredientID) const;
    EIngredientClass Classify(FName IngredientID) const;
    bool IsWater(FName IngredientID) const;
    bool IsKnown(FName IngredientID) const;

    // Спавн ресурсов (с кэшированием по биомам)
    TArray<FName> GetResourcesForBiome(EBiomeType Biome) const;
    FName GetRandomResourceForBiome(EBiomeType Biome, FRandomStream& Rng) const;

    // Сброс
    void Reset();

private:
    TMap<FName, FIngredientTableRow> Rows;
    bool bInitialized = false;

    // Кэшированные списки для быстрого доступа (пункт 3.3)
    TMap<EBiomeType, TArray<FName>> CachedResourcesByBiome;
    TMap<EBiomeType, TArray<int32>> CachedWeightsByBiome;

    void BuildCache();
};