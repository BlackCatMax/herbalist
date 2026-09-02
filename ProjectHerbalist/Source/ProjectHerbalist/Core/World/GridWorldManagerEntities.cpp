// Core/World/GridWorldManagerEntities.cpp
//
// Вертикальный срез "Проявление сущностей" (02_GDD/16_Entity_Manifestation.md).
//   - Гнильники, Моховые духи, Степные огни (Низший) -> амбиентная зона (§16.2),
//     таблица определений AmbientEntityTypes.h, не захардкожено по одной штуке
//   - Полевик     (Основной,   Лесостепь)      -> "хозяин" с Respect (§16.3)
//   - Берегиня    (Легендарный, Речная пойма)  -> порог мирового состояния (§16.4),
//     два независимых пути: HistoryPurity клетки ИЛИ Restoration капища рядом
//   - Морочники   (Опасная нечисть, повсеместно) -> искажение восприятия (§16.5)
//   - Ночной нудж (Опасная нечисть, §16.5)     -> разлитый по всей сетке
//     Distortion/Corruption, пока держится ночь; не претендует на клетку
//
// Всё, что мутирует State/TargetState, идёт через Delta.TargetStateNudges ->
// ApplyStateDelta — тот же внепайплайновый, но Single-Writer-совместимый канал,
// что уже использует ApplyBiomeInfluences (GridWorldManagerCore.cpp). Ничего
// здесь не проходит через Command/Delta детерминированный пайплайн (PipelineV2) —
// это осознанно, по той же логике, что и у RegenerateCellParameters: это
// презентационно-атмосферный слой, а не игровая причинность.

#include "Core/World/GridWorldManager.h"
#include "Core/Config/HerbalistSettings.h"
#include "Core/Entities/AmbientEntityTypes.h"
#include "Core/Entities/LandmarkTypes.h"
#include "Core/Entities/LegendaryEntityTypes.h"
#include "Core/Entities/HerbalistEntityActor.h"
#include "Core/Entities/AmbientEntityActor.h"
#include "Core/Entities/LandmarkEntityActor.h"
#include "Core/Entities/LegendaryEntityActor.h"
#include "Core/BiomeGraph/BiomeGraphSubsystem.h"
#include "Core/Simulation/Public/DeltaTypes.h"
#include "Core/Types/HerbalistCoreMath.h"
#include "ProjectHerbalist.h"
#include "HerbalistLogChannels.h"

using HerbalistCore::Math::PassesHysteresisThreshold;

namespace
{
    const FName EntityID_Gnilniki(TEXT("Гнильники"));
    const FName EntityID_Bereginya(TEXT("Берегиня"));

    // Ранг бестиария (см. заголовок файла: Низший/Основной/Легендарный) решает,
    // кто вытесняет кого при совпадении условий в одной клетке (DESIGN_World_State.md
    // §14) — "вытеснение вместо миграции": никто не переезжает, просто более
    // редкая/сильная сущность выигрывает проекцию, если условия обеих выполнены
    // одновременно. Выше число — выше приоритет.
    int32 GetEntityManifestationPriority(FName EntityID)
    {
        if (EntityID == EntityID_Bereginya) return 2;  // Легендарный
        // Легендарный ранг из реестра (LegendaryEntityTypes.h, 2026-08-29) —
        // та же биома-независимая проверка, что уже есть у Основного/Низшего
        // ниже. Берегиня выше (жёстко закодирована в этом файле) не тронута.
        for (const FLegendaryEntityDefinition& Def : GetLegendaryEntityDefinitions())
        {
            if (EntityID == Def.EntityID) return 2;
        }
        // Основной ранг — раньше только Полевик хардкодом, остальные 12
        // "хозяев" §16.3 молча падали в -1 при столкновении биомов с другим
        // рангом (не было проблемой, пока биомы landmark/ambient/legendary
        // не пересекались — Легендарный реестр выше это меняет, Дуб-старец
        // и Жердяи оба на Широколиственном лесу). Не найдено багом раньше,
        // почищено попутно, не отдельной задачей. Полевик тоже находится
        // этим циклом (он есть в реестре) — отдельная проверка не нужна.
        for (const FLandmarkDefinition& Def : GetLandmarkDefinitions())
        {
            if (EntityID == Def.EntityID) return 1;
        }
        // Низший ранг — Гнильники и все определения из AmbientEntityTypes.h
        // (Моховые духи, Степные огни, ...). Сегодня биомы не пересекаются,
        // так что до сравнения приоритетов дело не доходит, но при добавлении
        // нового Низшего на биом Полевика/Берегини это должно уже работать
        // правильно, а не молча вернуть -1 (см. AmbientEntityTypes.h).
        for (const FAmbientEntityDefinition& Def : GetAmbientEntityDefinitions())
        {
            if (EntityID == Def.EntityID) return 0;
        }
        return -1;
    }

    // Клетка свободна для CandidateID, если она либо не занята, либо уже занята
    // им же (переподтверждение), либо занята кем-то менее приоритетным (вытесняем).
    bool CanManifest(const FGridCell& Cell, FName CandidateID)
    {
        return Cell.ManifestedEntityID.IsNone()
            || Cell.ManifestedEntityID == CandidateID
            || GetEntityManifestationPriority(CandidateID) > GetEntityManifestationPriority(Cell.ManifestedEntityID);
    }

    // Согласованность триггер-оси Низшего с испорченным полюсом бистабильности
    // (находка финального аудита 2026-08-30: гейт `!bDegrading`, добавленный тем
    // же днём чуть раньше, изначально освобождал от него только буквальную ось
    // Corruption, хотя переход в bDegrading фиксирует ЧЕТЫРЕ оси разом —
    // Corruption=1, Purity=0, Distortion=1, Stability=0, см. флип-блок в
    // GridWorldManagerCore.cpp — не одну). Существо согласовано с полюсом, если
    // его триггер стреляет в ту же сторону, что и полюс держит эту ось: высокий
    // Corruption/Distortion или низкий Purity/Stability. Остальные оси
    // (HarvestStress, Body/Mind/Spirit/Nature) полюс не трогает вовсе — всегда
    // ортогональны, гейт остаётся для них в силе независимо от направления.
    bool IsAxisConsistentWithCorruptPole(EAmbientTriggerAxis Axis, bool bTriggerAbove)
    {
        switch (Axis)
        {
            case EAmbientTriggerAxis::Corruption: return bTriggerAbove;    // полюс: Corruption=1
            case EAmbientTriggerAxis::Purity:     return !bTriggerAbove;   // полюс: Purity=0
            case EAmbientTriggerAxis::Distortion: return bTriggerAbove;    // полюс: Distortion=1
            case EAmbientTriggerAxis::Stability:  return !bTriggerAbove;   // полюс: Stability=0
            default:                               return false;
        }
    }

    // Собственный C++-сигнал погоды (§15.7), 2026-08-29 — детерминированная
    // value-noise: хэш (сид, индекс фронта) даёт число в [0,1), интерполяция
    // между соседними фронтами по времени сглаживает переход. Без сохраняемого
    // состояния вовсе (чистая функция GameClockSeconds+RngBaseSeed) — тот же
    // принцип, что уже даёт детерминизм пайплайну через HashCombine(сид, номер
    // тика) в ExecuteTick, просто на другой временной шкале.
    float DeterministicNoise01(int32 BaseSeed, int32 Index)
    {
        const uint32 Hash = HashCombine(static_cast<uint32>(BaseSeed), static_cast<uint32>(Index));
        return FRandomStream(static_cast<int32>(Hash)).FRand();
    }

    float SampleWeatherNoise(float GameClockSeconds, int32 RngBaseSeed, float FrontDurationSeconds, int32 Channel)
    {
        const float SafeDuration = FMath::Max(1.0f, FrontDurationSeconds);
        const float Position = GameClockSeconds / SafeDuration;
        const int32 FrontA = FMath::FloorToInt(Position);
        const int32 FrontB = FrontA + 1;
        // *2+Channel разводит каналы (ветер/снег) по разным хэш-потокам —
        // иначе они были бы идеально скоррелированы (одна и та же метель
        // всегда точь-в-точь совпадала бы с тем же ветром).
        const float NoiseA = DeterministicNoise01(RngBaseSeed, FrontA * 2 + Channel);
        const float NoiseB = DeterministicNoise01(RngBaseSeed, FrontB * 2 + Channel);
        const float Alpha = FMath::SmoothStep(0.0f, 1.0f, Position - static_cast<float>(FrontA));
        return FMath::Lerp(NoiseA, NoiseB, Alpha);
    }

}

