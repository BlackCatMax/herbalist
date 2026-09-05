// IngredientDryingDurationPatchCommandlet.cpp
#include "Commandlets/IngredientDryingDurationPatchCommandlet.h"
#include "Core/Data/IngredientTableRow.h"
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

int32 UIngredientDryingDurationPatchCommandlet::Main(const FString& Params)
{
    const FString PatchPath = FPaths::ConvertRelativePathToFull(
        FPaths::Combine(FPaths::ProjectDir(), TEXT(".."), TEXT("herbalist_docs"), TEXT("CSV_tabs"), TEXT("ingredient_drying_duration_patch.json")));

    FString PatchJsonText;
    if (!FFileHelper::LoadFileToString(PatchJsonText, *PatchPath))
    {
        UE_LOG(LogTemp, Error, TEXT("IngredientDryingDurationPatch: не удалось прочитать %s"), *PatchPath);
        return 1;
    }

    TArray<TSharedPtr<FJsonValue>> PatchRows;
    TSharedRef<TJsonReader<TCHAR>> PatchReader = TJsonReaderFactory<TCHAR>::Create(PatchJsonText);
    if (!FJsonSerializer::Deserialize(PatchReader, PatchRows))
    {
        UE_LOG(LogTemp, Error, TEXT("IngredientDryingDurationPatch: не удалось разобрать JSON %s"), *PatchPath);
        return 1;
    }

    const TCHAR* AssetPath = TEXT("/Game/Herbalist/Data/DT_IngredientClass");
    UDataTable* Table = LoadObject<UDataTable>(nullptr, AssetPath);
    if (!Table)
    {
        UE_LOG(LogTemp, Error, TEXT("IngredientDryingDurationPatch: не удалось загрузить %s"), AssetPath);
        return 1;
    }

    // Точечная правка через FindRow, БЕЗ GetTableAsJSON()/CreateTableFromJSON
    // String() по всей таблице -- см. довод в .h у этого командлета.
    int32 PatchedCount = 0;
    for (const TSharedPtr<FJsonValue>& Value : PatchRows)
    {
        const TSharedPtr<FJsonObject> Obj = Value->AsObject();
        if (!Obj.IsValid()) continue;

        const FString RowName = Obj->GetStringField(TEXT("Name"));
        FIngredientTableRow* Row = Table->FindRow<FIngredientTableRow>(
            FName(*RowName), TEXT("IngredientDryingDurationPatch"), /*bWarnIfRowMissing=*/false);
        if (!Row)
        {
            UE_LOG(LogTemp, Error, TEXT("IngredientDryingDurationPatch: ряд '%s' из патча не найден в живой таблице"), *RowName);
            return 1;
        }

        double DurationSeconds = 0.0;
        if (!Obj->TryGetNumberField(TEXT("DryingDurationSeconds"), DurationSeconds))
        {
            UE_LOG(LogTemp, Error, TEXT("IngredientDryingDurationPatch: ряд '%s' -- отсутствует DryingDurationSeconds"), *RowName);
            return 1;
        }

        Row->DryingDurationSeconds = (float)DurationSeconds;
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
        UE_LOG(LogTemp, Error, TEXT("IngredientDryingDurationPatch: не удалось сохранить пакет %s"), *PackageFileName);
        return 1;
    }

    UE_LOG(LogTemp, Display, TEXT("IngredientDryingDurationPatch: %s -- пропатчено %d рядов, остальные %d не тронуты байтово"),
        AssetPath, PatchedCount, Table->GetRowMap().Num() - PatchedCount);
    return 0;
}
