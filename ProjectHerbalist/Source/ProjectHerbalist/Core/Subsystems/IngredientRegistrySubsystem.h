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

    // Пригодность (DESIGN_World_State.md §15/§16, звенья 3 и 8): AllowedBiomes
    // уже решил, КТО может расти здесь (кэш CachedResourcesByBiome) — Cell и
    // HarvestContext решают, СКОЛЬКО шансов у каждого кандидата: близость к
    // BaseState (звено 3) умножается на (1 − Cell.HarvestStress) и на
    // СезонноеОкно×ВременноеОкно×ЛунноеОкно×ПогодноеОкно (звено 8), каждое —
    // мягкий гейт (IngredientWindowMismatchMultiplier), не hard-фильтр. Cell
    // целиком, не Biome+State по отдельности — ради HarvestStress.
    FName GetRandomResourceForBiome(const FGridCell& Cell, const FHarvestContext& Context, FRandomStream& Rng) const;

    // Пристройка сада (DESIGN_Community_And_Homestead.md §2.4, 2026-08-31) —
    // тот же вопрос "сколько шансов у кого" (близость State/сезон/окна), но
    // кандидаты берутся из EGardenNiche ингредиента, не из AllowedBiomes
    // клетки: постройка физически подделывает нишу, не переносит биом
    // целиком. Пусто/None -> нет кандидатов, вызывающая сторона (SpawnResourcesInCell)
    // должна сама решать, откатываться ли на GetRandomResourceForBiome.
    FName GetRandomResourceForNiche(const FGridCell& Cell, EGardenNiche Niche, const FHarvestContext& Context, FRandomStream& Rng) const;

    void Reset();

private:
    TMap<FName, FIngredientTableRow> Rows;
    bool bInitialized = false;

    TMap<EBiomeType, TArray<FName>> CachedResourcesByBiome;
    TMap<EBiomeType, TArray<int32>> CachedWeightsByBiome;

    TMap<EGardenNiche, TArray<FName>> CachedResourcesByNiche;
    TMap<EGardenNiche, TArray<int32>> CachedWeightsByNiche;

    void BuildCache();

    // Общая взвешенная выборка (DESIGN_World_State.md §15/§16, звенья 3 и 8)
    // — вынесена из GetRandomResourceForBiome, чтобы GetRandomResourceForNiche
    // не дублировал ту же формулу (Suitability по State + сезон/время/луна/
    // погода) над другим списком кандидатов.
    //
    // BaseWeights — TArray<float>, не int32 (правка 2026-08-31, PCG-биомы):
    // GetRandomResourceForBiome домножает базовый вес карточки на долю
    // клетки в биоме (Cell.BiomeWeights, может быть 0.5/0.33/...) до
    // вызова этой функции — при исходном int32 умножение "1 (дефолтный
    // RarityWeight) * 0.5" усекалось бы до 0, ингредиент с обычным весом
    // становился бы невыбираемым на любом стыке двух регионов, без единой
    // ошибки в логе. Сама функция ниже не меняется — она уже кастовала
    // BaseWeights[i] в float на каждой итерации.
    FName PickWeightedResource(const TArray<FName>& Candidates, const TArray<float>& BaseWeights,
        const FGridCell& Cell, const FHarvestContext& Context, FRandomStream& Rng) const;
};