// ============================================================================
// СУТОЧНЫЙ ЦИКЛ (минимальная версия для среза — только фаза, без луны/сезона)
// ============================================================================

float AGridWorldManager::GetTimeOfDay01() const
{
    const UHerbalistSettings* Settings = GetHerbalistSettings();
    const float DayLengthSeconds = FMath::Max(1.0f, (Settings ? Settings->GameDayMinutes : 32.0f) * 60.0f);
    // GameClockSeconds, не GetWorld()->GetTimeSeconds() — правка 2026-08-23:
    // движковое время обнуляется при перезапуске сессии, а фаза суток должна
    // переживать сохранение/загрузку (см. комментарий у GameClockSeconds в
    // GridWorldManager.h). Раньше здесь читалось прямо GetTimeSeconds() как
    // единственный источник, которым также помечен CreationTime предметов —
    // тот пока НЕ переведён на GameClockSeconds, это малый, самостоятельный
    // косметический разъезд (AUDIT_AND_REFACTORING_PLAN §3.4), не блокирует.
    return FMath::Fmod(GameClockSeconds, DayLengthSeconds) / DayLengthSeconds;
}

bool AGridWorldManager::IsNight() const
{
    const UHerbalistSettings* Settings = GetHerbalistSettings();
    const float DayMinutes = FMath::Max(1.0f, Settings ? Settings->GameDayMinutes : 32.0f);
    // Ночь — последние 6 игровых минут суток (15_Cycles_And_Shrines §15.2:
    // Рассвет 6 / День 14 / Закат 6 / Ночь 6, итого 32).
    const float NightDurationMinutes = 6.0f;
    const float NightStartFraction = 1.0f - FMath::Clamp(NightDurationMinutes / DayMinutes, 0.0f, 1.0f);
    return GetTimeOfDay01() >= NightStartFraction;
}

bool AGridWorldManager::IsDawn() const
{
    const UHerbalistSettings* Settings = GetHerbalistSettings();
    const float DayMinutes = FMath::Max(1.0f, Settings ? Settings->GameDayMinutes : 32.0f);
    // Рассвет — первые 6 игровых минут суток, тот же расклад §15.2, что и у
    // IsNight() (абсолютные минуты, не пропорциональная доля — если
    // GameDayMinutes поменять, Рассвет/Закат/Ночь останутся теми же 6
    // минутами, просто станут другой долей суток).
    const float DawnDurationMinutes = 6.0f;
    const float DawnEndFraction = FMath::Clamp(DawnDurationMinutes / DayMinutes, 0.0f, 1.0f);
    return GetTimeOfDay01() < DawnEndFraction;
}

bool AGridWorldManager::IsDusk() const
{
    const UHerbalistSettings* Settings = GetHerbalistSettings();
    const float DayMinutes = FMath::Max(1.0f, Settings ? Settings->GameDayMinutes : 32.0f);
    const float NightDurationMinutes = 6.0f;
    const float DuskDurationMinutes = 6.0f;
    const float NightStartFraction = 1.0f - FMath::Clamp(NightDurationMinutes / DayMinutes, 0.0f, 1.0f);
    const float DuskStartFraction = NightStartFraction - FMath::Clamp(DuskDurationMinutes / DayMinutes, 0.0f, 1.0f);
    const float T = GetTimeOfDay01();
    return T >= DuskStartFraction && T < NightStartFraction;
}

float AGridWorldManager::GetDuskProgress01() const
{
    // 0 на входе в Закат, 1 у порога Ночи — "+Distortion (нарастающее)"
    // §15.2: Морок просыпается раньше, чем стемнеет, но не сразу в полную силу.
    if (!IsDusk()) return 0.0f;
    const UHerbalistSettings* Settings = GetHerbalistSettings();
    const float DayMinutes = FMath::Max(1.0f, Settings ? Settings->GameDayMinutes : 32.0f);
    const float NightDurationMinutes = 6.0f;
    const float DuskDurationMinutes = 6.0f;
    const float NightStartFraction = 1.0f - FMath::Clamp(NightDurationMinutes / DayMinutes, 0.0f, 1.0f);
    const float DuskStartFraction = NightStartFraction - FMath::Clamp(DuskDurationMinutes / DayMinutes, 0.0f, 1.0f);
    const float DuskSpan = FMath::Max(NightStartFraction - DuskStartFraction, KINDA_SMALL_NUMBER);
    return FMath::Clamp((GetTimeOfDay01() - DuskStartFraction) / DuskSpan, 0.0f, 1.0f);
}

bool AGridWorldManager::IsPoludnitsaWindow() const
{
    // Полудница (§15.2 "Полдень как отдельная опасность"): короткий (~2 мин)
    // всплеск Distortion в середине Дня, только в открытых биомах — не
    // отдельная сущность в UpdateEntityManifestations (не привязана к
    // конкретной клетке-хозяйке, как Полевик), а временное окно, тем же
    // паттерном, что ночной нудж §16.5 в самом UpdateEntityManifestations.
    const UHerbalistSettings* Settings = GetHerbalistSettings();
    const float DayMinutes = FMath::Max(1.0f, Settings ? Settings->GameDayMinutes : 32.0f);
    const float DawnDurationMinutes = 6.0f;
    const float DuskDurationMinutes = 6.0f;
    const float NightDurationMinutes = 6.0f;
    const float PoludnitsaWindowMinutes = 2.0f;

    const float DawnEndFraction = FMath::Clamp(DawnDurationMinutes / DayMinutes, 0.0f, 1.0f);
    const float NightStartFraction = 1.0f - FMath::Clamp(NightDurationMinutes / DayMinutes, 0.0f, 1.0f);
    const float DuskStartFraction = NightStartFraction - FMath::Clamp(DuskDurationMinutes / DayMinutes, 0.0f, 1.0f);
    const float DayMidpoint = (DawnEndFraction + DuskStartFraction) * 0.5f;
    const float HalfWindow = FMath::Clamp(PoludnitsaWindowMinutes / DayMinutes, 0.0f, 1.0f) * 0.5f;

    return FMath::Abs(GetTimeOfDay01() - DayMidpoint) < HalfWindow;
}

// ============================================================================
// ЛУННЫЙ ЦИКЛ (02_GDD/15_Cycles_And_Shrines.md §15.3) — v1: только фаза и
// эффект на сбор (Растущая/Полнолуние). Эффекты применённого зелья
// (Новолуние/Убывающая) сознательно не в этом проходе — тот же принцип
// вертикального среза, что уже был у капищ/бестиария/Заряны.
// ============================================================================

EMoonPhase AGridWorldManager::GetMoonPhase() const
{
    const UHerbalistSettings* Settings = GetHerbalistSettings();
    const float DayLengthSeconds = FMath::Max(1.0f, (Settings ? Settings->GameDayMinutes : 32.0f) * 60.0f);
    // 7 игровых суток на фазу — тот же StressRecoveryGameDays, что уже
    // определяет срок зарастания клетки (§15.3: "не новое число"), не
    // отдельная константа, которая могла бы разъехаться с ней (см.
    // AUDIT_AND_REFACTORING_PLAN.md "проверка на двух источниках одного понятия").
    const float PhaseDurationDays = FMath::Max(0.01f, Settings ? Settings->StressRecoveryGameDays : 7.0f);
    const float MoonCycleSeconds = PhaseDurationDays * 4.0f * DayLengthSeconds;

    const float CycleFraction = FMath::Fmod(GameClockSeconds, MoonCycleSeconds) / MoonCycleSeconds;
    const int32 PhaseIndex = FMath::Clamp(FMath::FloorToInt(CycleFraction * 4.0f), 0, 3);
    return static_cast<EMoonPhase>(PhaseIndex);
}

