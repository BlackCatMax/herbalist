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
#include "Core/Simulation/Public/DeltaTypes.h"
#include "Core/Types/HerbalistCoreMath.h"
#include "ProjectHerbalist.h"
#include "HerbalistLogChannels.h"

using HerbalistCore::Math::PassesHysteresisThreshold;

namespace
{
    const FName EntityID_Gnilniki(TEXT("Гнильники"));
    const FName EntityID_Polevik(TEXT("Полевик"));
    const FName EntityID_Bereginya(TEXT("Берегиня"));

    // Ранг бестиария (см. заголовок файла: Низший/Основной/Легендарный) решает,
    // кто вытесняет кого при совпадении условий в одной клетке (DESIGN_World_State.md
    // §14) — "вытеснение вместо миграции": никто не переезжает, просто более
    // редкая/сильная сущность выигрывает проекцию, если условия обеих выполнены
    // одновременно. Выше число — выше приоритет.
    int32 GetEntityManifestationPriority(FName EntityID)
    {
        if (EntityID == EntityID_Bereginya) return 2;  // Легендарный
        if (EntityID == EntityID_Polevik)   return 1;  // Основной
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

    // Полевик: первая найденная клетка Лесостепи, не водная.
    for (const FGridCell& Cell : Cells)
    {
        if (Cell.Biome == EBiomeType::ForestSteppe && !Cell.bIsWater)
        {
            FEntityLandmark Landmark;
            Landmark.EntityID = EntityID_Polevik;
            Landmark.Cell = FIntPoint(Cell.X, Cell.Y);
            Landmark.Respect = 0.0f;
            EntityLandmarks.Add(Landmark);
            UE_LOG(LogHerbalistWorld, Log, TEXT("[Entities] Seeded landmark %s at (%d,%d)"),
                *EntityID_Polevik.ToString(), Cell.X, Cell.Y);
            break;
        }
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
    const float WinterPurityRate = Settings ? Settings->WinterPurityRate                    : 0.002f;
    const bool bIsWinter = GetSeason() == ESeason::Winter;   // одинаково для всей сетки за этот тик
    const float RespectGainRate     = Settings ? Settings->LandmarkRespectGainRate          : 0.01f;
    const float RespectDecayRate    = Settings ? Settings->LandmarkRespectDecayRate         : 0.02f;
    const float StressAngerThreshold= Settings ? Settings->LandmarkStressAngerThreshold     : 0.6f;
    const float HysteresisMargin    = Settings ? Settings->EntityManifestationHysteresis    : 0.05f;

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
        // AmbientEntityTypes.h. Гнильники, Моховые духи, Степные огни — по
        // построению у каждого свой Biome, так что на одну клетку может
        // претендовать не больше одного определения зараз (см. комментарий
        // в самом файле определений); при добавлении нового с уже занятым
        // Biome эту гарантию придётся пересмотреть явно, не молча.
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
                const float AxisValue = GetAmbientTriggerAxisValue(Cell.State.Meta, Def.TriggerAxis);
                const float SignedValue     = Def.bTriggerAbove ?  AxisValue  : -AxisValue;
                const float SignedThreshold = Def.bTriggerAbove ?  Threshold : -Threshold;
                bEligible = PassesHysteresisThreshold(bWasActive, SignedValue, SignedThreshold, Def.HysteresisMargin);
            }
            if (Def.bRequiresNight)
            {
                bEligible = bEligible && IsNight();
            }

            if (bEligible && CanManifest(Cell, Def.EntityID))
            {
                Cell.ManifestedEntityID = Def.EntityID;
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

                if (CorruptionRate      != 0.0f) NewTarget.Meta.Corruption = FMath::Clamp(NewTarget.Meta.Corruption + CorruptionRate      * DeltaTime, 0.0f, 1.0f);
                if (PurityRate          != 0.0f) NewTarget.Meta.Purity     = FMath::Clamp(NewTarget.Meta.Purity     + PurityRate          * DeltaTime, 0.0f, 1.0f);
                if (Def.DistortionRate  != 0.0f) NewTarget.Meta.Distortion = FMath::Clamp(NewTarget.Meta.Distortion + Def.DistortionRate  * DeltaTime, 0.0f, 1.0f);
                if (Def.StabilityRate   != 0.0f) NewTarget.Meta.Stability  = FMath::Clamp(NewTarget.Meta.Stability  + Def.StabilityRate   * DeltaTime, 0.0f, 1.0f);
                bChanged = true;
            }
            else if (bWasActive)
            {
                // Порог больше не пройден (с учётом гистерезиса) или клетку
                // отобрал более приоритетный хозяин — проекция прекращается.
                Cell.ManifestedEntityID = NAME_None;
            }
            break; // Biome уникален на определение (см. комментарий выше) — нашли, хватит.
        }

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
            const bool bEligible = bHistoryEligible || bShrineEligible;

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
        if (IsNight())
        {
            NewTarget.Meta.Distortion = FMath::Clamp(NewTarget.Meta.Distortion + NightHorrorDistortionRate * DeltaTime, 0.0f, 1.0f);
            NewTarget.Meta.Corruption = FMath::Clamp(NewTarget.Meta.Corruption + NightHorrorCorruptionRate * DeltaTime, 0.0f, 1.0f);
            bChanged = true;
        }

