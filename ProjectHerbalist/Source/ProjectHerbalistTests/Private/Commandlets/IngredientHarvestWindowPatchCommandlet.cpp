// IngredientHarvestWindowPatchCommandlet.cpp
#include "Commandlets/IngredientHarvestWindowPatchCommandlet.h"
#include "Engine/DataTable.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonWriter.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/PackageName.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

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

    TMap<FString, TSharedPtr<FJsonObject>> PatchByName;
    for (const TSharedPtr<FJsonValue>& Value : PatchRows)
    {
        const TSharedPtr<FJsonObject> Obj = Value->AsObject();
        if (!Obj.IsValid()) continue;
        PatchByName.Add(Obj->GetStringField(TEXT("Name")), Obj);
    }

    const TCHAR* AssetPath = TEXT("/Game/Herbalist/Data/DT_IngredientClass");
    UDataTable* Table = LoadObject<UDataTable>(nullptr, AssetPath);
    if (!Table)
    {
        UE_LOG(LogTemp, Error, TEXT("IngredientHarvestWindowPatch: не удалось загрузить %s"), AssetPath);
        return 1;
    }
    const int32 OriginalRowCount = Table->GetRowMap().Num();

    const FString LiveJsonText = Table->GetTableAsJSON();
    TArray<TSharedPtr<FJsonValue>> LiveRows;
    TSharedRef<TJsonReader<TCHAR>> LiveReader = TJsonReaderFactory<TCHAR>::Create(LiveJsonText);
    if (!FJsonSerializer::Deserialize(LiveReader, LiveRows))
    {
        UE_LOG(LogTemp, Error, TEXT("IngredientHarvestWindowPatch: не удалось разобрать текущий JSON таблицы"));
        return 1;
    }

    int32 PatchedCount = 0;
    TSet<FString> AppliedNames;
    for (const TSharedPtr<FJsonValue>& Value : LiveRows)
    {
        const TSharedPtr<FJsonObject> LiveObj = Value->AsObject();
        if (!LiveObj.IsValid()) continue;

        const FString RowName = LiveObj->GetStringField(TEXT("Name"));
        const TSharedPtr<FJsonObject>* PatchObj = PatchByName.Find(RowName);
        if (!PatchObj) continue;

        // Только 5 новых ключей окна сбора -- всё остальное на живом ряду
        // (BaseState/Icon/ResourceMesh/AllowedBiomes/...) не трогаем.
        for (const auto& Field : (*PatchObj)->Values)
        {
            if (Field.Key == TEXT("Name")) continue;
            LiveObj->SetField(Field.Key, Field.Value);
        }
        ++PatchedCount;
        AppliedNames.Add(RowName);
    }

    for (const auto& Pair : PatchByName)
    {
        if (!AppliedNames.Contains(Pair.Key))
        {
            UE_LOG(LogTemp, Error, TEXT("IngredientHarvestWindowPatch: ряд '%s' из патча не найден в живой таблице"), *Pair.Key);
            return 1;
        }
    }

    FString MergedJsonText;
    TSharedRef<TJsonWriter<TCHAR>> Writer = TJsonWriterFactory<TCHAR>::Create(&MergedJsonText);
    FJsonSerializer::Serialize(LiveRows, Writer);

    const TArray<FString> Problems = Table->CreateTableFromJSONString(MergedJsonText);
    for (const FString& Problem : Problems)
    {
        UE_LOG(LogTemp, Warning, TEXT("IngredientHarvestWindowPatch: %s"), *Problem);
    }

    const int32 FinalRowCount = Table->GetRowMap().Num();
    if (FinalRowCount != OriginalRowCount)
    {
        UE_LOG(LogTemp, Error, TEXT("IngredientHarvestWindowPatch: ожидалось %d рядов (не добавляем/не удаляем), получилось %d — не сохраняю"),
            OriginalRowCount, FinalRowCount);
        return 1;
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

    UE_LOG(LogTemp, Display, TEXT("IngredientHarvestWindowPatch: %s -- пропатчено %d из %d рядов (патч содержал %d записей)"),
        AssetPath, PatchedCount, FinalRowCount, PatchByName.Num());
    return 0;
}
