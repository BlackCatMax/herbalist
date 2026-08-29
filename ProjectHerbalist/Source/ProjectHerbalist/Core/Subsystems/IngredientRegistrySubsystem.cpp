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

    for (const auto& Pair : Rows)
    {
        for (EBiomeType Biome : Pair.Value.AllowedBiomes)
        {
            CachedResourcesByBiome.FindOrAdd(Biome).Add(Pair.Key);
            CachedWeightsByBiome.FindOrAdd(Biome).Add(Pair.Value.RarityWeight);
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

TArray<FName> UIngredientRegistrySubsystem::GetResourcesForBiome(EBiomeType Biome) const
{
    if (!bInitialized) return TArray<FName>();
    const TArray<FName>* Found = CachedResourcesByBiome.Find(Biome);
    return Found ? *Found : TArray<FName>();
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
    const TArray<FName>* Candidates = CachedResourcesByBiome.Find(Cell.Biome);
    if (!Candidates || Candidates->Num() == 0) return NAME_None;
    if (Candidates->Num() == 1) return (*Candidates)[0];

    const TArray<int32>* BaseWeights = CachedWeightsByBiome.Find(Cell.Biome);
    if (!BaseWeights || BaseWeights->Num() != Candidates->Num()) return (*Candidates)[0];

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
    EffectiveWeights.Reserve(Candidates->Num());
    float TotalWeight = 0.0f;
    for (int32 i = 0; i < Candidates->Num(); ++i)
    {
        const FIngredientTableRow* Row = Rows.Find((*Candidates)[i]);
        if (!Row)
        {
            EffectiveWeights.Add(static_cast<float>((*BaseWeights)[i]));
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

        const float Weight = static_cast<float>((*BaseWeights)[i]) * Suitability * StressFactor
            * SeasonWindow * TimeWindow * MoonWindow * WeatherWindow;
        EffectiveWeights.Add(Weight);
        TotalWeight += Weight;
    }
    if (TotalWeight <= KINDA_SMALL_NUMBER) return (*Candidates)[0];

    const float Roll = Rng.FRandRange(0.0f, TotalWeight);
    float Accum = 0.0f;
    for (int32 i = 0; i < Candidates->Num(); ++i)
    {
        Accum += EffectiveWeights[i];
        if (Roll <= Accum) return (*Candidates)[i];
    }
    return Candidates->Last();
}

void UIngredientRegistrySubsystem::Reset()
{
    Rows.Empty();
    CachedResourcesByBiome.Empty();
    CachedWeightsByBiome.Empty();
    bInitialized = false;
}