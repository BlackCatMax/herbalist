// IngredientHarvestWindowPatchCommandlet.cpp
#include "Commandlets/IngredientHarvestWindowPatchCommandlet.h"
#include "Core/Data/IngredientTableRow.h"
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
    // Общий разбор enum-строки из патча в любой UENUM (Season/HarvestTimeWindow/
    // MoonPhase здесь) -- StaticEnum<T>()->GetValueByNameString возвращает
    // INDEX_NONE на опечатку, что и даёт громкую ошибку вместо тихого 0.
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

int32 UIngredientHarvestWindowPatchCommandlet::Main(const FString& Params)
{
    const FString PatchPath = FPaths::ConvertRelativePathToFull(
        FPaths::Combine(FPaths::ProjectDir(), TEXT(".."), TEXT("herbalist_docs"), TEXT("CSV_tabs"), TEXT("ingredient_harvest_windows.json")));

    FString PatchJsonText;
    if (!FFileHelper::LoadFileToString(PatchJsonText, *PatchPath))
    {
        UE_LOG(LogTemp, Error, TEXT("IngredientHarvestWindowPatch: не удалось прочитать %s"), *PatchPath);
        return 1;
    }

    TArray<TSharedPtr<FJsonValue>> PatchRows;
    TSharedRef<TJsonReader<TCHAR>> PatchReader = TJsonReaderFactory<TCHAR>::Create(PatchJsonText);
    if (!FJsonSerializer::Deserialize(PatchReader, PatchRows))
    {
        UE_LOG(LogTemp, Error, TEXT("IngredientHarvestWindowPatch: не удалось разобрать JSON %s"), *PatchPath);
        return 1;
    }

    const TCHAR* AssetPath = TEXT("/Game/Herbalist/Data/DT_IngredientClass");
    UDataTable* Table = LoadObject<UDataTable>(nullptr, AssetPath);
    if (!Table)
    {
        UE_LOG(LogTemp, Error, TEXT("IngredientHarvestWindowPatch: не удалось загрузить %s"), AssetPath);
        return 1;
    }

    // Точечная правка через FindRow, БЕЗ прохода Table->GetTableAsJSON()/
    // CreateTableFromJSONString() по всей таблице (2026-09-04, тот же
    // реальный баг, что и в IngredientGatheringAndGardenPatchCommandlet:
    // полный JSON-роундтрип молча терял ряды с пробелом в имени --
    // "Молодильное яблоко" и все 4 "Перо *" -- ни разу не упомянутые в
    // патче, ломались просто ФАКТОМ прогона командлета по таблице, где они
    // уже есть. FindRow трогает РОВНО ряды, названные в патче).
    int32 PatchedCount = 0;
    for (const TSharedPtr<FJsonValue>& Value : PatchRows)
    {
        const TSharedPtr<FJsonObject> Obj = Value->AsObject();
        if (!Obj.IsValid()) continue;

        const FString RowName = Obj->GetStringField(TEXT("Name"));
        FIngredientTableRow* Row = Table->FindRow<FIngredientTableRow>(
            FName(*RowName), TEXT("IngredientHarvestWindowPatch"), /*bWarnIfRowMissing=*/false);
        if (!Row)
        {
            UE_LOG(LogTemp, Error, TEXT("IngredientHarvestWindowPatch: ряд '%s' из патча не найден в живой таблице"), *RowName);
            return 1;
        }

        bool bBoolValue = false;
        if (Obj->TryGetBoolField(TEXT("bAutumnOnly"), bBoolValue)) Row->bAutumnOnly = bBoolValue;
        if (Obj->TryGetBoolField(TEXT("bRequiresMoonPhase"), bBoolValue)) Row->bRequiresMoonPhase = bBoolValue;
        if (Obj->TryGetBoolField(TEXT("bRequiresDryWeather"), bBoolValue)) Row->bRequiresDryWeather = bBoolValue;

        FString EnumStr;
        if (Obj->TryGetStringField(TEXT("HarvestTimeWindow"), EnumStr))
        {
            EHarvestTimeWindow Window;
            if (!StringToEnum(EnumStr, Window))
            {
                UE_LOG(LogTemp, Error, TEXT("IngredientHarvestWindowPatch: ряд '%s' -- неизвестное значение HarvestTimeWindow '%s'"), *RowName, *EnumStr);
                return 1;
            }
            Row->HarvestTimeWindow = Window;
        }
        if (Obj->TryGetStringField(TEXT("RequiredMoonPhase"), EnumStr))
        {
            EMoonPhase Phase;
            if (!StringToEnum(EnumStr, Phase))
            {
                UE_LOG(LogTemp, Error, TEXT("IngredientHarvestWindowPatch: ряд '%s' -- неизвестное значение RequiredMoonPhase '%s'"), *RowName, *EnumStr);
                return 1;
            }
            Row->RequiredMoonPhase = Phase;
        }

        const TArray<TSharedPtr<FJsonValue>>* SeasonsArray = nullptr;
        if (Obj->TryGetArrayField(TEXT("AllowedSeasons"), SeasonsArray))
        {
            TArray<ESeason> Seasons;
            for (const TSharedPtr<FJsonValue>& SeasonValue : *SeasonsArray)
            {
                ESeason Season;
                const FString SeasonStr = SeasonValue->AsString();
                if (!StringToEnum(SeasonStr, Season))
                {
                    UE_LOG(LogTemp, Error, TEXT("IngredientHarvestWindowPatch: ряд '%s' -- неизвестное значение AllowedSeasons '%s'"), *RowName, *SeasonStr);
                    return 1;
                }
                Seasons.Add(Season);
            }
            Row->AllowedSeasons = Seasons;
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

    const bool bSuccess = UPackage::SavePackage(Package, Table, *PackageFileName, SaveArgs);
    if (!bSuccess)
    {
        UE_LOG(LogTemp, Error, TEXT("IngredientHarvestWindowPatch: не удалось сохранить пакет %s"), *PackageFileName);
        return 1;
    }

    UE_LOG(LogTemp, Display, TEXT("IngredientHarvestWindowPatch: %s -- пропатчено %d рядов, остальные %d не тронуты байтово"),
        AssetPath, PatchedCount, Table->GetRowMap().Num() - PatchedCount);
    return 0;
}
