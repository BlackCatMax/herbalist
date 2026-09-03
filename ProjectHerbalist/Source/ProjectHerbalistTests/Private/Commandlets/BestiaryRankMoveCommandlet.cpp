// BestiaryRankMoveCommandlet.cpp
#include "Commandlets/BestiaryRankMoveCommandlet.h"

#include "Core/Entities/AmbientEntityTypes.h"
#include "Core/Types/BiomeTypes.h"

#include "Engine/DataTable.h"
#include "UObject/SavePackage.h"
#include "Misc/PackageName.h"

namespace
{
    struct FMove
    {
        const TCHAR* EntityID;
        EBiomeType Biome;
        float MorokAffinity;   // из карточки -> порог Distortion
        bool bRequiresDusk;    // из документа, а не из головы
    };

    // Порядок объявления = порядок регистрации внутри своего биома. Все три
    // добавляются ПОСЛЕ уже существующих Низших (SortOrder продолжает
    // нумерацию), то есть на общих клетках биома приоритет остаётся у тех,
    // кто был реализован раньше -- перенос не должен молча отбирать клетку
    // у работающего существа.
    const FMove Moves[] = {
        // Карточка: «Степные курганы, особенно в сумерках», morok 0.50.
        { TEXT("Курганники"),      EBiomeType::ForestSteppe,    0.5f, false },
        // Карточка: «Изгороди, межи, заборы», morok 0.20.
        { TEXT("Жердяи"),          EBiomeType::BroadleafForest, 0.2f, false },
        // §16.2: «курган, сумерки» -- второй гейт настоящий, не заглушка.
        { TEXT("Курганные огни"),  EBiomeType::Steppe,          0.6f, true  },
    };

    bool SaveRankMoveTable(UDataTable* Table)
    {
        Table->MarkPackageDirty();
        UPackage* Package = Table->GetOutermost();
        const FString FileName = FPackageName::LongPackageNameToFilename(
            Package->GetName(), FPackageName::GetAssetPackageExtension());

        FSavePackageArgs Args;
        Args.TopLevelFlags = RF_Public | RF_Standalone;
        Args.SaveFlags = SAVE_NoError;
        return UPackage::SavePackage(Package, Table, *FileName, Args);
    }
}

int32 UBestiaryRankMoveCommandlet::Main(const FString& Params)
{
    UDataTable* AmbientTable = LoadObject<UDataTable>(nullptr, TEXT("/Game/Herbalist/Data/DT_AmbientEntities"));
    UDataTable* LandmarkTable = LoadObject<UDataTable>(nullptr, TEXT("/Game/Herbalist/Data/DT_Landmarks"));
    if (!AmbientTable || !LandmarkTable)
    {
        UE_LOG(LogTemp, Error, TEXT("BestiaryRankMove: таблицы бестиария не загрузились"));
        return 1;
    }

    // Продолжаем нумерацию от текущего размера Низшей таблицы -- ровно тот
    // же приём, что в BestiaryStubsAppend.
    int32 Order = AmbientTable->GetRowMap().Num();

    int32 Moved = 0, AlreadyThere = 0, NotInLandmarks = 0;

    for (const FMove& M : Moves)
    {
        const FName Id(M.EntityID);

        if (AmbientTable->GetRowMap().Contains(Id))
        {
            ++AlreadyThere;
            UE_LOG(LogTemp, Display, TEXT("  = %s уже в Низшем -- пропуск"), M.EntityID);
            continue;
        }

        if (!LandmarkTable->GetRowMap().Contains(Id))
        {
            ++NotInLandmarks;
            UE_LOG(LogTemp, Warning, TEXT("  ? %s нет и в Основном -- переносить нечего"), M.EntityID);
            continue;
        }

        FAmbientEntityDefinition Row;
        Row.EntityID = Id;
        Row.Biome = M.Biome;
        Row.SortOrder = Order++;

        // Курганы, межи и изгороди -- признак земли, взятый из карточки, а
        // не выдуманный баланс: дух кургана в воде был бы прямой ошибкой.
        Row.bLandOnly = true;

        Row.TriggerAxis = EAmbientTriggerAxis::Distortion;
        Row.bTriggerAbove = true;
        Row.TriggerThreshold = M.MorokAffinity;
        Row.bRequiresDusk = M.bRequiresDusk;

        // Ставки эффекта нулевые -- см. шапку заголовка.

        AmbientTable->AddRow(Id, Row);
        LandmarkTable->RemoveRow(Id);
        ++Moved;

        UE_LOG(LogTemp, Display, TEXT("  -> %s: Основной -> Низший (Distortion > %.2f%s, эффект пока нулевой)"),
            M.EntityID, M.MorokAffinity, M.bRequiresDusk ? TEXT(", сумерки") : TEXT(""));
    }

    if (Moved > 0)
    {
        if (!SaveRankMoveTable(AmbientTable))
        {
            UE_LOG(LogTemp, Error, TEXT("BestiaryRankMove: не удалось сохранить DT_AmbientEntities"));
            return 1;
        }
        if (!SaveRankMoveTable(LandmarkTable))
        {
            UE_LOG(LogTemp, Error, TEXT("BestiaryRankMove: не удалось сохранить DT_Landmarks"));
            return 1;
        }
    }

    UE_LOG(LogTemp, Display, TEXT("BestiaryRankMove: перенесено %d, уже в Низшем %d, не найдено в Основном %d"),
        Moved, AlreadyThere, NotInLandmarks);
    return 0;
}
