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
    Brew,
    // Фрагмент памяти Заряны (Core/Zaryana/MemoryFragmentTypes.h), 2026-08-29 —
    // закрывает разрыв "игрок собирает фрагмент, видит только UE_LOG": текст
    // (FragmentText ниже) теперь читается тем же экраном, что харвест/варка,
    // не теряется после экранного попапа (UI/MemoryRevealWidget.h).
    MemoryFragment
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

    // Фаза суток на момент записи (15_Cycles_And_Shrines §15.2). Луна/сезон
    // с 2026-08-24/29 тоже реализованы в коде (GetMoonPhase/GetSeason), но
    // сюда, в саму запись, ещё не добавлены отдельными полями — не
    // блокирует ничего конкретного, просто не заведено.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Journal")
    bool bWasNight = false;

    // GetWorld()->GetTimeSeconds() — тот же единственный источник времени,
    // что уже используется для CreationTime предметов и суточного цикла
    // (AUDIT_AND_REFACTORING_PLAN §3.4, устранённое дублирование часов).
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Journal")
    float GameTimeSeconds = 0.0f;

    // Только для Type == MemoryFragment — само воспоминание (Def->TrueText/
    // FalseText, см. GridWorldManagerZaryana.cpp::CollectMemoryFragment).
    // IngredientID/Count/PerceivedState для этого типа не используются
    // (остаются на дефолтах), Cell/Biome/bWasNight/GameTimeSeconds — как есть.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Journal")
    FText FragmentText;

    // Подлинное (true) или искажённое Мороком (false) воспоминание. Ложные
    // тоже записываются — игрок должен иметь возможность потом сравнить и
    // научиться различать, не только в момент сбора (06_Progression.md:
    // прогрессия как сжатие ошибки через сравнение).
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Journal")
    bool bFragmentWasTrue = true;
};
