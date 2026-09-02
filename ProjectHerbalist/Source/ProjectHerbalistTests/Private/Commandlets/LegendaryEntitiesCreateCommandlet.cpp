// LegendaryEntitiesCreateCommandlet.cpp
#include "Commandlets/LegendaryEntitiesCreateCommandlet.h"
#include "Core/Entities/LegendaryEntityTypes.h"
#include "Engine/DataTable.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "Misc/PackageName.h"
#include "AssetRegistry/AssetRegistryModule.h"

namespace
{
    // Построчная транскрипция прежнего литерального массива
    // LegendaryEntityTypes.h::GetLegendaryEntityDefinitions() (16
    // карточек) -- числа не менялись, только способ хранения.
    TArray<FLegendaryEntityDefinition> BuildLegendaryRows()
    {
        TArray<FLegendaryEntityDefinition> Defs;
        int32 Order = 0;

        // --- Опасный полюс ---
        // Болотный царь (Болото)
        {
            FLegendaryEntityDefinition D;
            D.EntityID = FName(TEXT("Болотный царь"));
            D.SortOrder = Order++;
            D.Biome = EBiomeType::Bog;
            D.Pole = ELegendaryPole::Malign;
            D.MorokThreshold = 0.75f;
            D.EffectAxis = ELandmarkAxis::Corruption; D.EffectRate = 0.03f;
            Defs.Add(D);
        }
        // Лихо Одноглазое (Болото)
        {
            FLegendaryEntityDefinition D;
            D.EntityID = FName(TEXT("Лихо Одноглазое"));
            D.SortOrder = Order++;
            D.Biome = EBiomeType::Bog;
            D.bLandOnly = true;
            D.Pole = ELegendaryPole::Malign;
            D.MorokThreshold = 0.8f;
            D.EffectAxis = ELandmarkAxis::Distortion; D.EffectRate = 0.03f;
            Defs.Add(D);
        }
        // Водяной царь (Речная пойма, ВОДА)
        {
            FLegendaryEntityDefinition D;
            D.EntityID = FName(TEXT("Водяной царь"));
            D.SortOrder = Order++;
            D.Biome = EBiomeType::Floodplain;
            D.bWaterOnly = true;
            D.Pole = ELegendaryPole::Malign;
            D.MorokThreshold = 0.75f;
            D.EffectAxis = ELandmarkAxis::Distortion; D.EffectRate = 0.025f;
            D.EffectAxis2 = ELandmarkAxis::Stability; D.EffectRate2 = -0.015f;
            Defs.Add(D);
        }
        // Суховей (Степь)
        {
            FLegendaryEntityDefinition D;
            D.EntityID = FName(TEXT("Суховей"));
            D.SortOrder = Order++;
            D.Biome = EBiomeType::Steppe;
            D.bLandOnly = true;
            D.Pole = ELegendaryPole::Malign;
            D.MorokThreshold = 0.7f;
            D.EffectAxis = ELandmarkAxis::Corruption; D.EffectRate = 0.02f;
            Defs.Add(D);
        }

        // --- Благой полюс ---
        // Дуб-старец (Широколиств. лес)
        {
            FLegendaryEntityDefinition D;
            D.EntityID = FName(TEXT("Дуб-старец"));
            D.SortOrder = Order++;
            D.Biome = EBiomeType::BroadleafForest;
            D.bLandOnly = true;
            D.Pole = ELegendaryPole::Benign;
            D.MorokThreshold = 0.2f;
            D.bHasShrinePath = true; D.ShrineThreshold = 0.7f;
            D.EffectAxis = ELandmarkAxis::Stability; D.EffectRate = 0.015f;
            Defs.Add(D);
        }
        // Гамаюн (Лесостепь)
        {
            FLegendaryEntityDefinition D;
            D.EntityID = FName(TEXT("Гамаюн"));
            D.SortOrder = Order++;
            D.Biome = EBiomeType::ForestSteppe;
            D.Pole = ELegendaryPole::Benign;
            D.MorokThreshold = 0.2f;
            D.bHasShrinePath = true; D.ShrineThreshold = 0.7f;
            D.EffectAxis = ELandmarkAxis::Purity; D.EffectRate = 0.015f;
            Defs.Add(D);
        }
        // Алконост (Степь)
        {
            FLegendaryEntityDefinition D;
            D.EntityID = FName(TEXT("Алконост"));
            D.SortOrder = Order++;
            D.Biome = EBiomeType::Steppe;
            D.bLandOnly = true;
            D.Pole = ELegendaryPole::Benign;
            D.MorokThreshold = 0.2f;
            D.bHasShrinePath = true; D.ShrineThreshold = 0.7f;
            D.EffectAxis = ELandmarkAxis::Purity; D.EffectRate = 0.012f;
            Defs.Add(D);
        }
        // Мать-Сыра-Земля (степная)
        {
            FLegendaryEntityDefinition D;
            D.EntityID = FName(TEXT("Мать-Сыра-Земля"));
            D.SortOrder = Order++;
            D.Biome = EBiomeType::Steppe;
            D.bLandOnly = true;
            D.Pole = ELegendaryPole::Benign;
            D.MorokThreshold = 0.2f;
            D.bHasShrinePath = true; D.ShrineThreshold = 0.65f;
            D.EffectAxis = ELandmarkAxis::Stability; D.EffectRate = 0.012f;
            D.EffectAxis2 = ELandmarkAxis::Purity;   D.EffectRate2 = 0.008f;
            Defs.Add(D);
        }
        // Индрик-зверь (Тайга)
        {
            FLegendaryEntityDefinition D;
            D.EntityID = FName(TEXT("Индрик-зверь"));
            D.SortOrder = Order++;
            D.Biome = EBiomeType::Taiga;
            D.bLandOnly = true;
            D.Pole = ELegendaryPole::Benign;
            D.MorokThreshold = 0.2f;
            D.bHasShrinePath = true; D.ShrineThreshold = 0.7f;
            D.EffectAxis = ELandmarkAxis::Potency; D.EffectRate = 0.012f;
            D.EffectAxis2 = ELandmarkAxis::Nature; D.EffectRate2 = 0.01f;
            Defs.Add(D);
        }
        // Волот (Тундра)
        {
            FLegendaryEntityDefinition D;
            D.EntityID = FName(TEXT("Волот"));
            D.SortOrder = Order++;
            D.Biome = EBiomeType::Tundra;
            D.bLandOnly = true;
            D.Pole = ELegendaryPole::Benign;
            D.MorokThreshold = 0.2f;
            D.bHasShrinePath = true; D.ShrineThreshold = 0.7f;
            D.EffectAxis = ELandmarkAxis::Stability; D.EffectRate = 0.015f;
            Defs.Add(D);
        }
        // Полкан (Лесостепь)
        {
            FLegendaryEntityDefinition D;
            D.EntityID = FName(TEXT("Полкан"));
            D.SortOrder = Order++;
            D.Biome = EBiomeType::ForestSteppe;
            D.bLandOnly = true;
            D.Pole = ELegendaryPole::Benign;
            D.MorokThreshold = 0.2f;
            D.bHasShrinePath = true; D.ShrineThreshold = 0.7f;
            D.EffectAxis = ELandmarkAxis::Body; D.EffectRate = 0.012f;
            Defs.Add(D);
        }
        // Вольга (Тайга)
        {
            FLegendaryEntityDefinition D;
            D.EntityID = FName(TEXT("Вольга"));
            D.SortOrder = Order++;
            D.Biome = EBiomeType::Taiga;
            D.bLandOnly = true;
            D.Pole = ELegendaryPole::Benign;
            D.MorokThreshold = 0.2f;
            D.bHasShrinePath = true; D.ShrineThreshold = 0.7f;
            D.EffectAxis = ELandmarkAxis::Body; D.EffectRate = 0.012f;
            Defs.Add(D);
        }
        // Баба-Яга (Смеш. лес)
        {
            FLegendaryEntityDefinition D;
            D.EntityID = FName(TEXT("Баба-Яга"));
            D.SortOrder = Order++;
            D.Biome = EBiomeType::MixedForest;
            D.bLandOnly = true;
            D.Pole = ELegendaryPole::Benign;
            D.MorokThreshold = 0.2f;
            D.bHasShrinePath = true; D.ShrineThreshold = 0.7f;
            D.EffectAxis = ELandmarkAxis::Body;   D.EffectRate = 0.01f;
            D.EffectAxis2 = ELandmarkAxis::Nature; D.EffectRate2 = 0.01f;
            Defs.Add(D);
        }
        // жар-птица (Смеш. лес)
        {
            FLegendaryEntityDefinition D;
            D.EntityID = FName(TEXT("жар-птица"));
            D.SortOrder = Order++;
            D.Biome = EBiomeType::MixedForest;
            D.bLandOnly = true;
            D.Pole = ELegendaryPole::Benign;
            D.MorokThreshold = 0.2f;
            D.bHasShrinePath = true; D.ShrineThreshold = 0.75f;
            D.EffectAxis = ELandmarkAxis::Resonance; D.EffectRate = 0.015f;
            Defs.Add(D);
        }

        // --- Зеркальный полюс (Malign по триггеру, не хищный эффект) ---
        // Сирин (Тундра)
        {
            FLegendaryEntityDefinition D;
            D.EntityID = FName(TEXT("Сирин"));
            D.SortOrder = Order++;
            D.Biome = EBiomeType::Tundra;
            D.bLandOnly = true;
            D.Pole = ELegendaryPole::Malign;
            D.MorokThreshold = 0.7f;
            D.EffectAxis = ELandmarkAxis::Distortion; D.EffectRate = 0.02f;
            D.EffectAxis2 = ELandmarkAxis::Resonance; D.EffectRate2 = 0.015f;
            Defs.Add(D);
        }
        // Кикимора-владычица (Широколиств. лес)
        {
            FLegendaryEntityDefinition D;
            D.EntityID = FName(TEXT("Кикимора-владычица"));
            D.SortOrder = Order++;
            D.Biome = EBiomeType::BroadleafForest;
            D.bLandOnly = true;
            D.Pole = ELegendaryPole::Malign;
            D.MorokThreshold = 0.7f;
            D.EffectAxis = ELandmarkAxis::Distortion; D.EffectRate = 0.02f;
            D.EffectAxis2 = ELandmarkAxis::Resonance; D.EffectRate2 = 0.012f;
            Defs.Add(D);
        }

        // --- Per-клеточный триггер (не якорный) ---
        // Берегиня (Речная пойма, ВОДА) -- 2026-09-02, унификация: раньше
        // жёстко закодирована в GridWorldManagerEntities.cpp, 17-я карточка
        // этого реестра, bUsesCellHistoryPurity=true. Значения -- прежние
        // BereginyaHistoryPurityThreshold/BereginyaShrineRestorationThreshold
        // из HerbalistSettings.h (0.75/0.7, теперь удалены оттуда как
        // мёртвый код) и floor-эффект (Purity >= 0.9), прежде хардкоженный
        // в самом тик-коде.
        {
            FLegendaryEntityDefinition D;
            D.EntityID = FName(TEXT("Берегиня"));
            D.SortOrder = Order++;
            D.Biome = EBiomeType::Floodplain;
            D.bWaterOnly = true;
            D.Pole = ELegendaryPole::Benign;
            D.bUsesCellHistoryPurity = true;
            D.HistoryPurityThreshold = 0.75f;
            D.bHasShrinePath = true; D.ShrineThreshold = 0.7f;
            D.bFloorEffect = true;
            D.EffectAxis = ELandmarkAxis::Purity; D.EffectRate = 0.9f;
            Defs.Add(D);
        }

        return Defs;
    }
}

