// HerbalistSettings.h
#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "HerbalistSettings.generated.h"

UCLASS(config = Game, defaultconfig, meta = (DisplayName = "Herbalist Settings"))
class PROJECTHERBALIST_API UHerbalistSettings : public UDeveloperSettings
{
    GENERATED_BODY()

public:
    UHerbalistSettings();

    // --- Pipeline coefficients ---
    UPROPERTY(config, EditAnywhere, Category = "Pipeline|Biome Context", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float BiomeZaryanaInfluence = 0.3f;

    UPROPERTY(config, EditAnywhere, Category = "Pipeline|Biome Context", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float BiomeAxisDriftWeight = 0.1f;

    // --- Water blending ---
    UPROPERTY(config, EditAnywhere, Category = "Pipeline|Water", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float MaxWaterRatio = 0.8f;

    UPROPERTY(config, EditAnywhere, Category = "Pipeline|Water", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float WaterDilutionPenalty = 0.2f;

    // --- Fold ---
    UPROPERTY(config, EditAnywhere, Category = "Pipeline|Fold", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float FoldWeightDecay = 0.8f;

    // --- Morok ---
    // Давление Морока: во сколько раз сильна его хватка при полном EffectiveMorok
    // и нулевой Stability. Входит показателем степени (PipelineV2::ComputeApplyResult),
    // а не слагаемым, поэтому диапазон шире 1.0.
    //
    // Переименовано из MorokMixStrengthFactor: прежнее имя врало — осевым
    // смешиванием (ApplyMorokAxisMix) этот коэффициент никогда не управлял,
    // туда идёт EffectiveMorok напрямую. Смысл тоже изменился вместе с формулой,
    // поэтому имя менять было обязательно, а не косметически.
    UPROPERTY(config, EditAnywhere, Category = "Pipeline|Morok", meta = (ClampMin = "0.0", ClampMax = "4.0"))
    float MorokPressure = 1.0f;

    // --- Zaryana ---
    UPROPERTY(config, EditAnywhere, Category = "Pipeline|Zaryana", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float ZaryanaBoostFactor = 0.5f;

    UPROPERTY(config, EditAnywhere, Category = "Pipeline|Zaryana", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float ZaryanaSuppressFactor = 0.3f;

    // --- Harvest ---
    // Насколько собранная трава подтягивается к состоянию места:
    // 0 = трава ровно своя (BaseState), 1 = трава целиком становится местом.
    // Гасится сопротивляемостью ингредиента (IngredientTableRow::Resilience).
    // 0.4, а не 0.6: при 0.6 место перебивало вид, и разные травы в одном
    // биоме сходились друг к другу, теряя собственный характер.
    UPROPERTY(config, EditAnywhere, Category = "Harvest", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float HarvestBiomeWeight = 0.4f;

    // Насколько один сбор истощает клетку. Живёт здесь, а не на
    // AGridWorldManager, потому что читается из Pipeline (ProcessHarvestCommand),
    // которому до актора не дотянуться.
    UPROPERTY(config, EditAnywhere, Category = "Harvest", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float HarvestStressIncrement = 0.1f;

    // --- Время ---
    // Длительность игровых суток в реальных минутах. До этого в проекте не было
    // ни одной константы длины суток — только неиспользуемый ETimeOfDayMask.
    UPROPERTY(config, EditAnywhere, Category = "Time", meta = (ClampMin = "1.0"))
    float GameDayMinutes = 32.0f;

    // За сколько игровых суток клетка со стрессом 1.0 полностью зарастает
    // в биоме со множителем 1.0. Умножается на FBiomeRow::StressRecoveryMultiplier:
    // болото держит след дольше, пойма промывает быстрее.
    UPROPERTY(config, EditAnywhere, Category = "Time", meta = (ClampMin = "0.01"))
    float StressRecoveryGameDays = 7.0f;

    // Лунный цикл (15_Cycles_And_Shrines.md §15.3), v1 — только сбор.
    // Растущая: "усиливает Body/Nature и Magnitude собранного".
    UPROPERTY(config, EditAnywhere, Category = "Time|Moon", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float MoonWaxingBoostStrength = 0.15f;

    // Полнолуние: "усиливает Spirit, Potency, Resonance". Спецификация также
    // говорит, что вместе с силой растёт и Morok ("ставки выше в обе
    // стороны") — эта часть не реализована в v1 (влияет на Bifurcation/
    // варку, не на сбор), см. 16_Entity_Manifestation-подобную оговорку
    // "не всё сразу" в 15_Cycles_And_Shrines.md §15.6.
    UPROPERTY(config, EditAnywhere, Category = "Time|Moon", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float MoonFullBoostStrength = 0.15f;

    // --- Inventory Decay ---
    // Скорость порчи: увеличение Distortion в секунду при отсутствии Stability.
    // Умножается на (1 - Stability) предмета, т.е. стабильные предметы портятся медленнее.
    UPROPERTY(config, EditAnywhere, Category = "Inventory|Decay", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float InventoryDecayRate = 0.02f;

    // Множители InventoryDecayRate по типу контейнера (EStorageContainerType,
    // 2026-08-29) — единственное, что теперь модулирует порчу: "травы
    // портятся сами по себе, естественный процесс... на сохранность влияют
    // сами контейнеры хранения" (прямая правка пользователя, отменяет
    // прежнюю location-based защиту капищ и порчу от Ржавых духов/Водяных
    // бесов/Злыдней). Изначально заведены только два полюса (Basket/Cellar,
    // "инфраструктура + 2-3 примера"), три остальных (Sack/Cabinet/Jar)
    // добавлены тем же днём отдельным заходом ("проработка инвентаря и
    // систем хранения") — спектр от худшего к лучшему:
    // Sack(1.4) > Basket(1.3) > None(1.0) > Cabinet(0.7) > Cellar(0.4) > Jar(0.25).
    UPROPERTY(config, EditAnywhere, Category = "Inventory|Decay", meta = (ClampMin = "0.1", ClampMax = "5.0"))
    float BasketDecayMultiplier = 1.3f;

    // Мешок — хуже корзины: дерюга/рогожа держит влагу вместо проветривания,
    // и куда уязвимее для моли/вредителей, чем плетёная корзина.
    UPROPERTY(config, EditAnywhere, Category = "Inventory|Decay", meta = (ClampMin = "0.1", ClampMax = "5.0"))
    float SackDecayMultiplier = 1.4f;

    // Шкаф — лучше базовой линии (закрыт от пыли/вредителей), но не так
    // хорош, как погреб: комнатная температура и влажность, не стабильные.
    UPROPERTY(config, EditAnywhere, Category = "Inventory|Decay", meta = (ClampMin = "0.05", ClampMax = "1.0"))
    float CabinetDecayMultiplier = 0.7f;

    UPROPERTY(config, EditAnywhere, Category = "Inventory|Decay", meta = (ClampMin = "0.05", ClampMax = "1.0"))
    float CellarDecayMultiplier = 0.4f;

    // Банка (герметичная) — лучшее хранение из всех: почти нет доступа
    // воздуха/влаги, лучше даже погреба (тот всё равно дышит).
    UPROPERTY(config, EditAnywhere, Category = "Inventory|Decay", meta = (ClampMin = "0.01", ClampMax = "1.0"))
    float JarDecayMultiplier = 0.25f;

    // --- Entity Manifestation (02_GDD/16_Entity_Manifestation.md) — вертикальный срез ---
    // Гнильники (Низший, Болото): порог Corruption клетки для проявления зоны.
    UPROPERTY(config, EditAnywhere, Category = "Entities|Gnilniki", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float GnilnikiCorruptionThreshold = 0.6f;

    // На сколько зона Гнильников дополнительно тянет клетку к Corruption/Purity в секунду
    // (самоусиливающаяся порча места, ограничена клиппом в ApplyStateDelta).
    UPROPERTY(config, EditAnywhere, Category = "Entities|Gnilniki", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float GnilnikiNudgeRate = 0.01f;

    // LandmarkRespectGainRate/LandmarkRespectDecayRate удалены 2026-08-29,
    // тем же проходом, что и LandmarkStressAngerThreshold ниже: скорость
    // благословения/порчи теперь у каждого "хозяина" своя (BlessRate/
    // CurseRate в LandmarkTypes.h), не общая на всех через одну настройку.

    // LandmarkStressAngerThreshold удалена 2026-08-29: Respect "хозяев"
    // §16.3 больше не реагирует на HarvestStress клетки пассивно — только на
    // подношение (Apply-to-cell), см. LandmarkOfferingGain ниже.

    // Берегиня (Легендарный, Речная пойма) — порог Memory.HistoryPurity водной
    // клетки для проявления. Писалось до капищ как "упрощённая версия без
    // полноценной системы" (15_Cycles_And_Shrines §15.5) — теперь один из
    // двух независимых путей к триггеру, см. BereginyaShrineRestorationThreshold.
    UPROPERTY(config, EditAnywhere, Category = "Entities|Bereginya", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float BereginyaHistoryPurityThreshold = 0.75f;

    // Второй путь к тому же триггеру (16_Entity_Manifestation §16.4: "высокая
    // Restoration капища поблизости — как уже спроектировано для Берегини"),
    // добавлен 2026-08-24 после того, как капища перестали быть гипотетическими.
    // Использует тот же радиус, что ShrineInfluenceRadius ниже.
    UPROPERTY(config, EditAnywhere, Category = "Entities|Bereginya", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float BereginyaShrineRestorationThreshold = 0.7f;

    // §16.5 "Опасная нечисть — сквозная ночная фаза" (Вурдалаки/Навьи/
    // Оборотни/Лихоманки/Черти — единственная категория бестиария без
    // привязки к биому). Применяется ко ВСЕЙ сетке разом, пока держится
    // ночь (AGridWorldManager::IsNight), поэтому заведомо мельче ставок
    // одиночных Низших (AmbientEntityTypes.h) — иначе затопил бы их сигнал.
    UPROPERTY(config, EditAnywhere, Category = "Entities|NightHorror", meta = (ClampMin = "0.0", ClampMax = "0.1"))
    float NightHorrorDistortionRate = 0.003f;

    UPROPERTY(config, EditAnywhere, Category = "Entities|NightHorror", meta = (ClampMin = "0.0", ClampMax = "0.1"))
    float NightHorrorCorruptionRate = 0.002f;

    // Рассвет/Закат/Полудница (§15.2, 2026-08-29) — таблица суток раньше
    // была закрыта только на Ночь (AUDIT_AND_REFACTORING_PLAN.md §7.2).
    // Рассвет: "мир на короткое время выдыхает" — единственная улучшающая
    // фаза суток, зеркало ночного нуджа.
    UPROPERTY(config, EditAnywhere, Category = "Entities|DayCycle", meta = (ClampMin = "0.0", ClampMax = "0.1"))
    float DawnPurityRate = 0.004f;

    UPROPERTY(config, EditAnywhere, Category = "Entities|DayCycle", meta = (ClampMin = "0.0", ClampMax = "0.1"))
    float DawnStabilityRate = 0.004f;

    // Закат: "+Distortion (нарастающее)" — умножается на AGridWorldManager::
    // GetDuskProgress01() (0 на входе, 1 у порога Ночи), не применяется как есть.
    UPROPERTY(config, EditAnywhere, Category = "Entities|DayCycle", meta = (ClampMin = "0.0", ClampMax = "0.1"))
    float DuskDistortionRate = 0.005f;

    // Полудница: "Полдень как отдельная опасность", §15.2 — короткое (~2 мин)
    // окно, только Степь/Лесостепь, поэтому ставка заметно выше разлитых по
    // всей сетке (у неё меньше суммарного времени действия, чем у Ночи/Заката).
    UPROPERTY(config, EditAnywhere, Category = "Entities|DayCycle", meta = (ClampMin = "0.0", ClampMax = "0.2"))
    float PoludnitsaDistortionRate = 0.02f;

    // Годовой круг (15_Cycles_And_Shrines.md §15.4). Длина сезона в игровых
    // сутках — "самое условное число во всём разделе", менять свободно.
    // 117 = 13 недель × 9 суток/неделя (компендиум), три сезона = 351/год.
    UPROPERTY(config, EditAnywhere, Category = "Time|Season", meta = (ClampMin = "1.0"))
    float SeasonDurationDays = 117.0f;

    // Весна: "временный бонус к скорости зарастания клеток во всех биомах".
    // Множится на биомный StressRecoveryMultiplier, не заменяет его. ВАЖНО:
    // StressRecoveryMultiplier — множитель ВРЕМЕНИ полного восстановления
    // (см. GetStressDecay в GridWorldManagerCore.cpp: полное время =
    // RecoveryDays × DaySeconds × Multiplier), не скорости — поэтому "бонус
    // к скорости" здесь означает число МЕНЬШЕ единицы (короче срок), не
    // больше. Перепутано было при первой реализации 2026-08-24, поймано
    // тестом Herbalist.Season.SpringSpeedsUpStressRecoveryWinterSlowsIt
    // (диагностика показала Весну медленнее Лета, Зиму быстрее — ровно
    // наоборот тому, что требует спецификация).
    UPROPERTY(config, EditAnywhere, Category = "Time|Season", meta = (ClampMin = "0.1", ClampMax = "5.0"))
    float SpringStressRecoveryMultiplier = 0.7f;

    // Зима: "клетки заживают медленнее (сезонное ухудшение
    // StressRecoveryMultiplier поверх уже посчитанного биомного)" — здесь
    // "ухудшение" срока = число БОЛЬШЕ единицы (дольше срок), см. оговорку у
    // SpringStressRecoveryMultiplier выше.
    UPROPERTY(config, EditAnywhere, Category = "Time|Season", meta = (ClampMin = "1.0", ClampMax = "5.0"))
    float WinterStressRecoveryMultiplier = 1.6f;

    // Зима: "снег как чистота" — Purity растёт по всей сетке, пока держится
    // зима, независимо от локальных сущностей. Тот же порядок величины, что
    // NightHorror*Rate выше (разлито по всей сетке, не точечный эффект).
    UPROPERTY(config, EditAnywhere, Category = "Time|Season", meta = (ClampMin = "0.0", ClampMax = "0.1"))
    float WinterPurityRate = 0.002f;

    // --- Окна внутри сезона (2026-08-29) — Листовики/Купальские требуют
    // "осень"/конкретную ночь в году, которых в трёхсезонной модели нет как
    // отдельных понятий. Прямое решение пользователя: не заводить четвёртый
    // сезон/полноценный календарь, найти узкие окна внутри существующих
    // трёх сезонов через GetSeasonProgress01(). ---

    // Листовики: последняя доля Лета читается как "осень" (увядание перед
    // Зимой) — тот же смысловой промежуток года, что настоящая осень.
    UPROPERTY(config, EditAnywhere, Category = "Time|Season", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float LateSummerProgressThreshold = 0.8f;

    // Купальская ночь — узкое окно внутри Лета (не привязано к реальной
    // григорианской дате, у проекта свой календарь). [Start, End) по
    // GetSeasonProgress01() — интервал уже достаточно узкий (3% сезона —
    // около 3-4 игровых суток при 117-суточном сезоне), чтобы читаться как
    // "определённая ночь", не "всё лето".
    UPROPERTY(config, EditAnywhere, Category = "Time|Season", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float KupalaWindowStart = 0.15f;
    UPROPERTY(config, EditAnywhere, Category = "Time|Season", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float KupalaWindowEnd = 0.18f;

    // --- Погода (§15.7) — 2026-08-29, собственный C++-сигнал, по прямому
    // решению пользователя, пока Ultra Dynamic Weather не установлен в
    // проект. Значения — детерминированная value-noise (GridWorldManagerEntities.cpp,
    // SampleWeatherNoise), эти пороги переключают её в "гейт да/нет" для
    // §16.2 (Ветряные бесы/Метельники/Вихри). ---

    // Как часто меняется "погодный фронт" — единица интерполяции ветра/снега.
    // 480с = четверть игровых суток по умолчанию (GameDayMinutes×60/4).
    UPROPERTY(config, EditAnywhere, Category = "Weather", meta = (ClampMin = "10.0"))
    float WeatherFrontDurationSeconds = 480.0f;

    UPROPERTY(config, EditAnywhere, Category = "Weather", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float WindyThreshold = 0.6f;

    // Ниже обычного WindyThreshold — метель это ветер И снег ОДНОВРЕМЕННО,
    // так что для неё достаточно чуть более скромного ветра самого по себе.
    UPROPERTY(config, EditAnywhere, Category = "Weather", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float BlizzardWindThreshold = 0.55f;
    UPROPERTY(config, EditAnywhere, Category = "Weather", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float BlizzardSnowThreshold = 0.55f;

    // Третий канал того же шума (Channel=2, SampleWeatherNoise), 2026-08-29 —
    // добавлено для сбора трав (FIngredientTableRow::bRequiresDryWeather):
    // компендиум почти повсеместно требует "сухой день" и явно запрещает сбор
    // в грозу, а ни Ветер, ни Метель (которая вдобавок возможна только Зимой,
    // GetSnowIntensity) этого не покрывают — большинство трав собирают
    // Весной/Летом. Независимый канал, не производный от Wind/Snow — иначе
    // дождь был бы идеально скоррелирован с ветром, а не отдельным фронтом.
    UPROPERTY(config, EditAnywhere, Category = "Weather", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float RainyThreshold = 0.6f;

    // Скорость обновления Memory.HistoryPurity (медленная скользящая средняя от текущей Purity).
    UPROPERTY(config, EditAnywhere, Category = "Entities|Bereginya", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float HistoryPurityLerpRate = 0.02f;

    // Морочники (Опасная нечисть, повсеместно) — надбавка к воспринятому Distortion
    // ночью (фаза “Ночь” из 15_Cycles §15.2). Используется только в UI/тултипах
    // (PerceiveValue), не меняет S_real.
    UPROPERTY(config, EditAnywhere, Category = "Entities|Morochniki", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float NightPerceptionDistortionBonus = 0.25f;

    // Общий запас гистерезиса для порогов проявления сущностей (Corruption у
    // Гнильников, HistoryPurity у Берегини, Respect у Полевика). Порог входа —
    // Threshold + запас, порог выхода — Threshold − запас: значение, дрожащее у
    // самой границы (из-за DeltaTime-шума или встречных Nudge от других систем),
    // не заставляет хозяина мигать проявлением каждый тик (DESIGN_World_State.md §14).
    UPROPERTY(config, EditAnywhere, Category = "Entities", meta = (ClampMin = "0.0", ClampMax = "0.3"))
    float EntityManifestationHysteresis = 0.05f;

    // --- Заряна: фрагменты памяти и Буян (обсуждение в сессии 2026-08-24) ---
    // Числа — первая прикидка, "числа пока забей" (прямая цитата из сессии):
    // темп ещё не определён, менять свободно, не ломая структуру.
    UPROPERTY(config, EditAnywhere, Category = "Zaryana", meta = (ClampMin = "1.0"))
    float MemoryFragmentLifetimeSeconds = 120.0f;

    UPROPERTY(config, EditAnywhere, Category = "Zaryana", meta = (ClampMin = "0.0"))
    float MemoryFragmentSpawnCooldownSeconds = 300.0f;

    // Как часто проверяются State-триггеры (LowLocalDistortion/ShrineRestored) —
    // не каждый кадр, сканирование всей сетки того не стоит.
    UPROPERTY(config, EditAnywhere, Category = "Zaryana", meta = (ClampMin = "1.0"))
    float MemoryFragmentStateCheckInterval = 5.0f;

    UPROPERTY(config, EditAnywhere, Category = "Zaryana", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float MemoryFragmentLowDistortionThreshold = 0.15f;

    UPROPERTY(config, EditAnywhere, Category = "Zaryana", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float MemoryFragmentShrineRestorationThreshold = 0.7f;

    // Порог Coherence/Purity/Distortion для триггера CoherentBrew (варка).
    UPROPERTY(config, EditAnywhere, Category = "Zaryana", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float MemoryFragmentBrewCoherenceThreshold = 0.8f;

    UPROPERTY(config, EditAnywhere, Category = "Zaryana", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float MemoryFragmentBrewDistortionCeiling = 0.2f;

    // Выше этого глобального (усреднённого по клеткам) Distortion фрагмент,
    // родившийся из подлинного триггера, всё равно рискует стать ложным —
    // "при высоком глобальном Morok фрагмент может проявиться как искажённый".
    UPROPERTY(config, EditAnywhere, Category = "Zaryana", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float MemoryFragmentFalseRiskGlobalDistortion = 0.5f;

    // Буян (§15.5): средний Distance(State, S0) по клеткам ниже порога +
    // все капища мира выше порога Restoration.
    UPROPERTY(config, EditAnywhere, Category = "Zaryana|Buyan", meta = (ClampMin = "0.0"))
    float BuyanAverageDistanceThreshold = 0.5f;

    UPROPERTY(config, EditAnywhere, Category = "Zaryana|Buyan", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float BuyanShrineRestorationThreshold = 0.7f;

    // --- Передозировка зелий (обсуждение в сессии 2026-08-24) ---
    // Компендиум ("сок его — сильное сердечное зелье, но и яд лютый", Ландыш;
    // тот же паттерн у Полярного мака и Чистотела) — сила лекарства и его яд
    // одна ось. Выше порога Potency зелье при применении на клетку не лечит
    // сильнее, а начинает вредить — риск/выгода мощных зелий, без отдельной
    // "тёмной" механики: порчей по-прежнему ведает Морок, это про дозу, не про скверну.
    UPROPERTY(config, EditAnywhere, Category = "Alchemy|Overdose", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float PotionOverdoseThreshold = 0.75f;

    UPROPERTY(config, EditAnywhere, Category = "Alchemy|Overdose", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float PotionOverdosePenalty = 0.5f;

    // --- Бистабильная релаксация (обсуждение в сессии 2026-08-24, DESIGN_World_State.md) ---
    // Раньше только Гнильники сдвигали цель релаксации к порче, и только для
    // Болота. Общий случай: любая клетка, чей Corruption проходит порог входа,
    // сама начинает целиться в испорченный полюс вместо здорового умолчания
    // биома — естественное восстановление перестаёт работать, пока игрок не
    // продавит Corruption ниже порога выхода активным вмешательством (не
    // пассивным ожиданием — на то и гистерезис). Центр 0.75, запас 0.10 —
    // вход 0.85 / выход 0.65. У Болота собственный здоровый Corruption уже 0.70
    // (DT_BiomeDefaults.json) — специально внутри этого промежутка: самый
    // тёмный из "нормальных" биомов и должен быть ближе всех к точке невозврата.
    UPROPERTY(config, EditAnywhere, Category = "Biome|Bistability", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float BiomeDegradeCenterCorruption = 0.75f;

    UPROPERTY(config, EditAnywhere, Category = "Biome|Bistability", meta = (ClampMin = "0.0", ClampMax = "0.3"))
    float BiomeDegradeMargin = 0.10f;

    // --- Капища (02_GDD/15_Cycles_And_Shrines.md §15.5) ---
    // Вклад в Restoration от применения зелья прямо на клетку капища:
    // OfferingGain × (Purity − Corruption) результата. Комментарий поправлен
    // 2026-08-29 — был не обновлён при самой правке формулы (варка сама по
    // себе больше не подношение, только Apply-на-клетку, см.
    // RunSimulationStep/GridWorldManagerTick.cpp): раньше здесь стояла
    // формула через Coherence варки, отброшенная той же правкой.
    UPROPERTY(config, EditAnywhere, Category = "Shrines", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float ShrineOfferingGain = 0.05f;

    // --- "Хозяева" места, §16.3 (Полевик и далее) ---
    // Тот же принцип, что ShrineOfferingGain выше (Apply-на-клетку-
    // обиталище, вклад = Gain × (Purity − Corruption) результата), но БЕЗ
    // капищного спада при небрежении — отдельная настройка, не переиспользует
    // ShrineOfferingGain, т.к. это философски разные системы (см.
    // GridWorldManagerEntities.cpp/GridWorldManagerTick.cpp, 2026-08-29).
    UPROPERTY(config, EditAnywhere, Category = "Entities|Landmarks", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float LandmarkOfferingGain = 0.05f;

    // Горизонт полного угасания заброшенного капища — то же число, что уже
    // задаёт лунный цикл (§15.3), не новая величина.
    UPROPERTY(config, EditAnywhere, Category = "Shrines", meta = (ClampMin = "1.0"))
    float ShrineNeglectDecayDays = 28.0f;

    // Радиус влияния (эффекты 1/2/4) в клетках — шире радиуса подношения
    // (только собственная клетка капища). Тот же порядок величины, что у
    // PropagationDepth биомного графа.
    UPROPERTY(config, EditAnywhere, Category = "Shrines", meta = (ClampMin = "1"))
    int32 ShrineInfluenceRadius = 3;

    // Множитель надбавки к Coherence варки в радиусе влияния (эффект 2,
    // §11.7): Coherence_итог = Coherence + Restoration × ShrineCoherenceBonus.
    UPROPERTY(config, EditAnywhere, Category = "Shrines", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float ShrineCoherenceBonus = 0.15f;

    // --- Типоспецифичные бонусы капищ (эффект 3, §15.5 "Типы капищ",
    // реализовано 2026-08-29 вместе с лорной привязкой к богам, см.
    // 15_Cycles_And_Shrines.md) — числа взяты дословно из таблицы шести
    // типов, RegenerateCellParameters/ApplyBiomeInfluences/PropagateWaves. ---

    // Родовое (Дажьбог): множитель к шагу релаксации ТОЛЬКО Stability-оси
    // (не всех осей разом, см. §15.5 "усиливает пуллинг Stability").
    UPROPERTY(config, EditAnywhere, Category = "Shrines|Types", meta = (ClampMin = "1.0"))
    float ShrineAncestralStabilityMultiplier = 1.5f;

    // Лесное (Велес): множитель к скорости спада HarvestStress в радиусе —
    // "StressRecoveryMultiplier биома делится на (1+0.5×Restoration)"
    // эквивалентно "decay-в-секунду умножается на (1+0.5×Restoration)"
    // (decay обратно пропорционален Multiplier), поэтому здесь хранится
    // сразу коэффициент 0.5, не сам делитель.
    UPROPERTY(config, EditAnywhere, Category = "Shrines|Types", meta = (ClampMin = "0.0"))
    float ShrineForestHealBonus = 0.5f;

    // Водное (Мокошь): "подтягивает Purity воды к 1.0 пропорционально
    // Restoration" — реализовано как локальный (в радиусе капища, не
    // глобальный DefaultWaterState биома целиком — эффект капища всегда
    // локален, тот же принцип, что у остальных четырёх типов) непрерывный
    // нудж TargetState.Meta.Purity воды, "в секунду" при полном Restoration.
    UPROPERTY(config, EditAnywhere, Category = "Shrines|Types", meta = (ClampMin = "0.0"))
    float ShrineWaterPurityPullRate = 0.02f;

    // Каменное (Стрибог): "глушит вклад MorokField в локальный Distortion на
    // (1 − 0.4×Restoration)".
    UPROPERTY(config, EditAnywhere, Category = "Shrines|Types", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float ShrineStoneMorokDampening = 0.4f;

    // Пограничное (Перун): "MorokLeak через рёбра графа умножается на
    // (1 − 0.5×Restoration)" — только для рёбер между биомами, физически
    // граничащими с клеткой самого капища в сетке (не любые два узла графа).
    UPROPERTY(config, EditAnywhere, Category = "Shrines|Types", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float ShrineBorderLeakDampening = 0.5f;

    // ShrineInventoryProtection удалена 2026-08-29 вместе с "эффектом 4"
    // капищ (§15.5) целиком — прямая правка пользователя: порча трав не
    // зависит от места в мире вообще, только от контейнера хранения (см.
    // EStorageContainerType, HerbalistInventoryComponent.h).

    // --- Spawning (DESIGN_World_State.md §15, звено 3) ---
    // Резкость спада пригодности ингредиента в GetRandomResourceForBiome:
    // Suitability = exp(-Falloff * Distance(CellState, BaseState)^2). 2.0 подобрано
    // так, чтобы типичный разброс Purity/Corruption внутри одного биома в компендиуме
    // (~0.45, см. таблицу Болота в DESIGN_World_State.md §15) давал заметный, но не
    // нулевой перекос: трава-полюс биома в клетке, близкой к его эталону, ещё
    // всходит, просто редко.
    UPROPERTY(config, EditAnywhere, Category = "Spawning", meta = (ClampMin = "0.0"))
    float IngredientSuitabilityFalloff = 2.0f;

    // Множитель пригодности вне окна сезона/времени суток/фазы луны/погоды
    // (DESIGN_World_State.md §15/§16: "СезонноеОкно × ПогодноеОкно"), 2026-08-29.
    // Не 0 — тот же принцип, что уже применён к дистанции по State выше
    // (комментарий над IngredientSuitabilityFalloff): окно резко гасит шанс,
    // но не запирает его наглухо (редкая трава вне срока — не невозможная
    // трава). Один общий множитель на все четыре гейта, не четыре отдельных
    // настройки — они однородны по роли (мягкий отсекающий гейт), лишняя
    // раздельная настройка не отражала бы никакой реальной разницы между ними.
    UPROPERTY(config, EditAnywhere, Category = "Spawning", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float IngredientWindowMismatchMultiplier = 0.15f;
};

UHerbalistSettings* GetHerbalistSettings();