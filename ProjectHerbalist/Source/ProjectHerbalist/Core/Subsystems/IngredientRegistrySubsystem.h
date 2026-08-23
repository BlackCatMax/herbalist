#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Core/Data/IngredientTableRow.h"
#include "Core/Data/WaterTypeRow.h"  // для EWaterSpecialEffect и FWaterTypeRow
#include "Math/RandomStream.h"
#include "IngredientRegistrySubsystem.generated.h"

class UWaterTypeRegistrySubsystem;  // forward declaration, сам include не нужен в .h, но можно оставить

UCLASS()
class PROJECTHERBALIST_API UIngredientRegistrySubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    void LoadFromDataTable(UDataTable* IngredientTable);

    const FIngredientTableRow* GetRow(FName IngredientID) const;
    EIngredientClass Classify(FName IngredientID) const;
    bool IsWater(FName IngredientID) const;
    bool IsKnown(FName IngredientID) const;

    TArray<FName> GetResourcesForBiome(EBiomeType Biome) const;

    // Пригодность (DESIGN_World_State.md §15, звено 3): AllowedBiomes уже решил,
    // КТО может расти здесь (кэш CachedResourcesByBiome) — CellState решает,
    // СКОЛЬКО шансов у каждого кандидата, через близость к его BaseState.
    FName GetRandomResourceForBiome(EBiomeType Biome, const FRealState& CellState, FRandomStream& Rng) const;

    void Reset();

private:
    TMap<FName, FIngredientTableRow> Rows;
    bool bInitialized = false;

    TMap<EBiomeType, TArray<FName>> CachedResourcesByBiome;
    TMap<EBiomeType, TArray<int32>> CachedWeightsByBiome;

    void BuildCache();
};