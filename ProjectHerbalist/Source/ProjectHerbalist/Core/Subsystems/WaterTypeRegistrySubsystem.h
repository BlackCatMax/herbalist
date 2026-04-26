#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Core/Data/WaterTypeRow.h"
#include "Math/RandomStream.h"
#include "WaterTypeRegistrySubsystem.generated.h"

UCLASS()
class PROJECTHERBALIST_API UWaterTypeRegistrySubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    void LoadFromDataTable(UDataTable* WaterTypeTable);

    const FWaterTypeRow* GetWaterType(FName WaterTypeID) const;
    FName GetRandomWaterType(EBiomeType Biome, FRandomStream& Rng) const;
    bool IsValidWaterType(FName WaterTypeID) const;
    TArray<FName> GetWaterTypesForBiome(EBiomeType Biome) const;
    int32 GetWaterTypeCount() const;
    void Reset();

private:
    TMap<FName, FWaterTypeRow> WaterTypeMap;
    bool bInitialized = false;

    // Кэш для быстрого доступа
    TMap<EBiomeType, TArray<FName>> CachedWaterTypesByBiome;
    TMap<EBiomeType, TArray<float>> CachedRarityByBiome;

    void BuildCache();
};