// ============================================================================
// ГОДОВОЙ КРУГ (02_GDD/15_Cycles_And_Shrines.md §15.4) — v1: сезон +
// эффект на скорость зарастания клеток (Весна/Зима) + разлитая по сетке
// прибавка Purity зимой ("снег как чистота", в один ряд с ночным нуджем
// §16.5 из UpdateEntityManifestations). "Резкое падение Fertility" зимой из
// спецификации НЕ реализовано: FEnvironment::Fertility нигде не читается
// нигде в коде (проверено grep по Source/) — заводить сезонный эффект на
// мёртвое поле значило бы разыграть эффект, которого никто не увидит,
// тот самый паттерн "объявлено и не используется" из META_AUDIT.md, только
// в новую сторону. Лето — намеренно нейтральный/базовый сезон: спецификация
// не даёт для него числовой формулы (только настроение — "максимум
// предсказуемости... нарастающая грань к концу"), в отличие от Весны/Зимы.
// ============================================================================

ESeason AGridWorldManager::GetSeason() const
{
    const UHerbalistSettings* Settings = GetHerbalistSettings();
    const float DayLengthSeconds = FMath::Max(1.0f, (Settings ? Settings->GameDayMinutes : 32.0f) * 60.0f);
    // §15.4: "самое условное число во всём разделе" — в отличие от 7-суточной
    // фазы луны, тут не на что переиспользоваться, честная новая настройка.
    const float SeasonDurationDays = FMath::Max(0.01f, Settings ? Settings->SeasonDurationDays : 117.0f);
    const float YearDurationSeconds = SeasonDurationDays * 3.0f * DayLengthSeconds;

    const float CycleFraction = FMath::Fmod(GameClockSeconds, YearDurationSeconds) / YearDurationSeconds;
    const int32 SeasonIndex = FMath::Clamp(FMath::FloorToInt(CycleFraction * 3.0f), 0, 2);
    return static_cast<ESeason>(SeasonIndex);
}

float AGridWorldManager::GetSeasonProgress01() const
{
    const UHerbalistSettings* Settings = GetHerbalistSettings();
    const float DayLengthSeconds = FMath::Max(1.0f, (Settings ? Settings->GameDayMinutes : 32.0f) * 60.0f);
    const float SeasonDurationDays = FMath::Max(0.01f, Settings ? Settings->SeasonDurationDays : 117.0f);
    const float YearDurationSeconds = SeasonDurationDays * 3.0f * DayLengthSeconds;

    const float CycleFraction = FMath::Fmod(GameClockSeconds, YearDurationSeconds) / YearDurationSeconds;
    return FMath::Frac(CycleFraction * 3.0f);
}

bool AGridWorldManager::IsLateSummer() const
{
    const UHerbalistSettings* Settings = GetHerbalistSettings();
    const float Threshold = Settings ? Settings->LateSummerProgressThreshold : 0.8f;
    return GetSeason() == ESeason::Summer && GetSeasonProgress01() >= Threshold;
}

bool AGridWorldManager::IsKupalaNight() const
{
    const UHerbalistSettings* Settings = GetHerbalistSettings();
    const float WindowStart = Settings ? Settings->KupalaWindowStart : 0.15f;
    const float WindowEnd   = Settings ? Settings->KupalaWindowEnd   : 0.18f;
    if (GetSeason() != ESeason::Summer) return false;
    const float Progress = GetSeasonProgress01();
    return Progress >= WindowStart && Progress < WindowEnd && IsNight();
}

float AGridWorldManager::GetWindIntensity() const
{
    const UHerbalistSettings* Settings = GetHerbalistSettings();
    const float FrontDuration = Settings ? Settings->WeatherFrontDurationSeconds : 480.0f;
    return SampleWeatherNoise(GameClockSeconds, RngBaseSeed, FrontDuration, /*Channel=*/0);
}

float AGridWorldManager::GetSnowIntensity() const
{
    if (GetSeason() != ESeason::Winter) return 0.0f;   // снегу неоткуда взяться вне Зимы
    const UHerbalistSettings* Settings = GetHerbalistSettings();
    const float FrontDuration = Settings ? Settings->WeatherFrontDurationSeconds : 480.0f;
    return SampleWeatherNoise(GameClockSeconds, RngBaseSeed, FrontDuration, /*Channel=*/1);
}

bool AGridWorldManager::IsWindy() const
{
    const UHerbalistSettings* Settings = GetHerbalistSettings();
    return GetWindIntensity() >= (Settings ? Settings->WindyThreshold : 0.6f);
}

bool AGridWorldManager::IsBlizzard() const
{
    const UHerbalistSettings* Settings = GetHerbalistSettings();
    const float WindThreshold = Settings ? Settings->BlizzardWindThreshold : 0.55f;
    const float SnowThreshold = Settings ? Settings->BlizzardSnowThreshold : 0.55f;
    return GetWindIntensity() >= WindThreshold && GetSnowIntensity() >= SnowThreshold;
}

float AGridWorldManager::GetRainIntensity() const
{
    const UHerbalistSettings* Settings = GetHerbalistSettings();
    const float FrontDuration = Settings ? Settings->WeatherFrontDurationSeconds : 480.0f;
    return SampleWeatherNoise(GameClockSeconds, RngBaseSeed, FrontDuration, /*Channel=*/2);
}

bool AGridWorldManager::IsRainy() const
{
    const UHerbalistSettings* Settings = GetHerbalistSettings();
    return GetRainIntensity() >= (Settings ? Settings->RainyThreshold : 0.6f);
}

float AGridWorldManager::ComputePerceptionDistortion(int32 X, int32 Y) const
{
    const FGridCell* Cell = GetCellConst(X, Y);
    if (!Cell) return 0.0f;

    const UHerbalistSettings* Settings = GetHerbalistSettings();
    float Perceived = Cell->Memory.AccumulatedDistortion;

    // Морочники: ночью восприятие лжёт сильнее, сам мир (S_real) не меняется —
    // это надбавка только для UI (тултипы через PerceiveValue), не для Cell.State.
    if (IsNight())
    {
        Perceived += Settings ? Settings->NightPerceptionDistortionBonus : 0.25f;
    }

    // Любая проявленная сущность в клетке — сигнал "здесь не всё как кажется",
    // независимо от того, какая именно (для среза это в первую очередь Гнильники).
    if (!Cell->ManifestedEntityID.IsNone())
    {
        Perceived += 0.1f;
    }

    return FMath::Clamp(Perceived, 0.0f, 1.0f);
}

// ============================================================================
// РАССТАНОВКА ТЕСТОВЫХ "ОБИТАЛИЩ" (только для среза — в продакшене вручную)
// ============================================================================

void AGridWorldManager::SeedTestLandmarks()
{
    EntityLandmarks.Empty();

    // Один "хозяин" на биом мог занять первую попавшуюся клетку молча —
    // с 2026-08-29 несколько определений делят один биом (Тайга: Аука +
    // Дух Медведя; Широколиств. лес: Гуменник + Овинник + Жердяи; Степь:
    // Переплут + Курганные огни; Смеш. лес: Боровик + Луговой; Лесостепь:
    // Полевик + Курганники; Речная пойма: Бродницы) — CellsUsed следит,
    // чтобы каждый получил СВОЮ клетку, а не потерялся, заняв уже занятую
    // (первый в реестре выигрывал бы её снова и снова).
    TSet<FIntPoint> CellsUsed;
    for (const FLandmarkDefinition& Def : GetLandmarkDefinitions())
    {
        // Домовой и подобные (bManualRegistrationOnly) не сеются по биому —
        // их регистрирует напрямую тот, кому они принадлежат (AAlchemyTableActor
        // ::BeginPlay), иначе задвоились бы: один экземпляр здесь на случайной
        // клетке подходящего биома, второй — на самом деле у жилища.
        if (Def.bManualRegistrationOnly) continue;

        for (const FGridCell& Cell : Cells)
        {
            if (Cell.Biome != Def.Biome || Cell.bIsWater) continue;
            const FIntPoint Coord(Cell.X, Cell.Y);
            if (CellsUsed.Contains(Coord)) continue;

            FEntityLandmark Landmark;
            Landmark.EntityID = Def.EntityID;
            Landmark.Cell = Coord;
            Landmark.Respect = 0.0f;
            EntityLandmarks.Add(Landmark);
            CellsUsed.Add(Coord);
            UE_LOG(LogHerbalistWorld, Log, TEXT("[Entities] Seeded landmark %s at (%d,%d)"),
                *Def.EntityID.ToString(), Coord.X, Coord.Y);
            break;
        }
    }
}

