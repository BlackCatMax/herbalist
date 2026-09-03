// BestiaryStubsAppendCommandlet.cpp
#include "Commandlets/BestiaryStubsAppendCommandlet.h"

#include "Core/Entities/AmbientEntityTypes.h"
#include "Core/Entities/LandmarkTypes.h"
#include "Core/Types/BiomeTypes.h"

#include "Engine/DataTable.h"
#include "UObject/SavePackage.h"
#include "Misc/PackageName.h"

namespace
{
    // Список ровно тот, что выдала сверка -run=CompendiumAudit как «нет
    // строки». Сознательно НЕ включены:
    //   - Жердяи, Курганники, Курганные огни -- расхождение РАНГА, строки
    //     давно есть в Основном (карточка говорит «Низший»); заготовка
    //     сделала бы дубль;
    //   - Полудница -- реализована глобальным атмосферным нуджем полудня
    //     (§15.2, PoludnitsaDistortionRate), а не строкой реестра; строка
    //     задвоила бы механику;
    //   - «Опасная нечисть» (6 карточек) -- §16.5, глобальный ночной нудж.
    struct FStub
    {
        const TCHAR* EntityID;
        EBiomeType Biome;
        float MorokAffinity;   // из карточки, идёт в порог Низшего ранга
    };

    const FStub AmbientStubs[] = {
        { TEXT("Лесавки"),      EBiomeType::Taiga,  0.3f },
        { TEXT("Степные духи"), EBiomeType::Steppe, 0.4f },
    };

    const FStub LandmarkStubs[] = {
        { TEXT("Болотник"),   EBiomeType::Bog,         0.8f },
        { TEXT("Дух Предка"), EBiomeType::ForestSteppe, 0.4f },
    };

    int32 NextSortOrder(const UDataTable* Table)
    {
        int32 Max = -1;
        for (const TPair<FName, uint8*>& Pair : Table->GetRowMap())
        {
            // Читаем SortOrder «вслепую» нельзя -- у двух таблиц разные
            // структуры, поэтому просто продолжаем нумерацию от размера.
            (void)Pair;
            ++Max;
        }
        return Max + 1;
    }

    bool SaveTable(UDataTable* Table)
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

int32 UBestiaryStubsAppendCommandlet::Main(const FString& Params)
{
    UDataTable* AmbientTable = LoadObject<UDataTable>(nullptr, TEXT("/Game/Herbalist/Data/DT_AmbientEntities"));
    UDataTable* LandmarkTable = LoadObject<UDataTable>(nullptr, TEXT("/Game/Herbalist/Data/DT_Landmarks"));
    if (!AmbientTable || !LandmarkTable)
    {
        UE_LOG(LogTemp, Error, TEXT("BestiaryStubsAppend: таблицы бестиария не загрузились"));
        return 1;
    }

    int32 AddedAmbient = 0, AddedLandmark = 0, Skipped = 0;

    int32 AmbientOrder = NextSortOrder(AmbientTable);
    for (const FStub& Stub : AmbientStubs)
    {
        const FName Id(Stub.EntityID);
        if (AmbientTable->GetRowMap().Contains(Id)) { ++Skipped; continue; }

        FAmbientEntityDefinition Row;
        Row.EntityID = Id;
        Row.Biome = Stub.Biome;
        Row.SortOrder = AmbientOrder++;

        // Гейт обязателен: карточка без единого условия отбрасывается
        // загрузчиком реестра как «заявляла бы клетку безусловно каждый
        // тик». Порог -- из morok_affinity карточки, заведомо временный.
        Row.TriggerAxis = EAmbientTriggerAxis::Distortion;
        Row.bTriggerAbove = true;
        Row.TriggerThreshold = Stub.MorokAffinity;

        // Ставки НУЛЕВЫЕ -- проявляется, мир не меняет. См. шапку заголовка.

        AmbientTable->AddRow(Id, Row);
        ++AddedAmbient;
        UE_LOG(LogTemp, Display, TEXT("  + Низший: %s (порог Distortion > %.2f, эффект пока нулевой)"), Stub.EntityID, Stub.MorokAffinity);
    }

    int32 LandmarkOrder = NextSortOrder(LandmarkTable);
    for (const FStub& Stub : LandmarkStubs)
    {
        const FName Id(Stub.EntityID);
        if (LandmarkTable->GetRowMap().Contains(Id)) { ++Skipped; continue; }

        FLandmarkDefinition Row;
        Row.EntityID = Id;
        Row.Biome = Stub.Biome;
        Row.SortOrder = LandmarkOrder++;
        // Осей благословения/проклятия карточка не задаёт -- оставляем None
        // с нулевыми ставками: хозяин места появится и займёт свою клетку,
        // но Respect ещё ни на что не влияет, пока числа не проставлены.

        LandmarkTable->AddRow(Id, Row);
        ++AddedLandmark;
        UE_LOG(LogTemp, Display, TEXT("  + Основной: %s (оси эффекта не заданы)"), Stub.EntityID);
    }

    if (AddedAmbient > 0 && !SaveTable(AmbientTable))
    {
        UE_LOG(LogTemp, Error, TEXT("BestiaryStubsAppend: не удалось сохранить DT_AmbientEntities"));
        return 1;
    }
    if (AddedLandmark > 0 && !SaveTable(LandmarkTable))
    {
        UE_LOG(LogTemp, Error, TEXT("BestiaryStubsAppend: не удалось сохранить DT_Landmarks"));
        return 1;
    }

    UE_LOG(LogTemp, Display, TEXT("BestiaryStubsAppend: добавлено Низших %d, Основных %d, пропущено (уже есть) %d"),
        AddedAmbient, AddedLandmark, Skipped);
    UE_LOG(LogTemp, Display, TEXT("Тюнинг (пороги, ставки, оси) проставляется руками -- парсер их не выдумывает."));
    return 0;
}
