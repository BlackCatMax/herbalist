#include "IngredientRegistrySubsystem.h"
#include "Core/Subsystems/WaterTypeRegistrySubsystem.h"
#include "Core/Types/HerbalistCoreMath.h"
#include "Core/Config/HerbalistSettings.h"
#include "Engine/DataTable.h"
#include "ProjectHerbalist.h"
#include "HerbalistLogChannels.h"

void UIngredientRegistrySubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
}

void UIngredientRegistrySubsystem::Deinitialize()
{
    Reset();
    Super::Deinitialize();
}

void UIngredientRegistrySubsystem::LoadFromDataTable(UDataTable* IngredientTable)
{
    if (bInitialized) return;
    if (!IngredientTable)
    {
        UE_LOG(LogHerbalistData, Error, TEXT("IngredientTable is null"));
        bInitialized = true;
        return;
    }

    const FString Context(TEXT("IngredientRegistrySubsystem"));
    TArray<FName> RowNames = IngredientTable->GetRowNames();
    for (FName RowName : RowNames)
    {
        if (FIngredientTableRow* Row = IngredientTable->FindRow<FIngredientTableRow>(RowName, Context))
        {
            Rows.Add(RowName, *Row);
        }
    }

    BuildCache();
    bInitialized = true;
    UE_LOG(LogHerbalistData, Log, TEXT("IngredientRegistrySubsystem loaded %d ingredients"), Rows.Num());
}

void UIngredientRegistrySubsystem::BuildCache()
{
    CachedResourcesByBiome.Empty();
    CachedWeightsByBiome.Empty();
    CachedAquaticResourcesByBiome.Empty();
    CachedAquaticWeightsByBiome.Empty();
    CachedResourcesByNiche.Empty();
    CachedWeightsByNiche.Empty();

    for (const auto& Pair : Rows)
    {
        for (EBiomeType Biome : Pair.Value.AllowedBiomes)
        {
            CachedResourcesByBiome.FindOrAdd(Biome).Add(Pair.Key);
            CachedWeightsByBiome.FindOrAdd(Biome).Add(Pair.Value.RarityWeight);

            // Водные растения (2026-09-02) -- та же принадлежность биому
            // (AllowedBiomes), отдельный пул, не смешанный с земляным.
            if (Pair.Value.bGrowsOnWater)
            {
                CachedAquaticResourcesByBiome.FindOrAdd(Biome).Add(Pair.Key);
                CachedAquaticWeightsByBiome.FindOrAdd(Biome).Add(Pair.Value.RarityWeight);
            }
        }

        // Пристройка сада (§2.4) -- отдельный, параллельный ключ, не подмешан
        // в AllowedBiomes выше: постройка подделывает нишу, не биом.
        if (Pair.Value.GardenNiche != EGardenNiche::None)
        {
            CachedResourcesByNiche.FindOrAdd(Pair.Value.GardenNiche).Add(Pair.Key);
            CachedWeightsByNiche.FindOrAdd(Pair.Value.GardenNiche).Add(Pair.Value.RarityWeight);
        }
    }
}

const FIngredientTableRow* UIngredientRegistrySubsystem::GetRow(FName IngredientID) const
{
    if (!bInitialized) return nullptr;
    return Rows.Find(IngredientID);
}

EIngredientClass UIngredientRegistrySubsystem::Classify(FName IngredientID) const
{
    const FIngredientTableRow* Row = GetRow(IngredientID);
    if (Row) return Row->Class;

    // Проверяем, не является ли ID типом воды из WaterTypeRegistry
    if (UWaterTypeRegistrySubsystem* WaterReg = GetGameInstance()->GetSubsystem<UWaterTypeRegistrySubsystem>())
    {
        if (WaterReg->IsValidWaterType(IngredientID))
            return EIngredientClass::Water;
    }
    return EIngredientClass::Unknown;
}

bool UIngredientRegistrySubsystem::IsWater(FName IngredientID) const
{
    const FIngredientTableRow* Row = GetRow(IngredientID);
    if (Row) return Row->bIsWater;

    // Если строка не найдена, проверяем в реестре типов воды
    if (UWaterTypeRegistrySubsystem* WaterReg = GetGameInstance()->GetSubsystem<UWaterTypeRegistrySubsystem>())
    {
        return WaterReg->IsValidWaterType(IngredientID);
    }
    return false;
}

bool UIngredientRegistrySubsystem::IsKnown(FName IngredientID) const
{
    if (bInitialized && Rows.Contains(IngredientID))
        return true;

    // Также считаем известным, если это валидный тип воды
    if (UWaterTypeRegistrySubsystem* WaterReg = GetGameInstance()->GetSubsystem<UWaterTypeRegistrySubsystem>())
    {
        return WaterReg->IsValidWaterType(IngredientID);
    }
    return false;
}

