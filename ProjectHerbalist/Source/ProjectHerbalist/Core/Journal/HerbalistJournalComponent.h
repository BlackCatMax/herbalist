// HerbalistJournalComponent.h
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Core/Journal/JournalTypes.h"
#include "HerbalistJournalComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnJournalEntryAdded);

// Травник (07_UX §7.2.4) — минимальная реализация: автофиксация, без
// подсветки закономерностей и сравнения (это следующие шаги, см.
// ROADMAP.md §2.1). Пишется тем же способом, что и HerbalistInventoryComponent
// рядом — компонент на PlayerController, не подсистема.
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class PROJECTHERBALIST_API UHerbalistJournalComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UHerbalistJournalComponent();

    // Бесконечно расти список не может — старые записи вытесняются новыми.
    // 2000 — на порядок больше, чем игрок соберёт/сварит за одну сессию;
    // ограничение против неограниченного роста, не игровой баланс.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Journal")
    int32 MaxEntries = 2000;

    void AddEntry(const FJournalEntry& Entry);

    UFUNCTION(BlueprintCallable, Category = "Journal")
    const TArray<FJournalEntry>& GetEntries() const { return Entries; }

    // Записи по конкретному ингредиенту — то, на чём будет строиться
    // "подсветка закономерностей" (07_UX §7.2.4), когда до неё дойдёт очередь.
    UFUNCTION(BlueprintCallable, Category = "Journal")
    TArray<FJournalEntry> GetEntriesForIngredient(FName IngredientID) const;

    // Точное восстановление из сохранения (Core/Save/HerbalistSaveTypes.h).
    void RestoreEntries(const TArray<FJournalEntry>& InEntries) { Entries = InEntries; }

    UPROPERTY(BlueprintAssignable, Category = "Journal")
    FOnJournalEntryAdded OnJournalEntryAdded;

protected:
    UPROPERTY()
    TArray<FJournalEntry> Entries;
};
