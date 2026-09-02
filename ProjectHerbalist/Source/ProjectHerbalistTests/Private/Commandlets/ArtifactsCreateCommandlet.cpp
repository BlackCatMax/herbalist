// ArtifactsCreateCommandlet.cpp
#include "Commandlets/ArtifactsCreateCommandlet.h"
#include "Core/Entities/ArtifactTypes.h"
#include "Engine/DataTable.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "Misc/PackageName.h"
#include "AssetRegistry/AssetRegistryModule.h"

namespace
{
    // Построчная транскрипция прежнего литерального массива
    // ArtifactTypes.h::GetArtifactDefinitions() (8 карточек) -- значения
    // не менялись, только способ хранения.
    TArray<FArtifactDefinition> BuildArtifactRows()
    {
        TArray<FArtifactDefinition> Defs;
        int32 Order = 0;

        {
            FArtifactDefinition D;
            D.ArtifactID = FName(TEXT("Зеркальце"));
            D.SortOrder = Order++;
            D.LegendaryEntityID = FName(TEXT("Гамаюн"));
            D.Biome = EBiomeType::ForestSteppe;
            D.bWarmsCompanionItem = true;
            Defs.Add(D);
        }
        {
            FArtifactDefinition D;
            D.ArtifactID = FName(TEXT("Клубочек"));
            D.SortOrder = Order++;
            D.LegendaryEntityID = FName(TEXT("Мать-Сыра-Земля"));
            D.Biome = EBiomeType::Steppe;
            D.bWarmsCompanionItem = true;
            Defs.Add(D);
        }
        {
            FArtifactDefinition D;
            D.ArtifactID = FName(TEXT("Рог"));
            D.SortOrder = Order++;
            D.LegendaryEntityID = FName(TEXT("Индрик-зверь"));
            D.Biome = EBiomeType::Taiga;
            Defs.Add(D);
        }
        {
            FArtifactDefinition D;
            D.ArtifactID = FName(TEXT("Гребень"));
            D.SortOrder = Order++;
            D.LegendaryEntityID = NAME_None;   // Берегиня — особый путь
            D.Biome = EBiomeType::Floodplain;
            Defs.Add(D);
        }
        {
            FArtifactDefinition D;
            D.ArtifactID = FName(TEXT("Молодильное яблоко"));
            D.SortOrder = Order++;
            D.LegendaryEntityID = FName(TEXT("Дуб-старец"));
            D.Biome = EBiomeType::BroadleafForest;
            Defs.Add(D);
        }
        {
            FArtifactDefinition D;
            D.ArtifactID = FName(TEXT("Шапка-невидимка"));
            D.SortOrder = Order++;
            D.LegendaryEntityID = FName(TEXT("Баба-Яга"));
            D.Biome = EBiomeType::MixedForest;
            Defs.Add(D);
        }
        {
            FArtifactDefinition D;
            D.ArtifactID = FName(TEXT("Камень-оберег"));
            D.SortOrder = Order++;
            D.LegendaryEntityID = FName(TEXT("Волот"));
            D.Biome = EBiomeType::Tundra;
            Defs.Add(D);
        }
        {
            FArtifactDefinition D;
            D.ArtifactID = FName(TEXT("Фонарь"));
            D.SortOrder = Order++;
            D.LegendaryEntityID = FName(TEXT("Болотный царь"));
            D.Biome = EBiomeType::Bog;
            D.bDeceptionOnly = true;
            D.bWarmsFromGlobalClarity = true;
            Defs.Add(D);
        }

        return Defs;
    }
}

int32 UArtifactsCreateCommandlet::Main(const FString& Params)
{
    const TCHAR* AssetPath = TEXT("/Game/Herbalist/Data/DT_Artifacts");

    if (UDataTable* Existing = LoadObject<UDataTable>(nullptr, AssetPath))
    {
        UE_LOG(LogTemp, Display, TEXT("ArtifactsCreate: %s уже существует (%d рядов), ничего не делаю"),
            AssetPath, Existing->GetRowMap().Num());
        return 0;
    }

    UPackage* Package = CreatePackage(AssetPath);
    if (!Package)
    {
        UE_LOG(LogTemp, Error, TEXT("ArtifactsCreate: не удалось создать пакет %s"), AssetPath);
        return 1;
    }

    UDataTable* Table = NewObject<UDataTable>(Package, FName(TEXT("DT_Artifacts")), RF_Public | RF_Standalone);
    if (!Table)
    {
        UE_LOG(LogTemp, Error, TEXT("ArtifactsCreate: не удалось создать UDataTable"));
        return 1;
    }
    Table->RowStruct = FArtifactDefinition::StaticStruct();

    FAssetRegistryModule::AssetCreated(Table);

    int32 AddedCount = 0;
    for (const FArtifactDefinition& Row : BuildArtifactRows())
    {
        Table->AddRow(Row.ArtifactID, Row);
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
        UE_LOG(LogTemp, Error, TEXT("ArtifactsCreate: не удалось сохранить пакет %s"), *PackageFileName);
        return 1;
    }

    UE_LOG(LogTemp, Display, TEXT("ArtifactsCreate: создан %s, %d рядов добавлено"), AssetPath, AddedCount);
    return 0;
}