FEntityLandmark* AGridWorldManager::FindLandmarkAt(const FIntPoint& Cell)
{
    for (FEntityLandmark& L : EntityLandmarks)
    {
        if (L.Cell == Cell) return &L;
    }
    return nullptr;
}

void AGridWorldManager::RegisterDomovoi(const FIntPoint& Cell)
{
    if (FindLandmarkAt(Cell))
    {
        // Уже что-то стоит на этой клетке (включая сам Домовой при повторном
        // BeginPlay) — тот же принцип идемпотентности, что RegisterShrine.
        return;
    }

    FEntityLandmark Landmark;
    Landmark.EntityID = FName(TEXT("Домовой"));
    Landmark.Cell = Cell;
    Landmark.Respect = 0.0f;
    EntityLandmarks.Add(Landmark);
    UE_LOG(LogHerbalistWorld, Log, TEXT("[Domovoi] Registered at (%d,%d)"), Cell.X, Cell.Y);
}

// Якоря Легендарного ранга (§16.4, LegendaryEntityTypes.h) — тот же
// CellsUsed-приём, что уже применяет SeedTestLandmarks выше, но с учётом
// bLandOnly/bWaterOnly (у Landmark их нет вовсе, там земля всегда).
void AGridWorldManager::SeedLegendaryAnchors()
{
    LegendaryAnchors.Empty();

    TSet<FIntPoint> CellsUsed;
    for (const FLegendaryEntityDefinition& Def : GetLegendaryEntityDefinitions())
    {
        for (const FGridCell& Cell : Cells)
        {
            if (Cell.Biome != Def.Biome) continue;
            if (Def.bLandOnly && Cell.bIsWater) continue;
            if (Def.bWaterOnly && !Cell.bIsWater) continue;
            const FIntPoint Coord(Cell.X, Cell.Y);
            if (CellsUsed.Contains(Coord)) continue;

            LegendaryAnchors.Add(Def.EntityID, Coord);
            CellsUsed.Add(Coord);
            UE_LOG(LogHerbalistWorld, Log, TEXT("[Entities] Seeded legendary anchor %s at (%d,%d)"),
                *Def.EntityID.ToString(), Coord.X, Coord.Y);
            break;
        }
    }
}

// Физический актор проявленной сущности (2026-08-30, "заводим родительские
// классы для сущностей и связки") — общий для всех трёх рангов, т.к. у всех
// троих один и тот же жизненный цикл проявления (см. три прохода ниже,
// каждый пишет Cell.ManifestedEntityID/None одинаковым способом). Сравниваем
// EntityID актора с EntityID клетки, а не просто "актор есть/нет" — если
// клетку тем же тиком отобрал другой EntityID того же ранга (CanManifest
// пропускает более приоритетного), старый актор должен уступить место новому.
void AGridWorldManager::SyncManifestedEntityActor(FGridCell& Cell, TSubclassOf<AHerbalistEntityActor> RequestedClass, TSubclassOf<AHerbalistEntityActor> DefaultClass)
{
    AHerbalistEntityActor* Existing = Cell.ManifestedEntityActor.Get();
    if (Existing && Existing->GetEntityID() == Cell.ManifestedEntityID)
    {
        return;   // уже актуален, ничего менять не нужно
    }

    if (Existing)
    {
        Existing->Destroy();
        Cell.ManifestedEntityActor.Reset();
    }

    if (Cell.ManifestedEntityID.IsNone() || !GetWorld()) return;

    const TSubclassOf<AHerbalistEntityActor> ClassToSpawn = RequestedClass ? RequestedClass : DefaultClass;
    // Позиция внутри формы биома (2026-09-02), не мёртвый центр клетки --
    // тот же приём, что уже SpawnResourcesInCell, см. GetSpawnPositionWithinBiome.
    const UHerbalistSettings* JitterSettings = GetHerbalistSettings();
    const float JitterRadius = JitterSettings ? JitterSettings->EntityManifestationJitterRadius : 30.0f;
    const FVector SpawnPos = GetSpawnPositionWithinBiome(Cell.X, Cell.Y, JitterRadius, WorldRNG);
    AHerbalistEntityActor* NewActor = GetWorld()->SpawnActor<AHerbalistEntityActor>(ClassToSpawn, SpawnPos, FRotator::ZeroRotator);
    if (NewActor)
    {
        NewActor->Init(Cell.ManifestedEntityID, FIntPoint(Cell.X, Cell.Y), this);
        Cell.ManifestedEntityActor = NewActor;
    }
}

// ============================================================================
// ГЛАВНЫЙ ТИК ПРОЯВЛЕНИЙ
// ============================================================================

