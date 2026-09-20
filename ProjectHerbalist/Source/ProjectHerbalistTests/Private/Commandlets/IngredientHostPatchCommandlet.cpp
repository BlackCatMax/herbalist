// IngredientHostPatchCommandlet.cpp
#include "Commandlets/IngredientHostPatchCommandlet.h"
#include "Core/Data/IngredientTableRow.h"
#include "Core/Entities/LandmarkTypes.h"
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

int32 UIngredientHostPatchCommandlet::Main(const FString& Params)
{
    const FString PatchPath = FPaths::ConvertRelativePathToFull(
        FPaths::Combine(FPaths::ProjectDir(), TEXT(".."), TEXT("herbalist_docs"), TEXT("CSV_tabs"), TEXT("ingredient_hosts.json")));

    FString PatchJsonText;
    if (!FFileHelper::LoadFileToString(PatchJsonText, *PatchPath))
    {
        UE_LOG(LogTemp, Error, TEXT("IngredientHostPatch: не удалось прочитать %s"), *PatchPath);
        return 1;
    }

    TArray<TSharedPtr<FJsonValue>> PatchRows;
    TSharedRef<TJsonReader<TCHAR>> PatchReader = TJsonReaderFactory<TCHAR>::Create(PatchJsonText);
    if (!FJsonSerializer::Deserialize(PatchReader, PatchRows))
    {
        UE_LOG(LogTemp, Error, TEXT("IngredientHostPatch: не удалось разобрать JSON %s"), *PatchPath);
        return 1;
    }

    const TCHAR* AssetPath = TEXT("/Game/Herbalist/Data/DT_IngredientClass");
    UDataTable* Table = LoadObject<UDataTable>(nullptr, AssetPath);
    if (!Table)
    {
        UE_LOG(LogTemp, Error, TEXT("IngredientHostPatch: не удалось загрузить %s"), AssetPath);
        return 1;
    }

    int32 PatchedCount = 0;
    for (const TSharedPtr<FJsonValue>& Value : PatchRows)
    {
        const TSharedPtr<FJsonObject> Obj = Value->AsObject();
        if (!Obj.IsValid()) continue;

        const FString RowName = Obj->GetStringField(TEXT("Name"));
        const FName HostID(*Obj->GetStringField(TEXT("HostEntityID")));
        // Хозяин -- только из реестра Основных: опечатка в имени молча
        // выключила бы множитель для травы.
        if (!FindLandmarkDefinition(HostID))
        {
            UE_LOG(LogTemp, Error, TEXT("IngredientHostPatch: ряд '%s' -- хозяина '%s' нет в DT_Landmarks"), *RowName, *HostID.ToString());
            return 1;
        }
        FIngredientTableRow* Row = Table->FindRow<FIngredientTableRow>(FName(*RowName), TEXT("IngredientHostPatch"), /*bWarnIfRowMissing=*/false);
        if (!Row)
        {
            UE_LOG(LogTemp, Error, TEXT("IngredientHostPatch: ряд '%s' из патча не найден в живой таблице"), *RowName);
            return 1;
        }
        Row->HostEntityID = HostID;
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
        UE_LOG(LogTemp, Error, TEXT("IngredientHostPatch: не удалось сохранить пакет %s"), *PackageFileName);
        return 1;
    }

    UE_LOG(LogTemp, Display, TEXT("IngredientHostPatch: %s -- хозяин проставлен %d рядам"), AssetPath, PatchedCount);
    return 0;
}
