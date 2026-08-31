// Source/ProjectHerbalistTests/Private/Tests/JournalTest.cpp
//
// Полный аудит проекта (2026-08-31, по прямому запросу пользователя):
// Core/Journal (UHerbalistJournalComponent) был единственной подсистемой
// без единого теста и без упоминания в ROADMAP.md как известного пробела
// (в отличие от Perception/TraceReplay/WaterTypeRegistrySubsystem, которые
// уже отслежены). Компонент простой (ActorComponent на PlayerController,
// TArray<FJournalEntry> + вытеснение по MaxEntries) -- тестируется напрямую
// через NewObject, без мира/актора.
//
// По пути нашёлся реальный краш: AddEntry вычислял число записей на
// удаление как (Entries.Num() - MaxEntries + 1) без клампа. MaxEntries --
// EditAnywhere без ClampMin; при MaxEntries <= 0 формула требовала удалить
// больше элементов, чем есть в массиве -- TArray::RemoveAt падает на assert
// вне диапазона. Починено (HerbalistJournalComponent.cpp) до написания
// теста на этот случай -- тест писать на код, который заведомо крашит,
// нельзя (уронил бы весь прогон automation, не только один тест).

#include "Core/Journal/HerbalistJournalComponent.h"
#include "Misc/AutomationTest.h"

#if WITH_AUTOMATION_TESTS

