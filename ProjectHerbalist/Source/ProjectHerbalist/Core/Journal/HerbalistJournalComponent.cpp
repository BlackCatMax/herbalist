// HerbalistJournalComponent.cpp
#include "HerbalistJournalComponent.h"

UHerbalistJournalComponent::UHerbalistJournalComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UHerbalistJournalComponent::AddEntry(const FJournalEntry& Entry)
{
    // Аудит 2026-08-31: MaxEntries -- EditAnywhere без ClampMin. При
    // MaxEntries <= 0 старая формула (Entries.Num() - MaxEntries + 1) могла
    // потребовать удалить больше элементов, чем реально есть в массиве --
    // TArray::RemoveAt падает на assert вне допустимого диапазона. Clamp
    // числа удаляемых элементов в [0, Entries.Num()] чинит это для любого
    // MaxEntries <= 0 одним и тем же путём (вырождается в "храним только
    // последнюю запись"), не только для 0 конкретно.
    if (Entries.Num() >= MaxEntries)
    {
        const int32 NumToRemove = FMath::Clamp(Entries.Num() - MaxEntries + 1, 0, Entries.Num());
        if (NumToRemove > 0)
        {
            Entries.RemoveAt(0, NumToRemove);
        }
    }
    Entries.Add(Entry);
    OnJournalEntryAdded.Broadcast();
}

TArray<FJournalEntry> UHerbalistJournalComponent::GetEntriesForIngredient(FName IngredientID) const
{
    TArray<FJournalEntry> Result;
    for (const FJournalEntry& Entry : Entries)
    {
        if (Entry.IngredientID == IngredientID)
        {
            Result.Add(Entry);
        }
    }
    return Result;
}
