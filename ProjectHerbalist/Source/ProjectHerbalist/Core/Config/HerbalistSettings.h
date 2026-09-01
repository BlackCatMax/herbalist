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

    // --- Согласие/конфликт трав (2026-08-30, "докручиваем варку") ---
    // Раньше не-водные ингредиенты сворачивались одним симметричным
    // взвешенным средним (Fold) -- любая сильная ось одной травы гасилась
    // слабой той же оси у другой, независимо от порядка добавления. Теперь
    // ингредиенты обрабатываются ПОСЛЕДОВАТЕЛЬНО (первый -- затравка, каждый
    // следующий РЕАГИРУЕТ на уже накопленный результат, не усредняется с ним
    // симметрично): согласные оси (обе выше или обе ниже 0.5) усиливают друг
    // друга, конфликтующие -- гасят к нейтральной середине. См.
    // ComputeApplyResult (PipelineV2.cpp).
    UPROPERTY(config, EditAnywhere, Category = "Pipeline|Alchemy Harmony", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float AlchemyAgreementRate = 0.6f;

    UPROPERTY(config, EditAnywhere, Category = "Pipeline|Alchemy Harmony", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float AlchemyConflictRate = 0.5f;

    // Мощь (Magnitude) зелья -- прямое следствие того, насколько ингредиенты
    // "спелись" (см. ComputeHarmony), не просто их количество: слаженное
    // сочетание реально сильнее, чем взятое отдельно любое из двух, а
    // разнородное -- слабее, даже если добавить больше веществ.
    UPROPERTY(config, EditAnywhere, Category = "Pipeline|Alchemy Harmony", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float AlchemyPowerGrowthRate = 0.5f;

    UPROPERTY(config, EditAnywhere, Category = "Pipeline|Alchemy Harmony", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float AlchemyPowerDecayRate = 0.6f;

    // --- Градации сложности/опасности варки (2026-08-30, "у зелий должны
    // появляться градации сложности варки и опасности" -- 2 просто, 3 риск,
    // 4 опасно, 5 смертельно, прямой запрос). Опасность = исход варки
    // (Bifurcation, не новая система вреда игроку -- в проекте нет системы
    // здоровья вовсе). Считается от числа РАЗНЫХ не-водных ингредиентов
    // (NonWaterItems.Num(), не суммарного Count) -- см. ComputeApplyResult. ---

    // На каждый ингредиент сверх двух порог Bifurcation (CollapseThreshold)
    // снижается на эту величину -- зелье срывается уже при более скромном
    // Distortion, чем при бережливой варке из двух вещей.
    UPROPERTY(config, EditAnywhere, Category = "Pipeline|Alchemy Risk", meta = (ClampMin = "0.0", ClampMax = "0.5"))
    float AlchemyRiskThresholdStep = 0.15f;

    // На каждый ингредиент сверх двух шанс "повезло" (Purified вместо
    // Catastrophe при сорвавшемся Bifurcation) домножается на (1 - это
    // значение) -- крепкая Stability всё ещё может спасти при 3-4
    // ингредиентах, но с каждым разом всё менее надёжно.
    UPROPERTY(config, EditAnywhere, Category = "Pipeline|Alchemy Risk", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float AlchemyRiskPurifyOddsStep = 0.3f;

    // От этого числа РАЗНЫХ не-водных ингредиентов и выше -- Catastrophe
    // гарантирована безусловно (не зависит от Distortion/Stability вовсе):
    // "5 смертельно" буквально, без исключений для удачно подобранных
    // ингредиентов -- котёл не прощает такую сложность, что бы в него ни
    // положили.
    UPROPERTY(config, EditAnywhere, Category = "Pipeline|Alchemy Risk", meta = (ClampMin = "3", ClampMax = "10"))
    int32 AlchemyGuaranteedCatastropheCount = 5;

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

    // Четвёртый триггер, HighCommunityTrust (17_Hero_And_Community.md §17.6,
    // "устойчиво высокая Молва") — мгновенный порог, БЕЗ гистерезиса, та
    // же простота, что у двух триггеров выше (не как у проявления
    // сущностей — там разведка этой сессии нашла, что в проекте вообще
    // нет механизма длительности "устойчиво N секунд").
    UPROPERTY(config, EditAnywhere, Category = "Zaryana", meta = (ClampMin = "-1.0", ClampMax = "1.0"))
    float MemoryFragmentHighMolvaThreshold = 0.5f;

    // OJIDANIE_BURI (17_Hero_And_Community.md §17.7, Тундра) — "клетка с
    // высокой Stability, удержанной долго". Мгновенный порог, тот же
    // принцип, что MemoryFragmentHighMolvaThreshold выше — в проекте нет
    // механизма длительности.
    UPROPERTY(config, EditAnywhere, Category = "Zaryana", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float MemoryFragmentHighStabilityThreshold = 0.7f;

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

    // Clarity = якорь + отклик (20_Investment_And_Progression.md §20.3,
    // 2026-09-01). Якорь (ClarityAnchor) растёт только от подлинных
    // фрагментов и никогда не уменьшается; отклик — волатильный слой поверх
    // него, ±ClarityResponseRange от среднего Restoration/Respect по
    // капищам/хозяевам минус средний MorokHistory по узлам биом-графа.
    // Число из главы (±0.2) черновое, как и остальные Zaryana-числа выше.
    UPROPERTY(config, EditAnywhere, Category = "Zaryana", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float ClarityResponseRange = 0.2f;

    // Роса Заряны (19_Rosa_Signal.md §19.2, Слой 3): радиус в клетках, с
    // которого влияние капищ/хозяев места подмешивается в её State —
    // растёт с Clarity ("восстановленное капище на другом краю карты чуть
    // светлит её кожу"). Числа черновые — 3 клетки у Clarity=0 (её
    // непосредственный двор), до ~18 у Clarity=1 (большая часть сетки
    // 20x20 по умолчанию).
    UPROPERTY(config, EditAnywhere, Category = "Zaryana", meta = (ClampMin = "0.0"))
    float RosaBaseRadius = 3.0f;

    UPROPERTY(config, EditAnywhere, Category = "Zaryana", meta = (ClampMin = "0.0"))
    float RosaRadiusPerClarity = 15.0f;

    // Клубочек (21_Journey_And_Artifacts.md §21.2) — игровых секунд на юнит
    // дистанции при перемещении между базами. Черновое число: полный
    // пересечение сетки 20x20 по умолчанию (~2500 юнитов) обходится примерно
    // в 375 игровых секунд, ~20% игровых суток при дефолтном GameDayMinutes
    // (32 мин) — заметная, но не разорительная трата, "стоит ли сейчас идти
    // проверять грядку" (§20.2), не бесплатная телепортация.
    UPROPERTY(config, EditAnywhere, Category = "Zaryana", meta = (ClampMin = "0.0"))
    float YarnBallSecondsPerUnit = 0.15f;

    // Буян (§15.5): средний Distance(State, S0) по клеткам ниже порога +
    // все капища мира выше порога Restoration.
    UPROPERTY(config, EditAnywhere, Category = "Zaryana|Buyan", meta = (ClampMin = "0.0"))
    float BuyanAverageDistanceThreshold = 0.5f;

    UPROPERTY(config, EditAnywhere, Category = "Zaryana|Buyan", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float BuyanShrineRestorationThreshold = 0.7f;

    // Путь 1 у Буяна — страж (18_Ending.md §18.2, 2026-09-01): "доступен
    // при высокой GlobalPerceptionClarity и высокой Молве — герою, которому
    // мир и люди уже доверяют". Мгновенный порог, без гистерезиса — тот же
    // принцип, что уже держит CheckBuyanCondition (в проекте нет механизма
    // длительности "устойчиво N секунд", находка разведки шага 1). Пути 2/3
    // — без порога, §18.2 явно: "искушение не должно быть наградой за
    // прогресс".
    UPROPERTY(config, EditAnywhere, Category = "Zaryana|Buyan", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float BuyanGuardianClarityThreshold = 0.7f;

    UPROPERTY(config, EditAnywhere, Category = "Zaryana|Buyan", meta = (ClampMin = "-1.0", ClampMax = "1.0"))
    float BuyanGuardianMolvaThreshold = 0.5f;

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

    // Заражение соседей (2026-08-30, "разрастание поганых мест") — пока клетка
    // в испорченном полюсе (Cell.Memory.bDegrading), она непрерывно толкает
    // TargetState четырёх ПРЯМЫХ соседей по сетке (не диагональных, не по
    // биомному графу — тот же принцип соседства, что уже у bRequiresBiomeBorder
    // в AmbientEntityTypes.h) в ту же сторону, что и её собственный полюс
    // (Corruption/Distortion вверх, Purity/Stability вниз), тем же "в секунду"
    // темпом на все четыре оси. Пересекает границу биома по решению
    // пользователя — заражение не спрашивает, какой биом у соседа, тот же
    // принцип, что уже применяет диффузия Морока по биомному графу, просто
    // на уровне отдельных клеток сетки, не восьми узлов графа. Само по себе
    // НЕ мгновенно перекидывает соседа через его собственный порог
    // гистерезиса — только медленно толкает TargetState, оставляя игроку
    // время заметить и вмешаться (собрать, полить зельем), прежде чем сосед
    // однажды перейдёт порог уже сам.
    UPROPERTY(config, EditAnywhere, Category = "Biome|Bistability", meta = (ClampMin = "0.0"))
    float ContagionSpreadRate = 0.01f;

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

    // --- Молва общины (17_Hero_And_Community.md §17.3, DESIGN_Community_
    // And_Homestead.md §1, реализация 2026-08-31) ---
    // Тот же принцип роста, что Shrine/LandmarkOfferingGain выше (Gain ×
    // (Purity − Corruption) поднесённого), но медленнее — "община доверяет
    // дольше, чем одно место" (§17.3, прикидка документа — 0.03 против 0.05
    // у капищ). Не тот же самый параметр, что выше: три философски разных
    // адресата (место / хозяин места / люди), три отдельные настройки —
    // тот же довод, что уже развёл ShrineOfferingGain и LandmarkOfferingGain.
    UPROPERTY(config, EditAnywhere, Category = "Community", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float MolvaOfferingGain = 0.03f;

    // Пороговая (не непрерывная) привязка веток диалога/торговли к Molva —
    // "ниже порога только анонимные записки/худший курс, выше — именные
    // ветки/лучший курс" (§17.3, таблица) -- НЕ реализована: единственный
    // существующий пример дерева (Домовой) гейтится Landmark.Respect, не
    // Molva вовсе (у него нет "имени" community-собеседника), а Торговля
    // уже использует Molva непрерывно (TradeMolvaRateBonus ниже), не через
    // пороги. Аудит "на аудит" (2026-08-31) снял отсюда MolvaLowThreshold/
    // MolvaHighThreshold -- два поля стояли здесь с комментарием,
    // утверждавшим поведение, которого нет ни в одной строчке кода (ни разу
    // не прочитаны нигде в проекте). Остаётся открытым пунктом §17.3, не
    // заглушкой в настройках.

    // --- Торговля с общиной (DESIGN_Community_And_Homestead.md §1.2) ---
    // Ценность(предмет) = Magnitude × (1 + PurityWeight×Purity) × (1 +
    // RarityWeight×(1/RarityWeight_ингредиента)) — редкость учитывается как
    // обратная величина уже существующего IngredientTableRow::RarityWeight
    // (меньше вес спавна = реже = ценнее), не новое понятие "редкости".
    UPROPERTY(config, EditAnywhere, Category = "Community", meta = (ClampMin = "0.0"))
    float TradeValuePurityWeight = 0.5f;
    UPROPERTY(config, EditAnywhere, Category = "Community", meta = (ClampMin = "0.0"))
    float TradeValueRarityWeight = 0.5f;

    // Курс = ЦенностьA/ЦенностьB, домножается на (1 + MolvaRateBonus×Molva) —
    // выше Molva, выгоднее курс (община доверяет качеству того, что даёт
    // герой), тот же язык, что уже в §1.2 документа.
    UPROPERTY(config, EditAnywhere, Category = "Community", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float TradeMolvaRateBonus = 0.3f;

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

    // ---- Инструмент сбора (DESIGN_Community_And_Homestead.md §2.3), 2026-08-31.
    // Применяются к Magnitude/Potency/Resonance собранного предмета в
    // GenerateHarvestResult (PipelineV2.cpp) — ГДЕ множитель <1 гасит их,
    // а не к шансу спавна ресурса в мире (тот уже решён IngredientSuitability*
    // выше): неверный инструмент портит УЖЕ найденную траву при сборе, не
    // мешает ей существовать в мире.

    // Базовый множитель качества голыми руками — безопасно везде, но
    // медленнее любого инструмента.
    UPROPERTY(config, EditAnywhere, Category = "Gathering Tools", meta = (ClampMin = "0.0", ClampMax = "2.0"))
    float GatheringToolBareHandsMultiplier = 0.7f;

    // Медь/кость на обычной (не помеченной) траве — чуть хуже железа,
    // цена универсальности неметаллического инструмента.
    UPROPERTY(config, EditAnywhere, Category = "Gathering Tools", meta = (ClampMin = "0.0", ClampMax = "2.0"))
    float GatheringToolNonIronMultiplier = 0.9f;

    // Железо на траве с bIronAverse — трава «чует железо» и прячет силу
    // (карточки Плакун-травы/Чистотела). Голые руки/медь/кость на такой
    // траве вместо этого получают полный множитель 1.0 (не их обычный
    // базовый) — уважительный сбор не наказывается вовсе.
    UPROPERTY(config, EditAnywhere, Category = "Gathering Tools", meta = (ClampMin = "0.0", ClampMax = "2.0"))
    float GatheringToolIronAverseMultiplier = 0.3f;

    // Костяной нож на траве с bDelicate — сохраняет мощь при срезе лучше
    // любого другого инструмента (карточка Медуницы). Перекрывает
    // GatheringToolIronAverseMultiplier, если трава несёт оба флага сразу
    // (кость и так не железо — снимает табу и даёт бонус одновременно).
    UPROPERTY(config, EditAnywhere, Category = "Gathering Tools", meta = (ClampMin = "0.0", ClampMax = "2.0"))
    float GatheringToolDelicateBoneBonus = 1.15f;

    // Сколько секунд держится на экране попап с текстом воспоминания Заряны/
    // объявлением Буяна (UI/MemoryRevealWidget.h), 2026-08-29. Достаточно на
    // 1-2 предложения текущей длины (см. MemoryFragmentDefinitions.h) при
    // спокойном темпе чтения, не хронометрировано точно под каждый текст.
    UPROPERTY(config, EditAnywhere, Category = "Zaryana", meta = (ClampMin = "1.0"))
    float MemoryRevealDisplaySeconds = 7.0f;

    // Артефакты Легендарных (21_Journey_And_Artifacts.md §21.3-21.4,
    // 2026-09-01) — единственная новая категория этого прохода без
    // существующего аналога (найдено разведкой к шагу 6: ни "Bases", ни
    // "Artifacts" не подходят ни под одну уже заведённую категорию).
    // Порог среднего Purity подношения, выше которого путь считается
    // честным (по РЕАЛЬНОМУ Purity) или обманным (по ВОСПРИНЯТОМУ, если
    // реальный ниже) — та же логика, что уже отличает S_real/S_Perceived
    // в тултипе (AlchemySlotWidget.cpp). Черновое число.
    UPROPERTY(config, EditAnywhere, Category = "Artifacts", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float ArtifactHonestPurityThreshold = 0.6f;

    // Семь эффектов артефактов (§21.3, 2026-09-01, ревизия "Ending and
    // artifacts"). Черновые числа, как и остальные новые Zaryana/Artifacts
    // числа этой сессии.

    // Молодильное яблоко — секунд игрового времени, на которое эффективная
    // Clarity росы поднимается до 1.0 (полное гашение шума), пока окно не истекло.
    UPROPERTY(config, EditAnywhere, Category = "Artifacts", meta = (ClampMin = "0.0"))
    float YouthAppleWindowSeconds = 180.0f;

    // Шапка-невидимка — секунд игрового времени подавления новых проявлений
    // за одно применение (само применение — Exec-команда, не расходуется).
    UPROPERTY(config, EditAnywhere, Category = "Artifacts", meta = (ClampMin = "0.0"))
    float InvisibilityCapDurationSeconds = 300.0f;
};

UHerbalistSettings* GetHerbalistSettings();