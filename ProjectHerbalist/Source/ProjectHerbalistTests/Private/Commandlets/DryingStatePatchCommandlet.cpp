// DryingStatePatchCommandlet.cpp
#include "Commandlets/DryingStatePatchCommandlet.h"
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
    bool ReadMetaField(const TSharedPtr<FJsonObject>& DeltaObj, const TCHAR* FieldName, float& OutValue)
    {
        double Value = 0.0;
        if (!DeltaObj->TryGetNumberField(FieldName, Value))
        {
            return false;
        }
        OutValue = (float)Value;
        return true;
    }
}

int32 UDryingStatePatchCommandlet::Main(const FString& Params)
{
    const FString PatchPath = FPaths::ConvertRelativePathToFull(
        FPaths::Combine(FPaths::ProjectDir(), TEXT(".."), TEXT("herbalist_docs"), TEXT("CSV_tabs"), TEXT("ingredient_drying_state_patch.json")));

    FString PatchJsonText;
    if (!FFileHelper::LoadFileToString(PatchJsonText, *PatchPath))
    {
        UE_LOG(LogTemp, Error, TEXT("DryingStatePatch: не удалось прочитать %s"), *PatchPath);
        return 1;
    }

    TArray<TSharedPtr<FJsonValue>> PatchRows;
    TSharedRef<TJsonReader<TCHAR>> PatchReader = TJsonReaderFactory<TCHAR>::Create(PatchJsonText);
    if (!FJsonSerializer::Deserialize(PatchReader, PatchRows))
    {
        UE_LOG(LogTemp, Error, TEXT("DryingStatePatch: не удалось разобрать JSON %s"), *PatchPath);
        return 1;
    }

    const TCHAR* AssetPath = TEXT("/Game/Herbalist/Data/DT_IngredientClass");
    UDataTable* Table = LoadObject<UDataTable>(nullptr, AssetPath);
    if (!Table)
    {
        UE_LOG(LogTemp, Error, TEXT("DryingStatePatch: не удалось загрузить %s"), AssetPath);
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
            FName(*RowName), TEXT("DryingStatePatch"), /*bWarnIfRowMissing=*/false);
        if (!Row)
        {
            UE_LOG(LogTemp, Error, TEXT("DryingStatePatch: ряд '%s' из патча не найден в живой таблице"), *RowName);
            return 1;
        }

        const TSharedPtr<FJsonObject>* DeltaObjPtr = nullptr;
        if (!Obj->TryGetObjectField(TEXT("DriedStateDelta"), DeltaObjPtr) || !DeltaObjPtr || !DeltaObjPtr->IsValid())
        {
            UE_LOG(LogTemp, Error, TEXT("DryingStatePatch: ряд '%s' -- отсутствует DriedStateDelta"), *RowName);
            return 1;
        }
        const TSharedPtr<FJsonObject> DeltaObj = *DeltaObjPtr;

        FMeta NewDelta;
        bool bOk = true;
        bOk &= ReadMetaField(DeltaObj, TEXT("Distortion"), NewDelta.Distortion);
        bOk &= ReadMetaField(DeltaObj, TEXT("Stability"),  NewDelta.Stability);
        bOk &= ReadMetaField(DeltaObj, TEXT("Purity"),     NewDelta.Purity);
        bOk &= ReadMetaField(DeltaObj, TEXT("Potency"),    NewDelta.Potency);
        bOk &= ReadMetaField(DeltaObj, TEXT("Resonance"),  NewDelta.Resonance);
        bOk &= ReadMetaField(DeltaObj, TEXT("Corruption"), NewDelta.Corruption);
        if (!bOk)
        {
            UE_LOG(LogTemp, Error, TEXT("DryingStatePatch: ряд '%s' -- DriedStateDelta с недостающим полем"), *RowName);
            return 1;
        }

        Row->DriedStateDelta = NewDelta;
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
        UE_LOG(LogTemp, Error, TEXT("DryingStatePatch: не удалось сохранить пакет %s"), *PackageFileName);
        return 1;
    }

    UE_LOG(LogTemp, Display, TEXT("DryingStatePatch: %s -- пропатчено %d рядов, остальные %d не тронуты байтово"),
        AssetPath, PatchedCount, Table->GetRowMap().Num() - PatchedCount);
    return 0;
}
