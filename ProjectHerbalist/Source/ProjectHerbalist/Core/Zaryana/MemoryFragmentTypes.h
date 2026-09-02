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
// Состояние на 2026-09-02: 12 фрагментов в DT_MemoryFragments (4 базовых +
// 3 исхода Буяна §18.1 + 5 биомных §17.7) на 8 типов триггеров. Прежняя
// пометка "v1, вертикальный срез: 3 определения... 4 из 62 карточек
// бестиария" снята как устаревшая — бестиарий с тех пор реализован целиком
// (60 карточек в трёх DataTable), а сами фрагменты выросли с 3 до 12.
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "MemoryFragmentTypes.generated.h"

UENUM(BlueprintType)
enum class EMemoryFragmentTrigger : uint8
{
    // Клетка/биом надолго осталась почти без искажения — мир "затих".
    LowLocalDistortion,
    // Только что сварено осознанно и чисто (высокий Coherence, низкий Distortion).
    CoherentBrew,
    // Капище пересекло высокий порог Restoration.
    ShrineRestored,
    // Молва общины устойчиво высока (17_Hero_And_Community.md §17.6).
    HighCommunityTrust,
    // Один из трёх исходов у Буяна выбран (18_Ending.md §18.1,
    // 21_Journey_And_Artifacts.md §21.1: "гарантированный фрагмент на
    // Буяне, три вариации"). Не спавнится через TrySpawnStateBasedFragment
    // — доставляется напрямую из AGridWorldManager::TryChooseBuyanPath,
    // тем же внепайплайновым, событийным приёмом, что уже CoherentBrew.
    BuyanPathChosen,

    // Пять ключевых биомных фрагментов (17_Hero_And_Community.md §17.7,
    // 2026-09-01) — три новых семейства триггеров, каждое биом-скопировано
    // (в отличие от LowLocalDistortion/ShrineRestored выше, которые не
    // смотрят на биом клетки вовсе).

    // Устойчиво высокая Stability конкретной клетки, скопировано на биом
    // (OJIDANIE_BURI, Тундра). **Реализовано как настоящее "выдержано N
    // секунд" 2026-09-02** — HerbalistCore::Math::TickSustainedCondition
    // (HerbalistCoreMath.h), per-клеточный аккумулятор
    // (AGridWorldManager::OjidanieBuriHoldSeconds), не мгновенный порог, как
    // раньше (комментарий здесь был устаревшим — механизм длительности
    // теперь есть в проекте, тот же аппарат покрывает и TISHINA_LESA/
    // KHLEB_SOL, см. TrySpawnStateBasedFragment, GridWorldManagerZaryana.cpp).
    HighStability,

    // Клетка-якорь Легендарного сейчас в благом проявленном состоянии
    // (NE_POKHVALILA, Смешанный лес/Баба-Яга). §17.7 описывает триггер как
    // "успешно пройденная зона, ранее помеченная Легендарной угрозой" —
    // Смешанный лес не держит Malign-полюсного Легендарного в реестре
    // (см. LegendaryEntityTypes.h), точный смысл фразы для этого конкретно
    // биома неочевиден; приближено через IsLegendaryManifested("Баба-Яга")
    // как ближайшее решение без придумывания новой сущности — открытый
    // вопрос, зафиксирован в CHANGELOG.md, не решён окончательно.
    LegendaryZoneResolved,

    // Артефакт-спутник Клубочек добыт (NIT_MATERI, Степь — "второй биом
    // путешествия"). Событийный, доставляется напрямую из
    // AGridWorldManager::TryAcquireArtifact при успехе для "Клубочек",
    // тем же приёмом, что уже BuyanPathChosen/CoherentBrew.
    YarnBallAcquired
};

// 2026-09-02, Unit 6/6 миграции контента проекта на DataTable — тот же
// паттерн, что уже FArtifactDefinition/FDialogueDefinition и весь
// бестиарий: GetAllMemoryFragmentDefinitions() (MemoryFragmentDefinitions.h)
// лениво грузит /Game/Herbalist/Data/DT_MemoryFragments. FActiveMemoryFragment
// НЕ мигрирует — runtime-состояние заспавненного фрагмента, не карточка.
USTRUCT(BlueprintType)
struct PROJECTHERBALIST_API FMemoryFragmentDefinition : public FTableRowBase
{
    GENERATED_BODY()

    UPROPERTY() FName ID;

    // Явный порядок регистрации — тот же приём, что у остальных пяти
    // мигрированных реестров (см. FAmbientEntityDefinition::SortOrder).
    // Порядок не задокументирован как значимый для этого реестра (нет
    // тай-брейков между фрагментами), заведено для единообразия.
    UPROPERTY() int32 SortOrder = 0;

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
