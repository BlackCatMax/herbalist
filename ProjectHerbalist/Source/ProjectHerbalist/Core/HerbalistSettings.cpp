// HerbalistSettings.cpp
#include "HerbalistSettings.h"
#include "Engine/Engine.h"

UHerbalistSettings::UHerbalistSettings()
{
    CategoryName = TEXT("Plugins");
    SectionName = TEXT("Herbalist");
}

UHerbalistSettings* GetHerbalistSettings()
{
    return GetMutableDefault<UHerbalistSettings>();
}
