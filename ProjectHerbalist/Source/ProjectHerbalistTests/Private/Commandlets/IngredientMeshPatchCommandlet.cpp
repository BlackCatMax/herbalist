// IngredientMeshPatchCommandlet.cpp
#include "Commandlets/IngredientMeshPatchCommandlet.h"
#include "Core/Data/IngredientTableRow.h"
#include "Engine/DataTable.h"
#include "Engine/StaticMesh.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonReader.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/PackageName.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

int32 UIngredientMeshPatchCommandlet::Main(const FString& Params)
{
    const FString PatchPath = FPaths::ConvertRelativePathToFull(
        FPaths::Combine(FPaths::ProjectDir(), TEXT(".."), TEXT("herbalist_docs"), TEXT("CSV_tabs"), TEXT("ingredient_mesh_patch.json")));

    FString PatchJsonText;
    if (!FFileHelper::LoadFileToString(PatchJsonText, *PatchPath))
    {
        UE_LOG(LogTemp, Error, TEXT("IngredientMeshPatch: не удалось прочитать %s"), *PatchPath);
        return 1;
    }

    TArray<TSharedPtr<FJsonValue>> PatchRows;
    TSharedRef<TJsonReader<TCHAR>> PatchReader = TJsonReaderFactory<TCHAR>::Create(PatchJsonText);
    if (!FJsonSerializer::Deserialize(PatchReader, PatchRows))
    {
        UE_LOG(LogTemp, Error, TEXT("IngredientMeshPatch: не удалось разобрать JSON %s"), *PatchPath);
        return 1;
    }

    const TCHAR* AssetPath = TEXT("/Game/Herbalist/Data/DT_IngredientClass");
    UDataTable* Table = LoadObject<UDataTable>(nullptr, AssetPath);
    if (!Table)
    {
        UE_LOG(LogTemp, Error, TEXT("IngredientMeshPatch: не удалось загрузить %s"), AssetPath);
        return 1;
    }

    // Сначала всё проверить, потом менять: битая строка патча не должна
    // оставить таблицу наполовину пропатченной.
    TArray<TPair<FIngredientTableRow*, UStaticMesh*>> Planned;
    for (const TSharedPtr<FJsonValue>& Value : PatchRows)
    {
        const TSharedPtr<FJsonObject> Obj = Value->AsObject();
        if (!Obj.IsValid()) continue;

        const FString RowName = Obj->GetStringField(TEXT("Name"));
        FIngredientTableRow* Row = Table->FindRow<FIngredientTableRow>(
            FName(*RowName), TEXT("IngredientMeshPatch"), /*bWarnIfRowMissing=*/false);
        if (!Row)
        {
            UE_LOG(LogTemp, Error, TEXT("IngredientMeshPatch: ряд '%s' из патча не найден в живой таблице"), *RowName);
            return 1;
        }

        FString MeshPath;
        if (!Obj->TryGetStringField(TEXT("ResourceMesh"), MeshPath))
        {
            UE_LOG(LogTemp, Error, TEXT("IngredientMeshPatch: ряд '%s' -- отсутствует ResourceMesh"), *RowName);
            return 1;
        }
        UStaticMesh* Mesh = nullptr;
        if (!MeshPath.IsEmpty())
        {
            Mesh = LoadObject<UStaticMesh>(nullptr, *MeshPath);
            if (!Mesh)
            {
                UE_LOG(LogTemp, Error, TEXT("IngredientMeshPatch: ряд '%s' -- меша %s нет"), *RowName, *MeshPath);
                return 1;
            }
        }
        Planned.Add({ Row, Mesh });
    }

    for (const TPair<FIngredientTableRow*, UStaticMesh*>& Change : Planned)
    {
        Change.Key->ResourceMesh = Change.Value;
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
        UE_LOG(LogTemp, Error, TEXT("IngredientMeshPatch: не удалось сохранить пакет %s"), *PackageFileName);
        return 1;
    }

    UE_LOG(LogTemp, Display, TEXT("IngredientMeshPatch: %s -- пропатчено %d рядов, остальные %d не тронуты"),
        AssetPath, Planned.Num(), Table->GetRowMap().Num() - Planned.Num());
    return 0;
}
