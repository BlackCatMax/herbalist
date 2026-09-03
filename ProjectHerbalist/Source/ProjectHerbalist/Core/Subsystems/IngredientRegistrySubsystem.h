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

    // Явная загрузка. Вызывающих двое (AProjectHerbalistGameModeBase::
    // BeginPlay и UAlchemySubsystem::Initialize) плюс тесты, которые
    // подсовывают собственную таблицу. Первый победивший фиксирует
    // содержимое: повторный вызов -- no-op (см. bInitialized).
    void LoadFromDataTable(UDataTable* IngredientTable);

    // Путь боевой таблицы. Публичен ради EnsureLoaded ниже и ради
    // единственности литерала -- до 2026-09-03 он был написан трижды
    // (здесь, в GameMode и в AlchemySubsystem) в двух разных формах.
    static constexpr const TCHAR* DefaultTablePath = TEXT("/Game/Herbalist/Data/DT_IngredientClass.DT_IngredientClass");

    const FIngredientTableRow* GetRow(FName IngredientID) const;
    EIngredientClass Classify(FName IngredientID) const;
    bool IsWater(FName IngredientID) const;
    bool IsKnown(FName IngredientID) const;

    // Пригодность (DESIGN_World_State.md §15/§16, звенья 3 и 8): AllowedBiomes
    // уже решил, КТО может расти здесь (кэш CachedResourcesByBiome) — Cell и
    // HarvestContext решают, СКОЛЬКО шансов у каждого кандидата: близость к
    // BaseState (звено 3) умножается на (1 − Cell.HarvestStress) и на
    // СезонноеОкно×ВременноеОкно×ЛунноеОкно×ПогодноеОкно (звено 8), каждое —
    // мягкий гейт (IngredientWindowMismatchMultiplier), не hard-фильтр. Cell
    // целиком, не Biome+State по отдельности — ради HarvestStress.
    FName GetRandomResourceForBiome(const FGridCell& Cell, const FHarvestContext& Context, FRandomStream& Rng) const;

    // Водные растения (2026-09-02, прямой запрос пользователя) — тот же
    // поиск по Cell.BiomeWeights, что и GetRandomResourceForBiome выше, но
    // кандидаты берутся из отдельного кэша (только строки с bGrowsOnWater),
    // не смешиваются с обычным земляным пулом. Вызывается только для
    // Cell.bIsWater == true (см. AGridWorldManager::SpawnResourcesInCell).
    FName GetRandomResourceForAquaticBiome(const FGridCell& Cell, const FHarvestContext& Context, FRandomStream& Rng) const;

    // Пристройка сада (DESIGN_Community_And_Homestead.md §2.4, 2026-08-31) —
    // тот же вопрос "сколько шансов у кого" (близость State/сезон/окна), но
    // кандидаты берутся из EGardenNiche ингредиента, не из AllowedBiomes
    // клетки: постройка физически подделывает нишу, не переносит биом
    // целиком. Пусто/None -> нет кандидатов, вызывающая сторона (SpawnResourcesInCell)
    // должна сама решать, откатываться ли на GetRandomResourceForBiome.
    FName GetRandomResourceForNiche(const FGridCell& Cell, EGardenNiche Niche, const FHarvestContext& Context, FRandomStream& Rng) const;

    void Reset();

private:
    // Ленивая самозагрузка (2026-09-03) -- ЗАКРЫВАЕТ РЕАЛЬНЫЙ БАГ, не
    // украшение. Initialize() подсистемы ничего не грузил, а
    // AProjectHerbalistGameModeBase::BeginPlay -- грузил. Но GameMode
    // спавнится динамически и попадает в конец списка акторов, поэтому
    // BeginPlay РАЗМЕЩЁННОГО на уровне BP_GridWorldManager идёт РАНЬШЕ.
    // AGridWorldManager::InitializeCells -> SpawnResourcesInCell брала
    // подсистему (не null -- объект есть) с ПУСТОЙ картой Rows,
    // GetRandomResourceForBiome возвращала NAME_None на каждый бросок, и
    // мир молча оставался без единого ресурсного актора. В PIE-логе это
    // видно прямо: "IngredientRegistrySubsystem loaded 89 ingredients"
    // стоит ПОСЛЕ "InitializeCells" и "Cached 10000 cell heights".
    //
    // Тот же приём, которым проект уже решил ровно эту задачу для реестров
    // сущностей (GetAmbientEntityDefinitions и соседи сами делают
    // LoadObject): читатель не обязан знать, кто и когда обязан был
    // позвать загрузчик. Явный LoadFromDataTable по-прежнему выигрывает,
    // если позван до первого чтения -- на этом стоят все тесты, которые
    // создают свежую подсистему и сразу подсовывают свою таблицу.
    void EnsureLoaded() const;

    TMap<FName, FIngredientTableRow> Rows;
    bool bInitialized = false;

    // Таблицу этому объекту уже подавали -- явно (LoadFromDataTable) или
    // самозагрузкой. Ровно ОДНА попытка на время жизни объекта, и Reset()
    // её не отменяет: иначе Reset(), который значит «опустошить», был бы
    // тут же отменён следующим же чтением, и Herbalist.WaterRegistry.
    // ResetClearsEverything падал бы (он и упал, когда самозагрузка была
    // написана без этого флага). Новая PIE-сессия получает новый
    // GameInstance и новый объект подсистемы, поэтому «один раз за жизнь»
    // не мешает второму и третьему запуску загрузиться заново.
    mutable bool bLoadAttempted = false;

    TMap<EBiomeType, TArray<FName>> CachedResourcesByBiome;
    TMap<EBiomeType, TArray<int32>> CachedWeightsByBiome;

    // Тот же смысл, что пара выше, но только строки с bGrowsOnWater == true
    // (2026-09-02) — отдельный, не смешиваемый с земляным пул кандидатов
    // для клеток, которые сейчас вода.
    TMap<EBiomeType, TArray<FName>> CachedAquaticResourcesByBiome;
    TMap<EBiomeType, TArray<int32>> CachedAquaticWeightsByBiome;

    TMap<EGardenNiche, TArray<FName>> CachedResourcesByNiche;
    TMap<EGardenNiche, TArray<int32>> CachedWeightsByNiche;

    void BuildCache();

    // Слияние по Cell.BiomeWeights + взвешенный ролл (DESIGN_World_State.md
    // §15/§16, звенья 3 и 8) — вынесено из GetRandomResourceForBiome, чтобы
    // GetRandomResourceForAquaticBiome не дублировал ту же логику merge над
    // другой парой кэшей (ResourceCache/WeightCache — обычная земляная пара
    // или аквакэш, вызывающая сторона решает какая).
    FName PickFromBiomeWeightedCache(const TMap<EBiomeType, TArray<FName>>& ResourceCache,
        const TMap<EBiomeType, TArray<int32>>& WeightCache,
        const FGridCell& Cell, const FHarvestContext& Context, FRandomStream& Rng) const;

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