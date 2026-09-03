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

    // Единственный литерал пути боевой таблицы -- см. одноимённое поле у
    // UIngredientRegistrySubsystem.
    static constexpr const TCHAR* DefaultTablePath = TEXT("/Game/Herbalist/Data/DT_WaterTypes.DT_WaterTypes");

    const FWaterTypeRow* GetWaterType(FName WaterTypeID) const;
    FName GetRandomWaterType(EBiomeType Biome, FRandomStream& Rng) const;
    bool IsValidWaterType(FName WaterTypeID) const;
    TArray<FName> GetWaterTypesForBiome(EBiomeType Biome) const;
    int32 GetWaterTypeCount() const;
    void Reset();

private:
    // Ленивая самозагрузка (2026-09-03) -- ровно тот же баг и то же
    // лекарство, что у UIngredientRegistrySubsystem::EnsureLoaded: читатель,
    // пришедший раньше AProjectHerbalistGameModeBase::BeginPlay, получал
    // пустой реестр вместо содержимого таблицы. Подробности и разбор
    // PIE-лога -- в комментарии там.
    void EnsureLoaded() const;

    TMap<FName, FWaterTypeRow> WaterTypeMap;
    bool bInitialized = false;

    // Одна попытка загрузки на время жизни объекта -- см. одноимённое поле
    // и его обоснование у UIngredientRegistrySubsystem.
    mutable bool bLoadAttempted = false;

    // Кэш для быстрого доступа
    TMap<EBiomeType, TArray<FName>> CachedWaterTypesByBiome;
    TMap<EBiomeType, TArray<float>> CachedRarityByBiome;

    void BuildCache();
};