namespace
{
    // Мягкий гейт "окна" (сезон/время суток/луна/погода) — DESIGN_World_State.md
    // §15/§16, звено 8. bRequires == false значит "условия нет" -> множитель 1
    // всегда, вне зависимости от bMatchesNow (та же семантика, что у пустого
    // AllowedSeasons). Иначе — 1, если текущее условие совпало с требуемым,
    // иначе IngredientWindowMismatchMultiplier (никогда ровно 0, тот же принцип,
    // что уже применён к гауссиане по State ниже).
    float WindowMultiplier(bool bRequires, bool bMatchesNow, float MismatchMultiplier)
    {
        if (!bRequires) return 1.0f;
        return bMatchesNow ? 1.0f : MismatchMultiplier;
    }
}

FName UIngredientRegistrySubsystem::GetRandomResourceForBiome(const FGridCell& Cell, const FHarvestContext& Context, FRandomStream& Rng) const
{
    return PickFromBiomeWeightedCache(CachedResourcesByBiome, CachedWeightsByBiome, Cell, Context, Rng);
}

FName UIngredientRegistrySubsystem::GetRandomResourceForAquaticBiome(const FGridCell& Cell, const FHarvestContext& Context, FRandomStream& Rng) const
{
    return PickFromBiomeWeightedCache(CachedAquaticResourcesByBiome, CachedAquaticWeightsByBiome, Cell, Context, Rng);
}

FName UIngredientRegistrySubsystem::PickFromBiomeWeightedCache(const TMap<EBiomeType, TArray<FName>>& ResourceCache,
    const TMap<EBiomeType, TArray<int32>>& WeightCache,
    const FGridCell& Cell, const FHarvestContext& Context, FRandomStream& Rng) const
{
    // PCG-биомы (2026-08-31): Cell.BiomeWeights пуст для клетки вне всех
    // ABiomeRegionVolume (см. AGridWorldManager::InitializeCells) --
    // обязательный откат на {Cell.Biome: 1.0}, не опция. Сохраняет
    // побитовую совместимость со всем, что конструирует FGridCell
    // напрямую и выставляет только Biome (IngredientHarvestWindowTest.cpp/
    // IngredientRegistryTest.cpp, 8 тестов, ни один не трогает BiomeWeights).
    TArray<FBiomeWeightEntry> Weights = Cell.BiomeWeights;
    if (Weights.Num() == 0)
    {
        FBiomeWeightEntry Fallback;
        Fallback.Biome = Cell.Biome;
        Fallback.Weight = 1.0f;
        Weights.Add(Fallback);
    }

    // Один взвешенный ролл над объединённым списком кандидатов всех долей
    // клетки, не отдельный RNG-бросок "какой биом сначала" -- сохраняет
    // однодетерминированную схему, на которой держится весь пайплайн (тот
    // же сид -> тот же результат, без лишнего потребления Rng).
    TArray<FName> MergedCandidates;
    TArray<float> MergedWeights;
    for (const FBiomeWeightEntry& Entry : Weights)
    {
        const TArray<FName>* Candidates = ResourceCache.Find(Entry.Biome);
        if (!Candidates || Candidates->Num() == 0) continue;

        const TArray<int32>* BaseWeights = WeightCache.Find(Entry.Biome);
        // Кэши строятся в одном проходе BuildCache() и всегда одной длины
        // -- на практике недостижимо, защитный пропуск этой доли, не крах.
        if (!BaseWeights || BaseWeights->Num() != Candidates->Num()) continue;

        for (int32 i = 0; i < Candidates->Num(); ++i)
        {
            // Один и тот же FName-кандидат из двух пересекающихся биомов
            // — сознательно НЕ дедуплицируется, вероятностная масса
            // складывается (верно для равного распределения на стыке
            // регионов — не "чинить" обратно в дедуп).
            MergedCandidates.Add((*Candidates)[i]);
            MergedWeights.Add(static_cast<float>((*BaseWeights)[i]) * Entry.Weight);
        }
    }

    if (MergedCandidates.Num() == 0) return NAME_None;
    if (MergedCandidates.Num() == 1) return MergedCandidates[0];

    return PickWeightedResource(MergedCandidates, MergedWeights, Cell, Context, Rng);
}

FName UIngredientRegistrySubsystem::GetRandomResourceForNiche(const FGridCell& Cell, EGardenNiche Niche, const FHarvestContext& Context, FRandomStream& Rng) const
{
    if (Niche == EGardenNiche::None) return NAME_None;

    const TArray<FName>* Candidates = CachedResourcesByNiche.Find(Niche);
    if (!Candidates || Candidates->Num() == 0) return NAME_None;
    if (Candidates->Num() == 1) return (*Candidates)[0];

    const TArray<int32>* BaseWeights = CachedWeightsByNiche.Find(Niche);
    if (!BaseWeights || BaseWeights->Num() != Candidates->Num()) return (*Candidates)[0];

    // Ниша сада не пересекается (один Niche на клетку, не список) --
    // просто перевод int32->float перед вызовом общей функции, без merge.
    TArray<float> FloatWeights;
    FloatWeights.Reserve(BaseWeights->Num());
    for (int32 W : *BaseWeights)
    {
        FloatWeights.Add(static_cast<float>(W));
    }

    return PickWeightedResource(*Candidates, FloatWeights, Cell, Context, Rng);
}