namespace
{
    FJournalEntry MakeHarvestEntry(FName IngredientID, int32 Count)
    {
        FJournalEntry Entry;
        Entry.Type = EJournalEntryType::Harvest;
        Entry.IngredientID = IngredientID;
        Entry.Count = Count;
        return Entry;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistJournal_AddEntryAppendsToList,
    "Herbalist.Journal.AddEntryAppendsToList",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistJournal_AddEntryAppendsToList::RunTest(const FString& Parameters)
{
    UHerbalistJournalComponent* Journal = NewObject<UHerbalistJournalComponent>();
    if (!TestNotNull(TEXT("Journal component constructed"), Journal)) return false;

    TestEqual(TEXT("Starts empty"), Journal->GetEntries().Num(), 0);

    Journal->AddEntry(MakeHarvestEntry(FName(TEXT("Плакун-трава")), 3));

    TestEqual(TEXT("One entry after AddEntry"), Journal->GetEntries().Num(), 1);
    TestEqual(TEXT("Entry ingredient matches"), Journal->GetEntries()[0].IngredientID, FName(TEXT("Плакун-трава")));
    TestEqual(TEXT("Entry count matches"), Journal->GetEntries()[0].Count, 3);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistJournal_GetEntriesForIngredientFiltersCorrectly,
    "Herbalist.Journal.GetEntriesForIngredientFiltersCorrectly",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistJournal_GetEntriesForIngredientFiltersCorrectly::RunTest(const FString& Parameters)
{
    UHerbalistJournalComponent* Journal = NewObject<UHerbalistJournalComponent>();
    if (!TestNotNull(TEXT("Journal component constructed"), Journal)) return false;

    const FName Chistotel(TEXT("Чистотел"));
    const FName Medunitsa(TEXT("Медуница"));

    Journal->AddEntry(MakeHarvestEntry(Chistotel, 1));
    Journal->AddEntry(MakeHarvestEntry(Medunitsa, 1));
    Journal->AddEntry(MakeHarvestEntry(Chistotel, 2));

    const TArray<FJournalEntry> ChistotelEntries = Journal->GetEntriesForIngredient(Chistotel);
    TestEqual(TEXT("Two Чистотел entries found"), ChistotelEntries.Num(), 2);
    if (ChistotelEntries.Num() == 2)
    {
        TestEqual(TEXT("Order preserved (first Чистотел entry first)"), ChistotelEntries[0].Count, 1);
        TestEqual(TEXT("Order preserved (second Чистотел entry second)"), ChistotelEntries[1].Count, 2);
    }

    const TArray<FJournalEntry> MedunitsaEntries = Journal->GetEntriesForIngredient(Medunitsa);
    TestEqual(TEXT("One Медуница entry found"), MedunitsaEntries.Num(), 1);

    const TArray<FJournalEntry> UnknownEntries = Journal->GetEntriesForIngredient(FName(TEXT("НичегоТакогоНеСобирали")));
    TestEqual(TEXT("Unknown ingredient returns empty, not a crash"), UnknownEntries.Num(), 0);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistJournal_MaxEntriesEvictsOldestFirst,
    "Herbalist.Journal.MaxEntriesEvictsOldestFirst",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistJournal_MaxEntriesEvictsOldestFirst::RunTest(const FString& Parameters)
{
    UHerbalistJournalComponent* Journal = NewObject<UHerbalistJournalComponent>();
    if (!TestNotNull(TEXT("Journal component constructed"), Journal)) return false;

    Journal->MaxEntries = 3;
    for (int32 i = 1; i <= 5; ++i)
    {
        Journal->AddEntry(MakeHarvestEntry(FName(TEXT("Herb")), i));
    }

    TestEqual(TEXT("Capped at MaxEntries"), Journal->GetEntries().Num(), 3);
    const TArray<FJournalEntry>& Entries = Journal->GetEntries();
    if (Entries.Num() == 3)
    {
        TestEqual(TEXT("Oldest two (Count=1,2) evicted, Count=3 is now oldest kept"), Entries[0].Count, 3);
        TestEqual(TEXT("Middle kept"), Entries[1].Count, 4);
        TestEqual(TEXT("Newest kept"), Entries[2].Count, 5);
    }

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistJournal_MaxEntriesNonPositiveDoesNotCrash,
    "Herbalist.Journal.MaxEntriesNonPositiveDoesNotCrash",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistJournal_MaxEntriesNonPositiveDoesNotCrash::RunTest(const FString& Parameters)
{
    // Регрессия на баг, найденный при аудите 2026-08-31 (см. комментарий в
    // начале файла) -- MaxEntries не клампится в редакторе (EditAnywhere,
    // без ClampMin), 0 или отрицательное значение раньше валило
    // TArray::RemoveAt на попытке удалить больше элементов, чем есть.
    UHerbalistJournalComponent* Journal = NewObject<UHerbalistJournalComponent>();
    if (!TestNotNull(TEXT("Journal component constructed"), Journal)) return false;

    Journal->MaxEntries = 0;
    for (int32 i = 1; i <= 3; ++i)
    {
        Journal->AddEntry(MakeHarvestEntry(FName(TEXT("Herb")), i));
    }
    TestTrue(TEXT("MaxEntries=0 degrades to 'keep only the newest', doesn't crash"),
        Journal->GetEntries().Num() <= 1);
    if (Journal->GetEntries().Num() == 1)
    {
        TestEqual(TEXT("The one kept entry is the most recently added"), Journal->GetEntries()[0].Count, 3);
    }

    UHerbalistJournalComponent* NegativeJournal = NewObject<UHerbalistJournalComponent>();
    NegativeJournal->MaxEntries = -5;
    for (int32 i = 1; i <= 3; ++i)
    {
        NegativeJournal->AddEntry(MakeHarvestEntry(FName(TEXT("Herb")), i));
    }
    TestTrue(TEXT("Negative MaxEntries also degrades safely, doesn't crash"),
        NegativeJournal->GetEntries().Num() <= 1);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistJournal_RestoreEntriesReplacesListExactly,
    "Herbalist.Journal.RestoreEntriesReplacesListExactly",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistJournal_RestoreEntriesReplacesListExactly::RunTest(const FString& Parameters)
{
    // Core/Save/HerbalistSaveSubsystem.cpp::LoadGame -> PC->JournalComponent
    // ->RestoreEntries(Save->JournalEntries) -- должен ЗАМЕНИТЬ текущий
    // список, не слить с ним (иначе повторная загрузка того же сейва
    // задваивала бы записи).
    UHerbalistJournalComponent* Journal = NewObject<UHerbalistJournalComponent>();
    if (!TestNotNull(TEXT("Journal component constructed"), Journal)) return false;

    Journal->AddEntry(MakeHarvestEntry(FName(TEXT("ДоЗагрузки")), 1));
    Journal->AddEntry(MakeHarvestEntry(FName(TEXT("ДоЗагрузки")), 2));

    TArray<FJournalEntry> SavedEntries;
    SavedEntries.Add(MakeHarvestEntry(FName(TEXT("ИзСейва")), 99));

    Journal->RestoreEntries(SavedEntries);

    TestEqual(TEXT("Exactly one entry after restore, not merged with pre-load state"),
        Journal->GetEntries().Num(), 1);
    if (Journal->GetEntries().Num() == 1)
    {
        TestEqual(TEXT("Restored entry is the one from the save, not the old one"),
            Journal->GetEntries()[0].IngredientID, FName(TEXT("ИзСейва")));
    }

    return true;
}

#endif // WITH_AUTOMATION_TESTS
