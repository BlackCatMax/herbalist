// IngredientGatheringAndGardenPatchCommandlet.cpp
#include "Commandlets/IngredientGatheringAndGardenPatchCommandlet.h"
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

namespace
{
    // Единственное место, где строка JSON превращается в EGardenNiche --
    // ручное сопоставление, не UEnum::GetValueByNameString (тот принимает и
    // "None"/числовой индекс, и любой мусор молча даст 0 = None, а опечатка
    // в патче должна быть громкой ошибкой, не тихим "не поставилось").
    bool StringToGardenNiche(const FString& Value, EGardenNiche& OutNiche)
    {
        if (Value == TEXT("Mycelium"))   { OutNiche = EGardenNiche::Mycelium;   return true; }
        if (Value == TEXT("RootCellar")) { OutNiche = EGardenNiche::RootCellar; return true; }
        if (Value == TEXT("Pond"))       { OutNiche = EGardenNiche::Pond;       return true; }
        if (Value == TEXT("SunnyBed"))   { OutNiche = EGardenNiche::SunnyBed;   return true; }
        if (Value == TEXT("ShadeBed"))   { OutNiche = EGardenNiche::ShadeBed;   return true; }
        if (Value == TEXT("Cave"))       { OutNiche = EGardenNiche::Cave;       return true; }
        if (Value == TEXT("None"))       { OutNiche = EGardenNiche::None;       return true; }
        return false;
    }
}

int32 UIngredientGatheringAndGardenPatchCommandlet::Main(const FString& Params)
{
    const FString PatchPath = FPaths::ConvertRelativePathToFull(
        FPaths::Combine(FPaths::ProjectDir(), TEXT(".."), TEXT("herbalist_docs"), TEXT("CSV_tabs"), TEXT("ingredient_gathering_and_garden_flags.json")));

    FString PatchJsonText;
    if (!FFileHelper::LoadFileToString(PatchJsonText, *PatchPath))
    {
        UE_LOG(LogTemp, Error, TEXT("IngredientGatheringAndGardenPatch: не удалось прочитать %s"), *PatchPath);
        return 1;
    }

    TArray<TSharedPtr<FJsonValue>> PatchRows;
    TSharedRef<TJsonReader<TCHAR>> PatchReader = TJsonReaderFactory<TCHAR>::Create(PatchJsonText);
    if (!FJsonSerializer::Deserialize(PatchReader, PatchRows))
    {
        UE_LOG(LogTemp, Error, TEXT("IngredientGatheringAndGardenPatch: не удалось разобрать JSON %s"), *PatchPath);
        return 1;
    }

    const TCHAR* AssetPath = TEXT("/Game/Herbalist/Data/DT_IngredientClass");
    UDataTable* Table = LoadObject<UDataTable>(nullptr, AssetPath);
    if (!Table)
    {
        UE_LOG(LogTemp, Error, TEXT("IngredientGatheringAndGardenPatch: не удалось загрузить %s"), AssetPath);
        return 1;
    }

    // Точечная правка через FindRow, БЕЗ прохода Table->GetTableAsJSON()/
    // CreateTableFromJSONString() по всей таблице (2026-09-04, найденный
    // реальный баг: полный JSON-роундтрип молча терял ряды, чьё имя
    // содержит пробел -- "Молодильное яблоко" и все 4 "Перо *", ни разу не
    // упомянутые в патче вообще, ломались просто ФАКТОМ прогона командлета
    // по таблице, где они уже есть, независимо от содержимого патча.
    // FindRow/прямая правка полей трогает РОВНО те ряды, что названы в
    // патче -- остальные 89-N рядов остаются в таблице побайтово теми же,
    // какими были).
    int32 PatchedCount = 0;
    for (const TSharedPtr<FJsonValue>& Value : PatchRows)
    {
        const TSharedPtr<FJsonObject> Obj = Value->AsObject();
        if (!Obj.IsValid()) continue;

        const FString RowName = Obj->GetStringField(TEXT("Name"));
        FIngredientTableRow* Row = Table->FindRow<FIngredientTableRow>(
            FName(*RowName), TEXT("IngredientGatheringAndGardenPatch"), /*bWarnIfRowMissing=*/false);
        if (!Row)
        {
            UE_LOG(LogTemp, Error, TEXT("IngredientGatheringAndGardenPatch: ряд '%s' из патча не найден в живой таблице"), *RowName);
            return 1;
        }

        bool bBoolValue = false;
        if (Obj->TryGetBoolField(TEXT("bIronAverse"), bBoolValue)) Row->bIronAverse = bBoolValue;
        if (Obj->TryGetBoolField(TEXT("bDelicate"), bBoolValue)) Row->bDelicate = bBoolValue;

        FString NicheStr;
        if (Obj->TryGetStringField(TEXT("GardenNiche"), NicheStr))
        {
            EGardenNiche Niche;
            if (!StringToGardenNiche(NicheStr, Niche))
            {
                UE_LOG(LogTemp, Error, TEXT("IngredientGatheringAndGardenPatch: ряд '%s' -- неизвестное значение GardenNiche '%s'"), *RowName, *NicheStr);
                return 1;
            }
            Row->GardenNiche = Niche;
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
        UE_LOG(LogTemp, Error, TEXT("IngredientGatheringAndGardenPatch: не удалось сохранить пакет %s"), *PackageFileName);
        return 1;
    }

    UE_LOG(LogTemp, Display, TEXT("IngredientGatheringAndGardenPatch: %s -- пропатчено %d рядов, остальные %d не тронуты байтово"),
        AssetPath, PatchedCount, Table->GetRowMap().Num() - PatchedCount);
    return 0;
}
