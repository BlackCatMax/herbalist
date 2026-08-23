// HerbalistJournalComponent.cpp
#include "HerbalistJournalComponent.h"

UHerbalistJournalComponent::UHerbalistJournalComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UHerbalistJournalComponent::AddEntry(const FJournalEntry& Entry)
{
    if (Entries.Num() >= MaxEntries)
    {
        Entries.RemoveAt(0, Entries.Num() - MaxEntries + 1);
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