void AGridWorldManager::UpdateEntityManifestations(float DeltaTime)
{
    const UHerbalistSettings* Settings = GetHerbalistSettings();
    const float GnilnikiThreshold   = Settings ? Settings->GnilnikiCorruptionThreshold      : 0.6f;
    const float GnilnikiNudgeRate   = Settings ? Settings->GnilnikiNudgeRate                : 0.01f;
    const float HistoryPurityRate   = Settings ? Settings->HistoryPurityLerpRate            : 0.02f;
    const float BereginyaThreshold  = Settings ? Settings->BereginyaHistoryPurityThreshold  : 0.75f;
    const float BereginyaShrineThreshold = Settings ? Settings->BereginyaShrineRestorationThreshold : 0.7f;
    const int32 ShrineInfluenceRadius = Settings ? Settings->ShrineInfluenceRadius          : 3;
    const float NightHorrorDistortionRate = Settings ? Settings->NightHorrorDistortionRate  : 0.003f;
    const float NightHorrorCorruptionRate = Settings ? Settings->NightHorrorCorruptionRate  : 0.002f;
    const float NightHorrorSpiritRate     = Settings ? Settings->NightHorrorSpiritRate      : 0.003f;
    const float WinterPurityRate = Settings ? Settings->WinterPurityRate                    : 0.002f;
    const bool bIsWinter = GetSeason() == ESeason::Winter;   // одинаково для всей сетки за этот тик
    const float DawnPurityRate      = Settings ? Settings->DawnPurityRate      : 0.004f;
    const float DawnStabilityRate   = Settings ? Settings->DawnStabilityRate   : 0.004f;
    const float DuskDistortionRate  = Settings ? Settings->DuskDistortionRate  : 0.005f;
    const float PoludnitsaDistortionRate = Settings ? Settings->PoludnitsaDistortionRate : 0.02f;
    const float HysteresisMargin    = Settings ? Settings->EntityManifestationHysteresis    : 0.05f;
    // Легендарный ранг (§16.4, LegendaryEntityTypes.h) читает MorokField
    // узла биом-графа, не Cell.State напрямую — нужна сама подсистема, не
    // только Settings. Может быть null в тестовом окружении без BeginPlay
    // геймлейна (см. FindGridWorldManager/InitializeFromAsset) — обрабатывается
    // как "легендарные молчат", не крашем, тот же принцип терпимости, что у
    // остальных внепайплайновых систем к отсутствующим данным.
    UBiomeGraphSubsystem* Graph = GetWorld() ? GetWorld()->GetSubsystem<UBiomeGraphSubsystem>() : nullptr;

    FStateDelta Delta;

    // ---- Низший (Гнильники) и Легендарный (Берегиня) — проход по всем клеткам ----
    for (FGridCell& Cell : Cells)
    {
        // Memory.HistoryPurity — медленная скользящая средняя от текущей Purity.
        // Поле уже существовало в FMemoryState, но нигде не обновлялось (см.
        // 16_Entity_Manifestation §16.4, упрощённый аналог Restoration капищ).
        //
        // Считается только для Речной поймы: это порог появления Берегини, и
        // больше HistoryPurity никто не читает. Раньше гонялось по всем клеткам.
        //
        // Сходимость через 1-exp(-rate*dt), а не rate*dt: линейная форма при
        // больших DeltaTime перелетает цель, а без DeltaTime вовсе (как было)
        // скорость сходимости зависела от FPS — порог Берегини достигался на
        // разных машинах за разное время.
        if (Cell.Biome == EBiomeType::Floodplain)
        {
            const float PurityAlpha = 1.0f - FMath::Exp(-HistoryPurityRate * DeltaTime);
            Cell.Memory.HistoryPurity = FMath::Lerp(Cell.Memory.HistoryPurity, Cell.State.Meta.Purity, PurityAlpha);
        }

        bool bChanged = false;
        FRealState NewTarget = Cell.TargetState;

        // --- Низший ранг: амбиентные зоны, §16.2, таблица определений в
        // AmbientEntityTypes.h. До 2026-08-29 биом был уникален на
        // определение (один Низший на биом), и цикл останавливался на первом
        // совпадении по Biome. С добавлением Ледяные духи/Суховейки/
        // Кувшинкины духи это перестало быть так — Степные огни (ночь) и
        // Суховейки (сезон) теперь оба претендуют на Степь и МОГУТ быть
        // оба формально "eligible" одновременно (летняя ночь). Больше НЕ
        // прерываем цикл на первом совпадении биома — проверяем все. Кто
        // первый в порядке объявления в реестре реально заявит
        // ManifestedEntityID, тот и "победил" на этот тик (CanManifest
        // отклонит второго claim той же клетки тем же рангом 0 — не
        // "больше приоритет", а "не выше") — эффект второго тихо не
        // применяется в этот тик, не складывается с первым. Осознанное
        // упрощение для v1: полноценное сложение эффектов нескольких
        // одноранговых Низших на одной клетке — отдельная задача, если
        // когда-нибудь понадобится.
        // Запоминаем, чьим ActorClass спавнить актора после цикла (2026-08-30)
        // — не более одного Def реально "выигрывает" клетку за тик (см.
        // комментарий выше про CanManifest), так что одного указателя хватает.
        const FAmbientEntityDefinition* ManifestingAmbientDef = nullptr;
        for (const FAmbientEntityDefinition& Def : GetAmbientEntityDefinitions())
        {
            if (Cell.Biome != Def.Biome) continue;
            if (Def.bLandOnly && Cell.bIsWater) continue;
            if (Def.bWaterOnly && !Cell.bIsWater) continue;

            const bool bWasActive = Cell.ManifestedEntityID == Def.EntityID;

            bool bEligible = true;
            if (Def.TriggerAxis != EAmbientTriggerAxis::None)
            {
                // Гнильники читают порог/скорость из UHerbalistSettings (уже
                // настраивался балансировщиком раньше этой сессии, см. §2.1
                // AUDIT_AND_REFACTORING_PLAN.md) — остальные пока только из
                // своего определения; отдельная настройка на каждое существо
                // в Project Settings — задел на потом, не сегодняшняя задача.
                const bool bIsGnilniki = Def.EntityID == EntityID_Gnilniki;
                const float Threshold = bIsGnilniki ? GnilnikiThreshold : Def.TriggerThreshold;
                const float AxisValue = GetAmbientTriggerAxisValue(Cell, Def.TriggerAxis);
                const float SignedValue     = Def.bTriggerAbove ?  AxisValue  : -AxisValue;
                const float SignedThreshold = Def.bTriggerAbove ?  Threshold : -Threshold;
                bEligible = PassesHysteresisThreshold(bWasActive, SignedValue, SignedThreshold, Def.HysteresisMargin);
            }
            if (Def.bRequiresNight)
            {
                bEligible = bEligible && IsNight();
            }
            if (Def.bRequiresSeason)
            {
                bEligible = bEligible && (GetSeason() == Def.RequiredSeason);
            }
            if (Def.bRequiresDusk)
            {
                bEligible = bEligible && IsDusk();
            }
            if (Def.bRequiresMoonPhase)
            {
                bEligible = bEligible && (GetMoonPhase() == Def.RequiredMoonPhase);
            }
            if (Def.bRequiresBiomeBorder)
            {
                // Реальная проверка соседей, не прокси -- тот же приём, что
                // уже применён к Пограничному капищу (BiomeGraphSubsystem.cpp,
                // CollectBorderShrineDamping). Только 4 ортогональных соседа
                // (Chebyshev через угол не считается "границей биома" здесь,
                // тот же выбор, что у капища).
                bool bOnBorder = false;
                static const FIntPoint Offsets[4] = { FIntPoint(1, 0), FIntPoint(-1, 0), FIntPoint(0, 1), FIntPoint(0, -1) };
                for (const FIntPoint& Offset : Offsets)
                {
                    const FGridCell* Neighbor = GetCellConst(Cell.X + Offset.X, Cell.Y + Offset.Y);
                    if (Neighbor && Neighbor->Biome != Cell.Biome)
                    {
                        bOnBorder = true;
                        break;
                    }
                }
                bEligible = bEligible && bOnBorder;
            }
            if (Def.bRequiresWeather)
            {
                bEligible = bEligible && (Def.RequiredWeather == EWeatherCondition::Blizzard ? IsBlizzard() : IsWindy());
            }
            if (Def.bRequiresLateSummer)
            {
                bEligible = bEligible && IsLateSummer();
            }
            if (Def.bRequiresKupalaNight)
            {
                bEligible = bEligible && IsKupalaNight();
            }

            // Находка сессии 2026-08-30 (SystemInteractionTest — обход всего
            // бестиария): большинство проверенных Низших существ манифестируют
            // на клетке, которую бистабильность независимо зафиксировала на
            // испорченном полюсе (bDegrading), потому что их триггер-ось
            // ортогональна полюсу по дизайну и не спадает вместе с ней.
            // Существа, чья ось И направление согласованы с полюсом (Гнильники
            // и по итогам финального аудита — Водяные бесы/Болотные огни/
            // Стукачи по высокому Distortion, Ржавые духи по низкой Stability),
            // не гейтим — см. IsAxisConsistentWithCorruptPole выше.
            if (!IsAxisConsistentWithCorruptPole(Def.TriggerAxis, Def.bTriggerAbove))
            {
                bEligible = bEligible && !Cell.Memory.bDegrading;
            }

            // Перо Жар-птицы (16_Entity_Manifestation.md §16.4, 2026-09-02) —
            // клетка, помеченная навечно чистой, исключена из ЛЮБых
            // проявлений навсегда, тот же принцип, что !Cell.Memory.bDegrading
            // выше, но безусловный (не только для тематически несогласованных осей).
            bEligible = bEligible && !Cell.bEternallyPure;

            // Шапка-невидимка (21_Journey_And_Artifacts.md §21.3, 2026-09-01)
            // — подавляет только НОВЫЕ проявления, не уже активные (Гребень
            // снимает уже проявленное, это другой предмет). Перо Алконоста
            // (§16.4, 2026-09-02) — та же подавляющая проверка, но
            // масштабированная на конкретный биом клетки, не всю сетку.
            if (bEligible && CanManifest(Cell, Def.EntityID) &&
                (bWasActive || (!IsInvisibilityCapActive(FIntPoint(Cell.X, Cell.Y)) && !IsAlkonostSuppressionActiveForBiome(Cell.Biome))))
            {
                Cell.ManifestedEntityID = Def.EntityID;
                ManifestingAmbientDef = &Def;
                // Самоусиливающийся эффект — тянем TargetState дальше, чем
                // клетка уже есть, а не жёстко фиксируем; ApplyStateDelta/
                // RegenerateCellParameters сами доведут State до цели.
                // Ставки "в секунду" — умножаем на DeltaTime, иначе эффект
                // зависел бы от FPS (тот же баг, что чинили у Гнильников
                // раньше этой сессии, §1.1 AUDIT_AND_REFACTORING_PLAN.md).
                // У Гнильников ставка по-прежнему приходит из
                // UHerbalistSettings (переопределяет числа определения
                // целиком, а не умножается на них — иначе получилось бы
                // rate*rate), у всех прочих — из самого определения.
                const bool bIsGnilniki = Def.EntityID == EntityID_Gnilniki;
                const float CorruptionRate = bIsGnilniki ? GnilnikiNudgeRate        : Def.CorruptionRate;
                const float PurityRate     = bIsGnilniki ? -GnilnikiNudgeRate * 0.5f : Def.PurityRate;

                // Метим клетку грязной только если хоть одна ставка реально
                // ненулевая — иначе существо без Meta/Direction-эффекта
                // вовсе (Ржавые духи/Водяные бесы/Злыдни — их location-based
                // эффект порчи предметов отменён правкой пользователя
                // 2026-08-29, см. AmbientEntityTypes.h; сейчас манифестируются
                // без эффекта, заглушка на будущий редизайн) безусловно
                // попадало бы в Delta.TargetStateNudges каждый tick без
                // единого реального изменения — тот же класс бага, что уже
                // чинили в ApplyBiomeInfluences/ночном нудже
                // (AUDIT_AND_REFACTORING_PLAN.md §7.1).
                bool bAnyRateFired = false;
                if (CorruptionRate      != 0.0f) { NewTarget.Meta.Corruption = FMath::Clamp(NewTarget.Meta.Corruption + CorruptionRate      * DeltaTime, 0.0f, 1.0f); bAnyRateFired = true; }
                if (PurityRate          != 0.0f) { NewTarget.Meta.Purity     = FMath::Clamp(NewTarget.Meta.Purity     + PurityRate          * DeltaTime, 0.0f, 1.0f); bAnyRateFired = true; }
                if (Def.DistortionRate  != 0.0f) { NewTarget.Meta.Distortion = FMath::Clamp(NewTarget.Meta.Distortion + Def.DistortionRate  * DeltaTime, 0.0f, 1.0f); bAnyRateFired = true; }
                if (Def.StabilityRate   != 0.0f) { NewTarget.Meta.Stability  = FMath::Clamp(NewTarget.Meta.Stability  + Def.StabilityRate   * DeltaTime, 0.0f, 1.0f); bAnyRateFired = true; }
                if (Def.PotencyRate     != 0.0f) { NewTarget.Meta.Potency    = FMath::Clamp(NewTarget.Meta.Potency    + Def.PotencyRate     * DeltaTime, 0.0f, 1.0f); bAnyRateFired = true; }
                if (Def.ResonanceRate   != 0.0f) { NewTarget.Meta.Resonance  = FMath::Clamp(NewTarget.Meta.Resonance  + Def.ResonanceRate   * DeltaTime, 0.0f, 1.0f); bAnyRateFired = true; }
                if (Def.MagnitudeRate   != 0.0f) { NewTarget.Magnitude       = FMath::Clamp(NewTarget.Magnitude       + Def.MagnitudeRate   * DeltaTime, 0.0f, 1.0f); bAnyRateFired = true; }
                // Direction, не Meta -- Max(0, ...), не Clamp(0,1): тот же
                // принцип, что уже применяет ApplyLandmarkAxisNudge
                // (LandmarkTypes.h) к Direction-осям, не клампится в 1.0
                // сверху (NormalizeSum пересчитывает сумму отдельно при
                // релаксации, не здесь).
                if (Def.NatureRate      != 0.0f) { NewTarget.Direction.Nature = FMath::Max(0.0f, NewTarget.Direction.Nature + Def.NatureRate * DeltaTime); bAnyRateFired = true; }
                bChanged = bChanged || bAnyRateFired;
            }
            else if (bWasActive)
            {
                // Порог больше не пройден (с учётом гистерезиса) или клетку
                // отобрал более приоритетный хозяин — проекция прекращается.
                Cell.ManifestedEntityID = NAME_None;
            }
        }
        SyncManifestedEntityActor(Cell, ManifestingAmbientDef ? ManifestingAmbientDef->ActorClass : nullptr, AAmbientEntityActor::StaticClass());

        // --- Берегиня: Легендарный, порог мирового состояния, Речная пойма ---
        if (Cell.Biome == EBiomeType::Floodplain && Cell.bIsWater)
        {
            const bool bWasActive = Cell.ManifestedEntityID == EntityID_Bereginya;

            // Два независимых пути к тому же триггеру (16_Entity_Manifestation
            // §16.4: "устойчиво низкий Distortion... ИЛИ высокая Restoration
            // капища поблизости — как уже спроектировано для Берегини"). До
            // аудита 2026-08-24 был только HistoryPurity — "упрощённая версия
            // без капищ" (см. комментарий у BereginyaHistoryPurityThreshold в
            // HerbalistSettings.h), написанная ДО того, как капища появились
            // в проекте. Теперь появились — добавляем второй путь, не убирая
            // первый: это разные, оба валидные, сигналы ("вода долго была
            // чистой сама по себе" vs "рядом ухоженное капище"), не дубли.
            const bool bHistoryEligible = PassesHysteresisThreshold(bWasActive, Cell.Memory.HistoryPurity, BereginyaThreshold, HysteresisMargin);
            const float ShrineInfluence = HerbalistCore::Shrine::GetInfluenceAt(FIntPoint(Cell.X, Cell.Y), Shrines, ShrineInfluenceRadius);
            const bool bShrineEligible = PassesHysteresisThreshold(bWasActive, ShrineInfluence, BereginyaShrineThreshold, HysteresisMargin);
            // Тот же гейт, что у Низшего ранга выше — Берегиня целиком
            // "чистая" по конструкции (порог по HistoryPurity/Restoration),
            // без злого аналога, поэтому исключений внутри неё нет.
            const bool bEligible = (bHistoryEligible || bShrineEligible) && !Cell.Memory.bDegrading;

            if (bEligible && CanManifest(Cell, EntityID_Bereginya))
            {
                Cell.ManifestedEntityID = EntityID_Bereginya;
                // Благословлённая вода — Purity подтягивается к минимум 0.9, но
                // не выше: только floor, без верхнего клампа. Раньше здесь стоял
                // Clamp(..., 0.97) — если Purity УЖЕ была выше 0.97 (например, от
                // самой Заряны при варке), формула её тихо понижала, хотя смысл
                // был обратный — не мешать, а подтягивать вверх.
                if (NewTarget.Meta.Purity < 0.9f)
                {
                    NewTarget.Meta.Purity = 0.9f;
                }
                bChanged = true;
            }
            else if (bWasActive)
            {
                Cell.ManifestedEntityID = NAME_None;
            }
            // Берегиня жёстко закодирована в этом файле (не через реестр
            // LegendaryEntityTypes.h, см. GetEntityManifestationPriority выше),
            // поэтому у неё нет своего FLegendaryEntityDefinition::ActorClass —
            // всегда базовый класс ранга, без per-существо переопределения.
            SyncManifestedEntityActor(Cell, nullptr, ALegendaryEntityActor::StaticClass());
        }

        // --- Опасная нечисть, §16.5: сквозная ночная фаза ---
        // Вурдалаки/Навьи/Оборотни/Лихоманки/Черти — единственная категория
        // бестиария без привязки к биому (biome: Повсеместно). Спецификация
        // прямо говорит: "прямая привязка к уже спроектированной ночной фазе
        // — даёт ночному окну реальные зубы за пределами атмосферы". Это не
        // "хозяин" клетки (не претендует на ManifestedEntityID/CanManifest —
        // Навьи не "владеют" клеткой так, как Гнильники владеют болотом,
        // они разлиты по всей карте разом, пока держится ночь) — плюс
        // нудж мельче любого одиночного Низшего: он не выбирает конкретные
        // клетки, а идёт по всей сетке сразу, и это не должно перекрывать
        // сигнал от локальных существ.
        //
        // Сравниваем с уже стоящим значением перед записью (не просто "ночь
        // идёт, значит меняем") — тот же принцип, что уже применён в
        // ApplyBiomeInfluences после аудита 2026-08-24 (§7.1
        // AUDIT_AND_REFACTORING_PLAN.md): без этого клетка у потолка/пола
        // (Distortion/Corruption уже 1.0) всё равно помечалась бы грязной
        // каждый тик безусловно, хотя реального сдвига нет.
        if (IsNight())
        {
            const float NewDistortion = FMath::Clamp(NewTarget.Meta.Distortion + NightHorrorDistortionRate * DeltaTime, 0.0f, 1.0f);
            const float NewCorruption = FMath::Clamp(NewTarget.Meta.Corruption + NightHorrorCorruptionRate * DeltaTime, 0.0f, 1.0f);
            if (!FMath::IsNearlyEqual(NewDistortion, NewTarget.Meta.Distortion, KINDA_SMALL_NUMBER) ||
                !FMath::IsNearlyEqual(NewCorruption, NewTarget.Meta.Corruption, KINDA_SMALL_NUMBER))
            {
                NewTarget.Meta.Distortion = NewDistortion;
                NewTarget.Meta.Corruption = NewCorruption;
                bChanged = true;
            }

            // §15.2, третья часть той же строки таблицы ("усиление оси Spirit
            // в Direction"), не реализованная вместе с Distortion/Corruption
            // выше 2026-08-29 — "полночь... самое сильное окно для
            // духовно-ориентированных трав". Direction, не Meta -- тот же
            // принцип Max(0, ...) без верхнего клампа, что уже применяют
            // ApplyLandmarkAxisNudge и NatureRate-нудж Низших выше в этом
            // файле: NormalizeSum пересчитывает сумму отдельно при
            // релаксации, здесь клампить в 1.0 незачем.
            const float NewSpirit = FMath::Max(0.0f, NewTarget.Direction.Spirit + NightHorrorSpiritRate * DeltaTime);
            if (!FMath::IsNearlyEqual(NewSpirit, NewTarget.Direction.Spirit, KINDA_SMALL_NUMBER))
            {
                NewTarget.Direction.Spirit = NewSpirit;
                bChanged = true;
            }
        }

        // --- Рассвет, §15.2: "мир на короткое время выдыхает" ---
        // Единственная "улучшающая" фаза суток — лучшее окно для сбора
        // чистых ингредиентов. Тот же разлитый-по-сетке паттерн и та же
        // проверка перед записью, что у ночного/зимнего нуджа.
        if (IsDawn())
        {
            const float NewPurity    = FMath::Clamp(NewTarget.Meta.Purity    + DawnPurityRate    * DeltaTime, 0.0f, 1.0f);
            const float NewStability = FMath::Clamp(NewTarget.Meta.Stability + DawnStabilityRate * DeltaTime, 0.0f, 1.0f);
            if (!FMath::IsNearlyEqual(NewPurity, NewTarget.Meta.Purity, KINDA_SMALL_NUMBER) ||
                !FMath::IsNearlyEqual(NewStability, NewTarget.Meta.Stability, KINDA_SMALL_NUMBER))
            {
                NewTarget.Meta.Purity    = NewPurity;
                NewTarget.Meta.Stability = NewStability;
                bChanged = true;
            }
        }

        // --- Закат, §15.2: "ни день, ни ночь" ---
        // "+Distortion (нарастающее)" — ставка растёт линейно от 0 на входе
        // в Закат до полной силы у порога Ночи (GetDuskProgress01) вместо
        // мгновенного включения, как у Ночи/Рассвета: Морок начинает
        // просыпаться раньше, чем стемнеет, но не сразу в полную силу.
        if (IsDusk())
        {
            const float NewDistortion = FMath::Clamp(NewTarget.Meta.Distortion + DuskDistortionRate * GetDuskProgress01() * DeltaTime, 0.0f, 1.0f);
            if (!FMath::IsNearlyEqual(NewDistortion, NewTarget.Meta.Distortion, KINDA_SMALL_NUMBER))
            {
                NewTarget.Meta.Distortion = NewDistortion;
                bChanged = true;
            }
        }

        // --- Полудница, §15.2 "Полдень как отдельная опасность" ---
        // Буквальная реализация уже существующей в бестиарии карточки
        // (демон полудня, наказывающий работающих в жаре), декоративной до
        // сих пор — здесь получает игровые зубы. Только открытые биомы
        // (Степь, Лесостепь), короткое (~2 мин) окно в середине Дня.
        if ((Cell.Biome == EBiomeType::Steppe || Cell.Biome == EBiomeType::ForestSteppe) && IsPoludnitsaWindow())
        {
            const float NewDistortion = FMath::Clamp(NewTarget.Meta.Distortion + PoludnitsaDistortionRate * DeltaTime, 0.0f, 1.0f);
            if (!FMath::IsNearlyEqual(NewDistortion, NewTarget.Meta.Distortion, KINDA_SMALL_NUMBER))
            {
                NewTarget.Meta.Distortion = NewDistortion;
                bChanged = true;
            }
        }

        // --- Зима, §15.4: "снег как чистота" ---
        // "Purity растёт, хотя мир опаснее всего... не баг баланса, а прямое
        // использование того, что Purity и Corruption — независимые оси:
        // зима честно самая чистая и самая опасная одновременно". Тот же
        // разлитый-по-сетке паттерн, что ночной нудж выше — независимо от
        // ManifestedEntityID, коэффициенты складываются (зимняя ночь получает
        // оба одновременно, и это осознанно, не коллизия). Та же проверка
        // перед записью, что и у ночного нуджа — не метить клетку у потолка.
        if (bIsWinter)
        {
            const float NewPurity = FMath::Clamp(NewTarget.Meta.Purity + WinterPurityRate * DeltaTime, 0.0f, 1.0f);
            if (!FMath::IsNearlyEqual(NewPurity, NewTarget.Meta.Purity, KINDA_SMALL_NUMBER))
            {
                NewTarget.Meta.Purity = NewPurity;
                bChanged = true;
            }
        }

        if (bChanged)
        {
            Delta.TargetStateNudges.Add(FIntPoint(Cell.X, Cell.Y), NewTarget);
        }
    }

    // ---- Основной (Полевик и далее §16.3) — проход по клеткам-обиталищам ----
    // Respect больше НЕ растёт/падает пассивно от состояния клетки — тот
    // подход был упрощением конкретно Полевика, разошедшимся со спецификацией
    // §16.3 ("подношение/уважение", не амбиентное условие). 2026-08-29,
    // по прямому решению пользователя: Respect меняется только через
    // подношение — Apply-на-клетку-обиталище, тот же канал и тот же принцип
    // знака (Purity−Corruption), что уже есть у капищ (см. RunSimulationStep,
    // GridWorldManagerTick.cpp), но БЕЗ капищного спада при небрежении —
    // капище "забывается" как структура, хозяин места — нет, у подношения
    // ему нет срока годности.
    for (FEntityLandmark& Landmark : EntityLandmarks)
    {
        FGridCell* Cell = GetCell(Landmark.Cell.X, Landmark.Cell.Y);
        if (!Cell) continue;

        // Если Гнильники/Берегиня (первый цикл выше) уже поставили нудж на
        // эту же клетку — берём его как отправную точку, а не сырой
        // Cell->TargetState. Иначе второй TMap::Add молча перезаписал бы
        // первый нудж целиком (AUDIT_AND_REFACTORING_PLAN §3.6): сегодня
        // Полевик и Гнильники живут на непересекающихся биомах и коллизии
        // не бывает, но это заложенная мина на будущих "хозяев" §16.3
        // (Кикимора болотная — прямой кандидат делить клетку с Гнильниками).
        FRealState NewTarget = Delta.TargetStateNudges.FindRef(Landmark.Cell, Cell->TargetState);
        bool bChanged = false;

        // Respect отходит от нуля медленно (Gain/DecayRate * DeltaTime) и должен
        // пройти через нейтральную зону (-0.3..0.5), прежде чем сменить знак —
        // поэтому его текущий знак надёжно указывает, к какому порогу гистерезиса
        // (благословение или порча) относится bWasActive, без отдельного флага.
        const bool bWasActive = Cell->ManifestedEntityID == Landmark.EntityID;
        const bool bWasBlessed = bWasActive && Landmark.Respect >= 0.0f;
        const bool bWasCursed  = bWasActive && Landmark.Respect < 0.0f;
        // Тот же гейт, что у Низшего/Берегини — благословенный Хозяин
        // ортогонален Corruption (Respect меняется только подношением) и
        // застревал бы на клетке, которую бистабильность уже зафиксировала
        // испорченной. Проклятие не гейтим — оно тематически согласовано с
        // испорченным полюсом, тот же принцип, что у Гнильников/Злого полюса.
        // Перо Жар-птицы (§16.4, 2026-09-02) — навечно чистая клетка
        // исключена из этого ранга тоже (не только Низшего/Легендарного, за
        // которыми уже следит Шапка/Алконост) — полная неприкосновенность,
        // не только защита от амбиентной угрозы.
        const bool bBlessEligible = PassesHysteresisThreshold(bWasBlessed, Landmark.Respect, 0.5f, HysteresisMargin) && !Cell->Memory.bDegrading && !Cell->bEternallyPure;
        const bool bCurseEligible = PassesHysteresisThreshold(bWasCursed, -Landmark.Respect, 0.3f, HysteresisMargin) && !Cell->bEternallyPure;

        // Какую ось благословлять/проклинать и с какой скоростью — из
        // реестра §16.3 (LandmarkTypes.h), не захардкожено на Полевика.
        // Отсутствие определения (ещё не заведённый "хозяин") — молчаливый
        // no-op, не крах: та же терпимость, что у AmbientEntityDefinition
        // к неизвестным EntityID.
        const FLandmarkDefinition* Def = FindLandmarkDefinition(Landmark.EntityID);

        if (bBlessEligible && Def && CanManifest(*Cell, Landmark.EntityID))
        {
            ApplyLandmarkAxisNudge(NewTarget, Def->BlessAxis,  Def->BlessRate  * DeltaTime);
            ApplyLandmarkAxisNudge(NewTarget, Def->BlessAxis2, Def->BlessRate2 * DeltaTime);
            Cell->ManifestedEntityID = Landmark.EntityID;
            bChanged = true;
        }
        else if (bCurseEligible && Def && CanManifest(*Cell, Landmark.EntityID))
        {
            ApplyLandmarkAxisNudge(NewTarget, Def->CurseAxis, Def->CurseRate * DeltaTime);
            // Отягощённое проклятие (§ комментарий у AggravatedCurseThreshold
            // в LandmarkTypes.h) — второй, более резкий удар поверх обычного
            // curse, когда Respect провалился существенно ниже обычного
            // порога, не просто пересёк его. Домовой: эскалация в домашнюю
            // Кикимору тем же приёмом, что уже применяется к Гнильникам/
            // прочим порогам, не отдельным событийным триггером.
            if (Landmark.Respect < Def->AggravatedCurseThreshold)
            {
                ApplyLandmarkAxisNudge(NewTarget, Def->AggravatedCurseAxis, Def->AggravatedCurseRate * DeltaTime);
            }
            Cell->ManifestedEntityID = Landmark.EntityID;
            bChanged = true;
        }
        else if (bWasActive)
        {
            // Нейтральная зона (с учётом гистерезиса) или клетку отобрал более
            // приоритетный хозяин — проекция прекращается.
            Cell->ManifestedEntityID = NAME_None;
        }

        if (bChanged)
        {
            Delta.TargetStateNudges.Add(Landmark.Cell, NewTarget);
        }
        SyncManifestedEntityActor(*Cell, Def ? Def->ActorClass : nullptr, ALandmarkEntityActor::StaticClass());
    }

    // ---- Легендарный (реестр LegendaryEntityTypes.h) — проход по якорным
    // клеткам, тем же принципом, что Основной выше (одна клетка на
    // существо, не "весь биом разом"). Сигнал триггера всё равно на уровне
    // биом-графа (MorokField узла, общий на все клетки биома) — но эффект
    // применяется только к SeedLegendaryAnchors'ом назначенной клетке,
    // иначе первое же срабатывание условия нуджило бы TargetState разом на
    // 30+ клетках одного биома (найдено и починено до коммита: интеграционный
    // тест на реальный Tick() с BiomeGraphSubsystem — Herbalist.BiomeGraph.
    // RealTickKeepsDirtyCellsSparse — сразу поймал 243/400 грязных клеток на
    // первой версии, где Легендарный жил внутри общего цикла по Cells). ----
    if (Graph)
    {
        for (const FLegendaryEntityDefinition& Def : GetLegendaryEntityDefinitions())
        {
            const FIntPoint* Anchor = LegendaryAnchors.Find(Def.EntityID);
            if (!Anchor) continue;   // биом без подходящей клетки в этой сетке -- молчаливый no-op

            FGridCell* Cell = GetCell(Anchor->X, Anchor->Y);
            if (!Cell) continue;

            const FName BiomeID = FBiomeDefaults::BiomeTypeToName(Def.Biome);
            const FBiomeGraphNode* Node = Graph->GetNode(BiomeID);
            if (!Node) continue;

            FRealState NewTarget = Delta.TargetStateNudges.FindRef(*Anchor, Cell->TargetState);
            bool bChanged = false;

            const bool bWasActive = Cell->ManifestedEntityID == Def.EntityID;
            bool bEligible = false;
            if (Def.Pole == ELegendaryPole::Malign)
            {
                bEligible = PassesHysteresisThreshold(bWasActive, Node->MorokField, Def.MorokThreshold, HysteresisMargin);
            }
            else
            {
                // "Устойчиво низкий MorokField" -- тот же приём инверсии
                // знака, что уже применяет §16.2 для bTriggerAbove=false
                // (AmbientEntityTypes.h): сравниваем отрицания, не пишем
                // отдельную "ниже порога" версию PassesHysteresisThreshold.
                const bool bMorokEligible = PassesHysteresisThreshold(bWasActive, -Node->MorokField, -Def.MorokThreshold, HysteresisMargin);
                bool bShrineEligible = false;
                if (Def.bHasShrinePath)
                {
                    const float ShrineInfluence = HerbalistCore::Shrine::GetInfluenceAt(*Anchor, Shrines, ShrineInfluenceRadius);
                    bShrineEligible = PassesHysteresisThreshold(bWasActive, ShrineInfluence, Def.ShrineThreshold, HysteresisMargin);
                }
                // Тот же гейт, что у остальных рангов — Благой полюс
                // ортогонален Corruption клетки (читает MorokField графа, не
                // Cell.State), Злой полюс не гейтим (высокий MorokField уже
                // тематически согласован с испорченным полюсом).
                bEligible = (bMorokEligible || bShrineEligible) && !Cell->Memory.bDegrading;
            }

            // Перо Жар-птицы (§16.4, 2026-09-02) — тот же безусловный гейт,
            // что уже применён к Низшему/Основному выше, для обоих полюсов
            // разом (Malign и Benign).
            bEligible = bEligible && !Cell->bEternallyPure;

            // Шапка-невидимка (21_Journey_And_Artifacts.md §21.3, 2026-09-01)
            // — подавляет только НОВЫЕ проявления (bWasActive уже true
            // проходит как раньше), "не даёт проявиться", не снимает уже
            // проявленное (это Гребень, UseCombOnCell). Перо Алконоста
            // (§16.4, 2026-09-02) — та же подавляющая проверка, масштабированная
            // на биом якорной клетки, не всю сетку.
            if (bEligible && CanManifest(*Cell, Def.EntityID) &&
                (bWasActive || (!IsInvisibilityCapActive(*Anchor) && !IsAlkonostSuppressionActiveForBiome(Cell->Biome))))
            {
                ApplyLandmarkAxisNudge(NewTarget, Def.EffectAxis,  Def.EffectRate  * DeltaTime);
                ApplyLandmarkAxisNudge(NewTarget, Def.EffectAxis2, Def.EffectRate2 * DeltaTime);
                Cell->ManifestedEntityID = Def.EntityID;
                bChanged = true;
            }
            else if (bWasActive)
            {
                Cell->ManifestedEntityID = NAME_None;
            }

            if (bChanged)
            {
                Delta.TargetStateNudges.Add(*Anchor, NewTarget);
            }
            SyncManifestedEntityActor(*Cell, Def.ActorClass, ALegendaryEntityActor::StaticClass());
        }
    }

    if (Delta.TargetStateNudges.Num() > 0)
    {
        ApplyStateDelta(Delta);
    }
}
