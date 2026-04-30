// Core/HerbalistSettings.cpp
#include "HerbalistSettings.h"
#include "Engine/Engine.h"
#include "Core/BiomeGraph/BiomeGraphAsset.h"
#include "UI/AlchemyTransferWidget.h"

UHerbalistSettings::UHerbalistSettings()
{
    CategoryName = TEXT("Plugins");
    SectionName = TEXT("Herbalist");

    IngredientTableAsset = TSoftObjectPtr<UDataTable>(FSoftObjectPath(TEXT("/Game/Herbalist/Data/DT_IngredientClass.DT_IngredientClass")));
    WaterTypeTableAsset = TSoftObjectPtr<UDataTable>(FSoftObjectPath(TEXT("/Game/Herbalist/Data/DT_WaterTypes.DT_WaterTypes")));
    BiomeDefaultsTableAsset = TSoftObjectPtr<UDataTable>(FSoftObjectPath(TEXT("/Game/Data/DT_BiomeDefaults.DT_BiomeDefaults")));
    BiomeGraphAsset = TSoftObjectPtr<UBiomeGraphAsset>(FSoftObjectPath(TEXT("/Game/Data/DA_BiomeGraph.DA_BiomeGraph")));
    AlchemyTransferWidgetClass = TSoftClassPtr<UAlchemyTransferWidget>(FSoftObjectPath(TEXT("/Game/UI/WBP_AlchemyTransfer.WBP_AlchemyTransfer_C")));
}

UHerbalistSettings* GetHerbalistSettings()
{
    return GetMutableDefault<UHerbalistSettings>();
}