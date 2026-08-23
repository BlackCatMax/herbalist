// MemoryFragmentTypes.h
//
// Фрагменты памяти Заряны (обсуждение в сессии 2026-08-24, 06_Progression.md
// "Прогрессия через Заряну", 15_Cycles_And_Shrines.md §15.5 "Буян"). Прототип
// от пользователя описывал EventOutbox/CommandBus/AssetCatalog/RuleSet —
// инфраструктуры этого класса в проекте нет, адаптировано на уже существующие
// каналы: внепайплайновый тик (как UpdateEntityManifestations/UpdateShrines),
// мировые акторы (как AHerbalistResourceActor), Interact() (как у
// AAlchemyTableActor/AStorageContainer).
//
// v1 — вертикальный срез: 3 определения (по одному на триггер), не полный
// авторский конвейер. Тот же принцип, что у Проявления сущностей (4 из 62
// карточек бестиария) и Капищ (3 эффекта из 4).
#pragma once

#include "CoreMinimal.h"
#include "MemoryFragmentTypes.generated.h"

UENUM(BlueprintType)
enum class EMemoryFragmentTrigger : uint8
{
    // Клетка/биом надолго осталась почти без искажения — мир "затих".
    LowLocalDistortion,
    // Только что сварено осознанно и чисто (высокий Coherence, низкий Distortion).
    CoherentBrew,
    // Капище пересекло высокий порог Restoration.
    ShrineRestored
};

USTRUCT()
struct FMemoryFragmentDefinition
{
    GENERATED_BODY()

    UPROPERTY() FName ID;
    UPROPERTY() EMemoryFragmentTrigger Trigger = EMemoryFragmentTrigger::LowLocalDistortion;

    // Подлинное воспоминание — то, что Заряна на самом деле помнит.
    UPROPERTY() FText TrueText;

    // Ложное (искажённое Мороком) воспоминание — похоже на подлинное, но
    // с нарочитым сбоем (см. FindFalseTellSign в MemoryFragmentActor.cpp);
    // игрок должен научиться отличать по контексту, не по тексту напрямую.
    UPROPERTY() FText FalseText;

    // Насколько подлинный сбор поднимает GlobalPerceptionClarity.
    UPROPERTY() float ClarityGain = 0.05f;
};

// Заспавненный в мире фрагмент — не предмет инвентаря, временное проявление
// (AMemoryFragmentActor), исчезающее, если не собрать вовремя.
USTRUCT()
struct FActiveMemoryFragment
{
    GENERATED_BODY()

    UPROPERTY() FName DefinitionID;
    UPROPERTY() FIntPoint Cell = FIntPoint(-1, -1);
    UPROPERTY() bool bIsFalse = false;
};