int32 ULegendaryEntitiesCreateCommandlet::Main(const FString& Params)
{
    const TCHAR* AssetPath = TEXT("/Game/Herbalist/Data/DT_LegendaryEntities");

    if (UDataTable* Existing = LoadObject<UDataTable>(nullptr, AssetPath))
    {
        UE_LOG(LogTemp, Display, TEXT("LegendaryEntitiesCreate: %s уже существует (%d рядов), ничего не делаю"),
            AssetPath, Existing->GetRowMap().Num());
        return 0;
    }

    UPackage* Package = CreatePackage(AssetPath);
    if (!Package)
    {
        UE_LOG(LogTemp, Error, TEXT("LegendaryEntitiesCreate: не удалось создать пакет %s"), AssetPath);
        return 1;
    }

    UDataTable* Table = NewObject<UDataTable>(Package, FName(TEXT("DT_LegendaryEntities")), RF_Public | RF_Standalone);
    if (!Table)
    {
        UE_LOG(LogTemp, Error, TEXT("LegendaryEntitiesCreate: не удалось создать UDataTable"));
        return 1;
    }
    Table->RowStruct = FLegendaryEntityDefinition::StaticStruct();

    FAssetRegistryModule::AssetCreated(Table);

    int32 AddedCount = 0;
    for (const FLegendaryEntityDefinition& Row : BuildLegendaryRows())
    {
        Table->AddRow(Row.EntityID, Row);
        ++AddedCount;
    }

    Table->MarkPackageDirty();

    const FString PackageFileName = FPackageName::LongPackageNameToFilename(
        Package->GetName(), FPackageName::GetAssetPackageExtension());

    FSavePackageArgs SaveArgs;
    SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
    SaveArgs.SaveFlags = SAVE_NoError;

    const bool bSuccess = UPackage::SavePackage(Package, Table, *PackageFileName, SaveArgs);
    if (!bSuccess)
    {
        UE_LOG(LogTemp, Error, TEXT("LegendaryEntitiesCreate: не удалось сохранить пакет %s"), *PackageFileName);
        return 1;
    }

    UE_LOG(LogTemp, Display, TEXT("LegendaryEntitiesCreate: создан %s, %d рядов добавлено"), AssetPath, AddedCount);
    return 0;
}
