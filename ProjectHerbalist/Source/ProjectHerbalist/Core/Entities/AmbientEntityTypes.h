// AmbientEntityTypes.h
//
// Низший ранг бестиария (16_Entity_Manifestation.md §16.2): "амбиентная
// зона, не объект" — условие есть переход Meta-оси клетки через порог
// (плюс, опционально, ночная фаза), эффект есть нудж TargetState теми же
// осями. §16.2 прямо говорит: "новый код не нужен — только данные (порог +
// d_manifest)". До аудита 2026-08-24 это было не так: единственный
// реализованный низший (Гнильники) жил как один бесповоротно захардкоженный
// if-блок в GridWorldManagerEntities.cpp, так что "просто данные" на деле
// означало "скопировать ещё один if-блок на каждое новое существо". Этот
// файл превращает раздел в то, чем он и задуман — таблицу определений,
// которую обходит один универсальный цикл (UpdateEntityManifestations).
//
// Сознательно НЕ вынесено в UDataTable/DA_*.uasset (в отличие от биомов и
// ингредиентов): определений пока пять, они меняются вместе с кодом
// (новый EAmbientTriggerAxis требует правки C++ всё равно), и это тот же
// принцип, что уже применён к MemoryFragmentDefinitions.h — простой
// статический реестр, а не полноценный ассет-пайплайн, пока карточек мало.
#pragma once

#include "CoreMinimal.h"
#include "Core/Types/HerbalistCoreTypes.h"
#include "AmbientEntityTypes.generated.h"

UENUM()
enum class EAmbientTriggerAxis : uint8
{
    None,          // условие только по ночи (bRequiresNight), без оси
    Corruption,
    Purity,
    Distortion,
    Stability
};

USTRUCT()
struct FAmbientEntityDefinition
{
    GENERATED_BODY()

    UPROPERTY() FName EntityID;
    UPROPERTY() EBiomeType Biome = EBiomeType::Bog;

    // Оба false = клетка любая (земля или вода). Ставить оба true бессмысленно.
    UPROPERTY() bool bLandOnly = false;
    UPROPERTY() bool bWaterOnly = false;

    // Условие проявления. TriggerAxis == None означает "только по ночи" —
    // тогда bRequiresNight обязан быть true, иначе определение никогда не
    // сработает (проверяется в GetAmbientEntityDefinitions() через check).
    UPROPERTY() EAmbientTriggerAxis TriggerAxis = EAmbientTriggerAxis::None;
    UPROPERTY() float TriggerThreshold = 0.0f;
    UPROPERTY() bool bTriggerAbove = true;   // true: ось > порога; false: ось < порога
    UPROPERTY() bool bRequiresNight = false;
    UPROPERTY() float HysteresisMargin = 0.05f;

    // Нудж TargetState, "в секунду" (как GnilnikiNudgeRate раньше) —
    // умножается на DeltaTime в UpdateEntityManifestations. Ноль = не трогать ось.
    UPROPERTY() float CorruptionRate = 0.0f;
    UPROPERTY() float PurityRate = 0.0f;
    UPROPERTY() float DistortionRate = 0.0f;
    UPROPERTY() float StabilityRate = 0.0f;
};

inline float GetAmbientTriggerAxisValue(const FMeta& Meta, EAmbientTriggerAxis Axis)
{
    switch (Axis)
    {
    case EAmbientTriggerAxis::Corruption: return Meta.Corruption;
    case EAmbientTriggerAxis::Purity:     return Meta.Purity;
    case EAmbientTriggerAxis::Distortion: return Meta.Distortion;
    case EAmbientTriggerAxis::Stability:  return Meta.Stability;
    default:                              return 0.0f;
    }
}

// Статический реестр — тот же паттерн, что HerbalistCore::Zaryana::
// GetMemoryFragmentDefinitions() (MemoryFragmentDefinitions.h).
inline const TArray<FAmbientEntityDefinition>& GetAmbientEntityDefinitions()
{
    static const TArray<FAmbientEntityDefinition> Definitions = []()
    {
        TArray<FAmbientEntityDefinition> Defs;

        // Гнильники (Болото, земля): Corruption > 0.6 -> Corruption++, Purity--.
        // Перенесено из прежнего захардкоженного блока без изменения чисел —
        // регрессия Herbalist.Bistability.* проверяет именно эти пороги.
        {
            FAmbientEntityDefinition D;
            D.EntityID = FName(TEXT("Гнильники"));
            D.Biome = EBiomeType::Bog;
            D.bLandOnly = true;
            D.TriggerAxis = EAmbientTriggerAxis::Corruption;
            D.TriggerThreshold = 0.6f;
            D.bTriggerAbove = true;
            D.CorruptionRate = 0.01f;
            D.PurityRate = -0.005f;
            Defs.Add(D);
        }

        // Моховые духи (Тайга, земля, 16_Entity_Manifestation §16.2): "Purity
        // клетки высокая -> Stability++, Purity++" — единственный "улучшающий"
        // низший по спецификации; зеркало Гнильников (самоусиливающееся
        // оздоровление вместо самоусиливающейся порчи).
        {
            FAmbientEntityDefinition D;
            D.EntityID = FName(TEXT("Моховые духи"));
            D.Biome = EBiomeType::Taiga;
            D.bLandOnly = true;
            D.TriggerAxis = EAmbientTriggerAxis::Purity;
            D.TriggerThreshold = 0.75f;
            D.bTriggerAbove = true;
            D.PurityRate = 0.01f;
            D.StabilityRate = 0.01f;
            Defs.Add(D);
        }

        // Степные огни (Степь, земля, §16.2): "сумерки/ночь, открытый биом ->
        // Distortion++, дезориентация восприятия" — блуждающие огни степного
        // фольклора, заводящие путника в темноте. Чисто ночной триггер, без
        // Meta-оси (TriggerAxis::None).
        {
            FAmbientEntityDefinition D;
            D.EntityID = FName(TEXT("Степные огни"));
            D.Biome = EBiomeType::Steppe;
            D.bLandOnly = true;
            D.TriggerAxis = EAmbientTriggerAxis::None;
            D.bRequiresNight = true;
            D.DistortionRate = 0.008f;
            Defs.Add(D);
        }

        for (const FAmbientEntityDefinition& D : Defs)
        {
            check(D.TriggerAxis != EAmbientTriggerAxis::None || D.bRequiresNight);
        }
        return Defs;
    }();
    return Definitions;
}
