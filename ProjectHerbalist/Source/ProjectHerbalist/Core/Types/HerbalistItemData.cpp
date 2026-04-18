// HerbalistItemData.cpp
#include "HerbalistItemData.h"
#include "ProjectHerbalist.h"

FPrimaryAssetId UHerbalistItemData::GetPrimaryAssetId() const
{
    // Используем имя объекта как PrimaryAssetId
    return FPrimaryAssetId(StaticClass()->GetFName(), GetFName());
}