        // --- Зима, §15.4: "снег как чистота" ---
        // "Purity растёт, хотя мир опаснее всего... не баг баланса, а прямое
        // использование того, что Purity и Corruption — независимые оси:
        // зима честно самая чистая и самая опасная одновременно". Тот же
        // разлитый-по-сетке паттерн, что ночной нудж выше — независимо от
        // ManifestedEntityID, коэффициенты складываются (зимняя ночь получает
        // оба одновременно, и это осознанно, не коллизия).
        if (bIsWinter)
        {
            NewTarget.Meta.Purity = FMath::Clamp(NewTarget.Meta.Purity + WinterPurityRate * DeltaTime, 0.0f, 1.0f);
            bChanged = true;
        }

        if (bChanged)
        {
            Delta.TargetStateNudges.Add(FIntPoint(Cell.X, Cell.Y), NewTarget);
        }
    }

    // ---- Основной (Полевик) — проход по клеткам-обиталищам ----
    for (FEntityLandmark& Landmark : EntityLandmarks)
    {
        FGridCell* Cell = GetCell(Landmark.Cell.X, Landmark.Cell.Y);
        if (!Cell) continue;

        // Бережное обращение (низкий стресс, высокая чистота места) поднимает
        // Respect; истощение клетки — опускает. Та же формула-дух, что и у
        // Restoration капищ (15_Cycles_And_Shrines §15.5), но без завязки на
        // конкретную варку — Полевик реагирует на состояние поля в целом.
        if (Cell->HarvestStress > StressAngerThreshold)
        {
            Landmark.Respect = FMath::Clamp(Landmark.Respect - RespectDecayRate * DeltaTime, -1.0f, 1.0f);
        }
        else if (Cell->State.Meta.Purity > 0.6f)
        {
            Landmark.Respect = FMath::Clamp(Landmark.Respect + RespectGainRate * DeltaTime, -1.0f, 1.0f);
        }

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
        const bool bBlessEligible = PassesHysteresisThreshold(bWasBlessed, Landmark.Respect, 0.5f, HysteresisMargin);
        const bool bCurseEligible = PassesHysteresisThreshold(bWasCursed, -Landmark.Respect, 0.3f, HysteresisMargin);

        if (bBlessEligible && CanManifest(*Cell, Landmark.EntityID))
        {
            // Благословлённое зерно (08_Content/бестиарий: "повышенная Potency и Purity").
            NewTarget.Meta.Potency = FMath::Clamp(NewTarget.Meta.Potency + RespectGainRate * DeltaTime, 0.0f, 1.0f);
            NewTarget.Meta.Purity  = FMath::Clamp(NewTarget.Meta.Purity + RespectGainRate * DeltaTime * 0.5f, 0.0f, 1.0f);
            Cell->ManifestedEntityID = Landmark.EntityID;
            bChanged = true;
        }
        else if (bCurseEligible && CanManifest(*Cell, Landmark.EntityID))
        {
            // "Порча посевов, наведение усталости" — переведено в Stability, а не
            // в новую придуманную ось "усталости".
            NewTarget.Meta.Stability = FMath::Clamp(NewTarget.Meta.Stability - RespectDecayRate * DeltaTime, 0.0f, 1.0f);
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
    }

    if (Delta.TargetStateNudges.Num() > 0)
    {
        ApplyStateDelta(Delta);
    }
}
