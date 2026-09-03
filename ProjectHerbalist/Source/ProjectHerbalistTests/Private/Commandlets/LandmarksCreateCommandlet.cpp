// LandmarksCreateCommandlet.cpp
#include "Commandlets/LandmarksCreateCommandlet.h"
#include "Core/Entities/LandmarkTypes.h"
#include "Engine/DataTable.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "Misc/PackageName.h"
#include "AssetRegistry/AssetRegistryModule.h"

namespace
{
    // Построчная транскрипция прежнего литерального массива
    // LandmarkTypes.h::GetLandmarkDefinitions() (15 карточек) -- числа не
    // менялись, только способ хранения.
    TArray<FLandmarkDefinition> BuildLandmarkRows()
    {
        TArray<FLandmarkDefinition> Defs;
        int32 Order = 0;

        // Полевик (Лесостепь)
        {
            FLandmarkDefinition D;
            D.EntityID = FName(TEXT("Полевик"));
            D.SortOrder = Order++;
            D.Biome = EBiomeType::ForestSteppe;
            D.BlessAxis = ELandmarkAxis::Potency;  D.BlessRate = 0.01f;
            D.BlessAxis2 = ELandmarkAxis::Purity;  D.BlessRate2 = 0.005f;
            D.CurseAxis = ELandmarkAxis::Stability; D.CurseRate = -0.02f;
            Defs.Add(D);
        }
        // Аука (Тайга)
        {
            FLandmarkDefinition D;
            D.EntityID = FName(TEXT("Аука"));
            D.SortOrder = Order++;
            D.Biome = EBiomeType::Taiga;
            D.BlessAxis = ELandmarkAxis::Mind; D.BlessRate = 0.01f;
            D.CurseAxis = ELandmarkAxis::Distortion; D.CurseRate = 0.02f;
            Defs.Add(D);
        }
        // Дух Медведя (Тайга)
        {
            FLandmarkDefinition D;
            D.EntityID = FName(TEXT("Дух Медведя"));
            D.SortOrder = Order++;
            D.Biome = EBiomeType::Taiga;
            D.BlessAxis = ELandmarkAxis::Body; D.BlessRate = 0.01f;
            D.CurseAxis = ELandmarkAxis::Stability; D.CurseRate = -0.025f;
            Defs.Add(D);
        }
        // Хозяин Севера (Тундра)
        {
            FLandmarkDefinition D;
            D.EntityID = FName(TEXT("Хозяин Севера"));
            D.SortOrder = Order++;
            D.Biome = EBiomeType::Tundra;
            D.BlessAxis = ELandmarkAxis::Stability; D.BlessRate = 0.01f;
            D.CurseAxis = ELandmarkAxis::Distortion; D.CurseRate = 0.02f;
            Defs.Add(D);
        }
        // Гуменник (Широколиств. лес)
        {
            FLandmarkDefinition D;
            D.EntityID = FName(TEXT("Гуменник"));
            D.SortOrder = Order++;
            D.Biome = EBiomeType::BroadleafForest;
            D.BlessAxis = ELandmarkAxis::Potency; D.BlessRate = 0.01f;
            D.CurseAxis = ELandmarkAxis::Stability; D.CurseRate = -0.02f;
            Defs.Add(D);
        }
        // Овинник (Широколиств. лес)
        {
            FLandmarkDefinition D;
            D.EntityID = FName(TEXT("Овинник"));
            D.SortOrder = Order++;
            D.Biome = EBiomeType::BroadleafForest;
            D.BlessAxis = ELandmarkAxis::Purity; D.BlessRate = 0.01f;
            D.CurseAxis = ELandmarkAxis::Corruption; D.CurseRate = 0.02f;
            Defs.Add(D);
        }
        // Кикимора болотная (Болото)
        {
            FLandmarkDefinition D;
            D.EntityID = FName(TEXT("Кикимора болотная"));
            D.SortOrder = Order++;
            D.Biome = EBiomeType::Bog;
            D.BlessAxis = ELandmarkAxis::Resonance; D.BlessRate = 0.01f;
            D.CurseAxis = ELandmarkAxis::Distortion; D.CurseRate = 0.025f;
            Defs.Add(D);
        }
        // Переплут (Степь)
        {
            FLandmarkDefinition D;
            D.EntityID = FName(TEXT("Переплут"));
            D.SortOrder = Order++;
            D.Biome = EBiomeType::Steppe;
            D.BlessAxis = ELandmarkAxis::Body; D.BlessRate = 0.008f;
            D.BlessAxis2 = ELandmarkAxis::Nature; D.BlessRate2 = 0.008f;
            D.CurseAxis = ELandmarkAxis::Stability; D.CurseRate = -0.02f;
            Defs.Add(D);
        }
        // Бродницы (Речная пойма)
        {
            FLandmarkDefinition D;
            D.EntityID = FName(TEXT("Бродницы"));
            D.SortOrder = Order++;
            D.Biome = EBiomeType::Floodplain;
            D.BlessAxis = ELandmarkAxis::Stability; D.BlessRate = 0.01f;
            D.CurseAxis = ELandmarkAxis::Distortion; D.CurseRate = 0.02f;
            Defs.Add(D);
        }
        // Боровик (Смеш. лес)
        {
            FLandmarkDefinition D;
            D.EntityID = FName(TEXT("Боровик"));
            D.SortOrder = Order++;
            D.Biome = EBiomeType::MixedForest;
            D.BlessAxis = ELandmarkAxis::Potency; D.BlessRate = 0.01f;
            D.CurseAxis = ELandmarkAxis::Stability; D.CurseRate = -0.02f;
            Defs.Add(D);
        }
        // Луговой (Смеш. лес)
        {
            FLandmarkDefinition D;
            D.EntityID = FName(TEXT("Луговой"));
            D.SortOrder = Order++;
            D.Biome = EBiomeType::MixedForest;
            D.BlessAxis = ELandmarkAxis::Purity; D.BlessRate = 0.01f;
            D.CurseAxis = ELandmarkAxis::Distortion; D.CurseRate = 0.015f;
            Defs.Add(D);
        }
        // Курганники / Жердяи / Курганные огни СТОЯЛИ ЗДЕСЬ и убраны
        // 2026-09-03: сверка -run=CompendiumAudit показала расхождение
        // ранга, и документация говорит «Низший» четырьмя независимыми
        // местами (карточки бестиария с id *_low_*, карточки биомов,
        // таблица §16.2, отсутствие в списке §16.3) против одной фразы
        // прозы §16.2. Их определения теперь в AmbientEntitiesCreate,
        // перенос уже сделанных ассетов -- -run=BestiaryRankMove.
        // Ставки Bless/Curse, стоявшие тут, были выдуманы этим генератором,
        // а не взяты из документа, поэтому в Низший они не переехали.
        // Домовой (жилище игрока, bManualRegistrationOnly -- не сеется по биому)
        {
            FLandmarkDefinition D;
            D.EntityID = FName(TEXT("Домовой"));
            D.SortOrder = Order++;
            D.bManualRegistrationOnly = true;
            D.BlessAxis = ELandmarkAxis::Stability; D.BlessRate = 0.01f;
            D.CurseAxis = ELandmarkAxis::Corruption; D.CurseRate = 0.015f;
            D.AggravatedCurseAxis = ELandmarkAxis::Stability;
            D.AggravatedCurseRate = -0.02f;
            D.AggravatedCurseThreshold = -0.6f;
            Defs.Add(D);
        }

        return Defs;
    }
}

