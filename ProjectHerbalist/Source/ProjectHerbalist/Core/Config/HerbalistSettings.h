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
    // клетки для проявления. Упрощённая версия без полноценной системы капищ
    // (15_Cycles_And_Shrines §15.5) — тот же принцип, одна клетка вместо структуры.
    UPROPERTY(config, EditAnywhere, Category = "Entities|Bereginya", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float BereginyaHistoryPurityThreshold = 0.75f;

    // Скорость обновления Memory.HistoryPurity (медленная скользящая средняя от текущей Purity).
    UPROPERTY(config, EditAnywhere, Category = "Entities|Bereginya", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float HistoryPurityLerpRate = 0.02f;

    // Морочники (Опасная нечисть, повсеместно) — надбавка к воспринятому Distortion
    // ночью (фаза “Ночь” из 15_Cycles §15.2). Используется только в UI/тултипах
    // (PerceiveValue), не меняет S_real.
    UPROPERTY(config, EditAnywhere, Category = "Entities|Morochniki", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float NightPerceptionDistortionBonus = 0.25f;
};

UHerbalistSettings* GetHerbalistSettings();