FName UIngredientRegistrySubsystem::PickWeightedResource(const TArray<FName>& Candidates, const TArray<float>& BaseWeights,
    const FGridCell& Cell, const FHarvestContext& Context, FRandomStream& Rng) const
{
    const UHerbalistSettings* Settings = GetHerbalistSettings();
    // Пригодность (DESIGN_World_State.md §15): AllowedBiomes уже отфильтровал
    // "кто может здесь расти" — Candidates. Здесь решается "сколько шансов у
    // кого": RarityWeight гасится удалённостью Cell.State от Row.BaseState
    // по гауссиане, не линейно — так середина спада мягкая, а полюса (клетка,
    // почти точная копия чьего-то эталона / его противоположность) выражены
    // резко, при этом вес никогда не проваливается ровно в 0.
    const float Falloff = Settings ? Settings->IngredientSuitabilityFalloff : 2.0f;
    // (1 − HarvestStress), §15 звено 4: истощённая сборами клетка родит меньше
    // — тот же HarvestStress, что уже двигает Гнильников/Злыдней/Подпольников
    // (AmbientEntityTypes.h), здесь читается напрямую, не через сущность.
    const float StressFactor = FMath::Clamp(1.0f - Cell.HarvestStress, 0.0f, 1.0f);
    const float WindowMismatch = Settings ? Settings->IngredientWindowMismatchMultiplier : 0.15f;

    TArray<float> EffectiveWeights;
    EffectiveWeights.Reserve(Candidates.Num());
    float TotalWeight = 0.0f;
    for (int32 i = 0; i < Candidates.Num(); ++i)
    {
        const FIngredientTableRow* Row = Rows.Find(Candidates[i]);
        if (!Row)
        {
            EffectiveWeights.Add(BaseWeights[i]);
            TotalWeight += EffectiveWeights.Last();
            continue;
        }

        const float Dist = HerbalistCore::Math::Distance(Cell.State, Row->BaseState);
        const float Suitability = FMath::Exp(-Falloff * Dist * Dist);

        // Пусто = любой сезон (см. комментарий у AllowedSeasons в IngredientTableRow.h).
        // bAutumnOnly — второй, более узкий гейт ВНУТРИ Лета (см. комментарий там же):
        // применяется только когда СЕЙЧАС Лето, весенние/зимние окна не трогает.
        const bool bSeasonOK = Row->AllowedSeasons.Num() == 0 || Row->AllowedSeasons.Contains(Context.Season);
        const bool bAutumnOK = !Row->bAutumnOnly || Context.Season != ESeason::Summer || Context.bLateSummer;
        const float SeasonWindow = (bSeasonOK && bAutumnOK) ? 1.0f : WindowMismatch;

        const float TimeWindow = WindowMultiplier(Row->HarvestTimeWindow != EHarvestTimeWindow::Any,
            Row->HarvestTimeWindow == Context.TimeOfDay, WindowMismatch);
        const float MoonWindow = WindowMultiplier(Row->bRequiresMoonPhase,
            Row->RequiredMoonPhase == Context.MoonPhase, WindowMismatch);
        const float WeatherWindow = WindowMultiplier(Row->bRequiresDryWeather,
            Context.bDryWeather, WindowMismatch);

        const float Weight = BaseWeights[i] * Suitability * StressFactor
            * SeasonWindow * TimeWindow * MoonWindow * WeatherWindow;
        EffectiveWeights.Add(Weight);
        TotalWeight += Weight;
    }
    if (TotalWeight <= KINDA_SMALL_NUMBER) return Candidates[0];

    const float Roll = Rng.FRandRange(0.0f, TotalWeight);
    float Accum = 0.0f;
    for (int32 i = 0; i < Candidates.Num(); ++i)
    {
        Accum += EffectiveWeights[i];
        if (Roll <= Accum) return Candidates[i];
    }
    return Candidates.Last();
}

void UIngredientRegistrySubsystem::Reset()
{
    Rows.Empty();
    CachedResourcesByBiome.Empty();
    CachedWeightsByBiome.Empty();
    CachedAquaticResourcesByBiome.Empty();
    CachedAquaticWeightsByBiome.Empty();
    CachedResourcesByNiche.Empty();
    CachedWeightsByNiche.Empty();
    bInitialized = false;
}