int32 ULandmarksCreateCommandlet::Main(const FString& Params)
{
    const TCHAR* AssetPath = TEXT("/Game/Herbalist/Data/DT_Landmarks");

    if (UDataTable* Existing = LoadObject<UDataTable>(nullptr, AssetPath))
    {
        UE_LOG(LogTemp, Display, TEXT("LandmarksCreate: %s уже существует (%d рядов), ничего не делаю"),
            AssetPath, Existing->GetRowMap().Num());
        return 0;
    }

    UPackage* Package = CreatePackage(AssetPath);
    if (!Package)
    {
        UE_LOG(LogTemp, Error, TEXT("LandmarksCreate: не удалось создать пакет %s"), AssetPath);
        return 1;
    }

    UDataTable* Table = NewObject<UDataTable>(Package, FName(TEXT("DT_Landmarks")), RF_Public | RF_Standalone);
    if (!Table)
    {
        UE_LOG(LogTemp, Error, TEXT("LandmarksCreate: не удалось создать UDataTable"));
        return 1;
    }
    Table->RowStruct = FLandmarkDefinition::StaticStruct();

    FAssetRegistryModule::AssetCreated(Table);

    int32 AddedCount = 0;
    for (const FLandmarkDefinition& Row : BuildLandmarkRows())
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
        UE_LOG(LogTemp, Error, TEXT("LandmarksCreate: не удалось сохранить пакет %s"), *PackageFileName);
        return 1;
    }

    UE_LOG(LogTemp, Display, TEXT("LandmarksCreate: создан %s, %d рядов добавлено"), AssetPath, AddedCount);
    return 0;
}
