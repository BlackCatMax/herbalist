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

    // --- Entity Manifestation (02_GDD/16_Entity_Manifestation.md) — вертикальный срез ---
    // Гнильники (Низший, Болото): порог Corruption клетки для проявления зоны.
    UPROPERTY(config, EditAnywhere, Category = "Entities|Gnilniki", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float GnilnikiCorruptionThreshold = 0.6f;

    // На сколько зона Гнильников дополнительно тянет клетку к Corruption/Purity в секунду
    // (самоусиливающаяся порча места, ограничена клиппом в ApplyStateDelta).
    UPROPERTY(config, EditAnywhere, Category = "Entities|Gnilniki", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float GnilnikiNudgeRate = 0.01f;

    // Полевик (Основной, Лесостепь) — скорость роста/падения Respect в секунду.
    UPROPERTY(config, EditAnywhere, Category = "Entities|Landmarks", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float LandmarkRespectGainRate = 0.01f;

    UPROPERTY(config, EditAnywhere, Category = "Entities|Landmarks", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float LandmarkRespectDecayRate = 0.02f;

    // Граница HarvestStress клетки-обиталища, выше которой Respect только падает.
    UPROPERTY(config, EditAnywhere, Category = "Entities|Landmarks", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float LandmarkStressAngerThreshold = 0.6f;

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
    // Вклад в Restoration от одной варки прямо на клетке капища:
    // OfferingGain × (Coherence−0.5) × 2 × (1−Distortion). При Coherence=1 и
    // чистом результате — максимум; ниже 0.5 — вклад отрицательный.
    UPROPERTY(config, EditAnywhere, Category = "Shrines", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float ShrineOfferingGain = 0.05f;

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

    // Насколько сильно полностью восстановленное капище снижает эффективный
    // InventoryDecayRate в своём радиусе (эффект 4): до вдвое при Restoration=1.
    UPROPERTY(config, EditAnywhere, Category = "Shrines", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float ShrineInventoryProtection = 0.5f;

    // --- Spawning (DESIGN_World_State.md §15, звено 3) ---
    // Резкость спада пригодности ингредиента в GetRandomResourceForBiome:
    // Suitability = exp(-Falloff * Distance(CellState, BaseState)^2). 2.0 подобрано
    // так, чтобы типичный разброс Purity/Corruption внутри одного биома в компендиуме
    // (~0.45, см. таблицу Болота в DESIGN_World_State.md §15) давал заметный, но не
    // нулевой перекос: трава-полюс биома в клетке, близкой к его эталону, ещё
    // всходит, просто редко.
    UPROPERTY(config, EditAnywhere, Category = "Spawning", meta = (ClampMin = "0.0"))
    float IngredientSuitabilityFalloff = 2.0f;
};

UHerbalistSettings* GetHerbalistSettings();