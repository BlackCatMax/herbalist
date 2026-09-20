// AmbientTimeGatesPatchCommandlet.cpp
#include "Commandlets/AmbientTimeGatesPatchCommandlet.h"
#include "Core/Entities/AmbientEntityTypes.h"
#include "Core/Types/HerbalistCoreTypes.h"
#include "Engine/DataTable.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonReader.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/PackageName.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

namespace
{
    // Опечатка в названии сезона/погоды даёт громкую ошибку, а не тихий ноль
    // (тот же приём, что у IngredientHarvestWindowPatch).
    template<typename TEnum>
    bool StringToEnum(const FString& Value, TEnum& OutValue)
    {
        const UEnum* Enum = StaticEnum<TEnum>();
        const int64 Index = Enum->GetValueByNameString(Value);
        if (Index == INDEX_NONE) return false;
        OutValue = static_cast<TEnum>(Index);
        return true;
    }
}

int32 UAmbientTimeGatesPatchCommandlet::Main(const FString& Params)
{
    const FString PatchPath = FPaths::ConvertRelativePathToFull(
        FPaths::Combine(FPaths::ProjectDir(), TEXT(".."), TEXT("herbalist_docs"), TEXT("CSV_tabs"), TEXT("ambient_time_gates.json")));

    FString PatchJsonText;
    if (!FFileHelper::LoadFileToString(PatchJsonText, *PatchPath))
    {
        UE_LOG(LogTemp, Error, TEXT("AmbientTimeGatesPatch: не удалось прочитать %s"), *PatchPath);
        return 1;
    }

    TArray<TSharedPtr<FJsonValue>> PatchRows;
    TSharedRef<TJsonReader<TCHAR>> PatchReader = TJsonReaderFactory<TCHAR>::Create(PatchJsonText);
    if (!FJsonSerializer::Deserialize(PatchReader, PatchRows))
    {
        UE_LOG(LogTemp, Error, TEXT("AmbientTimeGatesPatch: не удалось разобрать JSON %s"), *PatchPath);
        return 1;
    }

    const TCHAR* AssetPath = TEXT("/Game/Herbalist/Data/DT_AmbientEntities");
    UDataTable* Table = LoadObject<UDataTable>(nullptr, AssetPath);
    if (!Table)
    {
        UE_LOG(LogTemp, Error, TEXT("AmbientTimeGatesPatch: не удалось загрузить %s"), AssetPath);
        return 1;
    }

    int32 PatchedCount = 0;
    for (const TSharedPtr<FJsonValue>& Value : PatchRows)
    {
        const TSharedPtr<FJsonObject> Obj = Value->AsObject();
        if (!Obj.IsValid()) continue;

        const FString RowName = Obj->GetStringField(TEXT("Name"));
        FAmbientEntityDefinition* Row = Table->FindRow<FAmbientEntityDefinition>(
            FName(*RowName), TEXT("AmbientTimeGatesPatch"), /*bWarnIfRowMissing=*/false);
        if (!Row)
        {
            UE_LOG(LogTemp, Error, TEXT("AmbientTimeGatesPatch: ряд '%s' из патча не найден в живой таблице"), *RowName);
            return 1;
        }

        bool bFlag = false;
        if (Obj->TryGetBoolField(TEXT("bRequiresNight"), bFlag)) Row->bRequiresNight = bFlag;
        if (Obj->TryGetBoolField(TEXT("bRequiresDusk"), bFlag))  Row->bRequiresDusk = bFlag;
        if (Obj->TryGetBoolField(TEXT("bRequiresSeason"), bFlag)) Row->bRequiresSeason = bFlag;
        if (Obj->TryGetBoolField(TEXT("bRequiresWeather"), bFlag)) Row->bRequiresWeather = bFlag;

        FString EnumStr;
        if (Obj->TryGetStringField(TEXT("RequiredSeason"), EnumStr))
        {
            ESeason Season;
            if (!StringToEnum(EnumStr, Season))
            {
                UE_LOG(LogTemp, Error, TEXT("AmbientTimeGatesPatch: ряд '%s' -- неизвестный сезон '%s'"), *RowName, *EnumStr);
                return 1;
            }
            Row->RequiredSeason = Season;
        }
        if (Obj->TryGetStringField(TEXT("RequiredWeather"), EnumStr))
        {
            EWeatherCondition Weather;
            if (!StringToEnum(EnumStr, Weather))
            {
                UE_LOG(LogTemp, Error, TEXT("AmbientTimeGatesPatch: ряд '%s' -- неизвестная погода '%s'"), *RowName, *EnumStr);
                return 1;
            }
            Row->RequiredWeather = Weather;
        }

        ++PatchedCount;
    }

    Table->MarkPackageDirty();

    UPackage* Package = Table->GetOutermost();
    const FString PackageFileName = FPackageName::LongPackageNameToFilename(
        Package->GetName(), FPackageName::GetAssetPackageExtension());

    FSavePackageArgs SaveArgs;
    SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
    SaveArgs.SaveFlags = SAVE_NoError;

    if (!UPackage::SavePackage(Package, Table, *PackageFileName, SaveArgs))
    {
        UE_LOG(LogTemp, Error, TEXT("AmbientTimeGatesPatch: не удалось сохранить пакет %s"), *PackageFileName);
        return 1;
    }

    UE_LOG(LogTemp, Display, TEXT("AmbientTimeGatesPatch: %s -- время дано %d карточкам, остальные %d без изменений"),
        AssetPath, PatchedCount, Table->GetRowMap().Num() - PatchedCount);
    return 0;
}
