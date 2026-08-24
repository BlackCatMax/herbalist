// IngredientAppendCommandlet.cpp
#include "Commandlets/IngredientAppendCommandlet.h"
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

int32 UIngredientAppendCommandlet::Main(const FString& Params)
{
    TArray<FString> Tokens, Switches;
    TMap<FString, FString> ParamsMap;
    ParseCommandLine(*Params, Tokens, Switches, ParamsMap);

    const FString* NamesArg = ParamsMap.Find(TEXT("Names"));
    if (!NamesArg || NamesArg->IsEmpty())
    {
        UE_LOG(LogTemp, Error, TEXT("IngredientAppend: укажите -Names=id1,id2,... — только эти ряды будут добавлены к живой таблице"));
        return 1;
    }
    TArray<FString> NamesToAdd;
    NamesArg->ParseIntoArray(NamesToAdd, TEXT(","), true);

    const FString InPath = FPaths::ConvertRelativePathToFull(
        FPaths::Combine(FPaths::ProjectDir(), TEXT(".."), TEXT("herbalist_docs"), TEXT("CSV_tabs"), TEXT("ingredients.json")));

    FString SourceJsonText;
    if (!FFileHelper::LoadFileToString(SourceJsonText, *InPath))
    {
        UE_LOG(LogTemp, Error, TEXT("IngredientAppend: не удалось прочитать %s"), *InPath);
        return 1;
    }

    TArray<TSharedPtr<FJsonValue>> SourceRows;
    TSharedRef<TJsonReader<TCHAR>> SourceReader = TJsonReaderFactory<TCHAR>::Create(SourceJsonText);
    if (!FJsonSerializer::Deserialize(SourceReader, SourceRows))
    {
        UE_LOG(LogTemp, Error, TEXT("IngredientAppend: не удалось разобрать JSON %s"), *InPath);
        return 1;
    }

    const TCHAR* AssetPath = TEXT("/Game/Herbalist/Data/DT_IngredientClass");
    UDataTable* Table = LoadObject<UDataTable>(nullptr, AssetPath);
    if (!Table)
    {
        UE_LOG(LogTemp, Error, TEXT("IngredientAppend: не удалось загрузить %s"), AssetPath);
        return 1;
    }
    const int32 OriginalRowCount = Table->GetRowMap().Num();

    // Текущее содержимое таблицы через штатный экспортёр — тот же формат
    // (включая NSLOCTEXT-обёртку FText), что и ручной экспорт из редактора.
    // Не пересобираем существующие 71+ рядов вручную из ingredients.json,
    // чтобы не потерять Icon/ResourceMesh, если они когда-то были
    // проставлены в редакторе, а извлекающий скрипт их не знает.
    const FString LiveJsonText = Table->GetTableAsJSON();
    TArray<TSharedPtr<FJsonValue>> MergedRows;
    TSharedRef<TJsonReader<TCHAR>> LiveReader = TJsonReaderFactory<TCHAR>::Create(LiveJsonText);
    if (!FJsonSerializer::Deserialize(LiveReader, MergedRows))
    {
        UE_LOG(LogTemp, Error, TEXT("IngredientAppend: не удалось разобрать текущий JSON таблицы"));
        return 1;
    }

    int32 AddedCount = 0;
    for (const FString& Name : NamesToAdd)
    {
        bool bFound = false;
        for (const TSharedPtr<FJsonValue>& Value : SourceRows)
        {
            const TSharedPtr<FJsonObject> Obj = Value->AsObject();
            if (Obj.IsValid() && Obj->GetStringField(TEXT("Name")) == Name)
            {
                MergedRows.Add(Value);
                ++AddedCount;
                bFound = true;
                break;
            }
        }
        if (!bFound)
        {
            UE_LOG(LogTemp, Error, TEXT("IngredientAppend: ряд '%s' не найден в %s"), *Name, *InPath);
            return 1;
        }
    }

    FString MergedJsonText;
    TSharedRef<TJsonWriter<TCHAR>> Writer = TJsonWriterFactory<TCHAR>::Create(&MergedJsonText);
    FJsonSerializer::Serialize(MergedRows, Writer);

    const TArray<FString> Problems = Table->CreateTableFromJSONString(MergedJsonText);
    for (const FString& Problem : Problems)
    {
        UE_LOG(LogTemp, Warning, TEXT("IngredientAppend: %s"), *Problem);
    }

    const int32 FinalRowCount = Table->GetRowMap().Num();
    if (FinalRowCount != OriginalRowCount + AddedCount)
    {
        UE_LOG(LogTemp, Error, TEXT("IngredientAppend: ожидалось %d рядов (было %d + добавлено %d), получилось %d — не сохраняю"),
            OriginalRowCount + AddedCount, OriginalRowCount, AddedCount, FinalRowCount);
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
        UE_LOG(LogTemp, Error, TEXT("IngredientAppend: не удалось сохранить пакет %s"), *PackageFileName);
        return 1;
    }

    UE_LOG(LogTemp, Display, TEXT("IngredientAppend: %s теперь содержит %d рядов (было %d, добавлено %d)"),
        AssetPath, FinalRowCount, OriginalRowCount, AddedCount);
    return 0;
}
