// Core/World/ResourceSlots.cpp

#include "Core/World/ResourceSlots.h"
#include "Misc/PackageName.h"

void UHerbalistResourceSlots::ReplaceSet(const FString& SourceKey, TArray<FHerbalistResourceSlot>&& Slots)
{
    Sets.RemoveAll([&SourceKey](const FHerbalistResourceSlotSet& Set) { return Set.SourceKey == SourceKey; });
    if (Slots.Num() > 0)
    {
        FHerbalistResourceSlotSet& Set = Sets.AddDefaulted_GetRef();
        Set.SourceKey = SourceKey;
        Set.Slots = MoveTemp(Slots);
    }
    // Порядок наборов -- по ключу: запекание в любом порядке даёт тот же ассет.
    Sets.Sort([](const FHerbalistResourceSlotSet& A, const FHerbalistResourceSlotSet& B) { return A.SourceKey < B.SourceKey; });
}

int32 UHerbalistResourceSlots::CountSlots() const
{
    int32 Count = 0;
    for (const FHerbalistResourceSlotSet& Set : Sets)
    {
        Count += Set.Slots.Num();
    }
    return Count;
}

FString HerbalistResourceSlots::AssetPackagePathForMap(const FString& MapPackageName)
{
    FString ShortName = FPackageName::GetShortName(MapPackageName);
    // PIE: UEDPIE_<n>_<карта>.
    if (ShortName.StartsWith(TEXT("UEDPIE_")))
    {
        int32 Underscore = INDEX_NONE;
        const FString Rest = ShortName.Mid(7);
        if (Rest.FindChar(TEXT('_'), Underscore))
        {
            ShortName = Rest.Mid(Underscore + 1);
        }
    }
    return FString::Printf(TEXT("/Game/Data/ResourceSlots/RS_%s"), *ShortName);
}

bool HerbalistResourceSlots::SlotSuitsSpecies(EResourceSlotKind Kind, bool bAquaticSpecies)
{
    if (Kind == EResourceSlotKind::Shore)
    {
        return true;
    }
    return bAquaticSpecies == (Kind == EResourceSlotKind::Water);
}

EResourceSlotKind HerbalistResourceSlots::ParseKind(const FString& Text)
{
    if (Text.Equals(TEXT("Water"), ESearchCase::IgnoreCase)) return EResourceSlotKind::Water;
    if (Text.Equals(TEXT("Shore"), ESearchCase::IgnoreCase)) return EResourceSlotKind::Shore;
    return EResourceSlotKind::Land;
}
