// JournalTypes.h
#pragma once

#include "CoreMinimal.h"
#include "Core/Types/HerbalistCoreTypes.h"
#include "Core/Types/BiomeTypes.h"
#include "JournalTypes.generated.h"

UENUM(BlueprintType)
enum class EJournalEntryType : uint8
{
    Harvest,
    Brew
};

// Одна запись Травника — 06_Progression.md: прогрессия как "сжатие ошибки
// между ожиданием и результатом" требует, чтобы игроку было что сравнивать.
// 07_UX §7.2.4: "каждое созданное зелье и каждый собранный ресурс
// записываются в Травник с указанием контекста (где, когда, в каких
// условиях)".
//
// КРИТИЧНО ДЛЯ ЭПИСТЕМИКИ ИГРЫ: PerceivedState — это ВСЕГДА искажённое
// (S_Perceived) состояние, замороженное в момент записи, никогда не
// S_real. Записывать сюда настоящее состояние значило бы дать игроку
// потайной доступ к истине через Травник — прямое нарушение всего, на чём
// стоит проект (01_Introduction: "игрок никогда не взаимодействует
// напрямую с объективным состоянием"). Заморожено намеренно, не
// пересчитывается при каждом просмотре — иначе игрок мог бы вычислить
// истину усреднением многих показов одной записи.
USTRUCT(BlueprintType)
struct PROJECTHERBALIST_API FJournalEntry
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Journal")
    EJournalEntryType Type = EJournalEntryType::Harvest;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Journal")
    FName IngredientID = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Journal")
    int32 Count = 1;

    // См. предупреждение выше — искажённое, не настоящее.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Journal")
    FRealState PerceivedState;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Journal")
    FIntPoint Cell = FIntPoint(-1, -1);

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Journal")
    EBiomeType Biome = EBiomeType::MixedForest;

    // Фаза суток на момент записи (15_Cycles_And_Shrines §15.2) — единственный
    // уже реализованный цикл; луна/сезон добавятся тем же полем, когда дойдут
    // до кода (ROADMAP.md §3.2).
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Journal")
    bool bWasNight = false;

    // GetWorld()->GetTimeSeconds() — тот же единственный источник времени,
    // что уже используется для CreationTime предметов и суточного цикла
    // (AUDIT_AND_REFACTORING_PLAN §3.4, устранённое дублирование часов).
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Journal")
    float GameTimeSeconds = 0.0f;
};
