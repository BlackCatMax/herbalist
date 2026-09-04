// IngredientBiomeRangePatchCommandlet.cpp
#include "Commandlets/IngredientBiomeRangePatchCommandlet.h"
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
    // Ручное сопоставление строка -> EBiomeType, тот же приём, что
    // StringToGardenNiche в IngredientGatheringAndGardenPatchCommandlet.cpp:
    // UEnum::GetValueByNameString молча даёт 0 на опечатке, а патч с
    // опечаткой в имени биома должен падать громко, не тихо ставить Tundra.
    bool StringToBiomeType(const FString& Value, EBiomeType& OutBiome)
    {
        if (Value == TEXT("Tundra"))          { OutBiome = EBiomeType::Tundra;          return true; }
        if (Value == TEXT("Taiga"))           { OutBiome = EBiomeType::Taiga;           return true; }
        if (Value == TEXT("MixedForest"))     { OutBiome = EBiomeType::MixedForest;     return true; }
        if (Value == TEXT("BroadleafForest")) { OutBiome = EBiomeType::BroadleafForest; return true; }
        if (Value == TEXT("ForestSteppe"))    { OutBiome = EBiomeType::ForestSteppe;    return true; }
        if (Value == TEXT("Steppe"))          { OutBiome = EBiomeType::Steppe;          return true; }
        if (Value == TEXT("Floodplain"))      { OutBiome = EBiomeType::Floodplain;      return true; }
        if (Value == TEXT("Bog"))             { OutBiome = EBiomeType::Bog;             return true; }
        return false;
    }
}

int32 UIngredientBiomeRangePatchCommandlet::Main(const FString& Params)
{
    const FString PatchPath = FPaths::ConvertRelativePathToFull(
        FPaths::Combine(FPaths::ProjectDir(), TEXT(".."), TEXT("herbalist_docs"), TEXT("CSV_tabs"), TEXT("ingredient_biome_range_patch.json")));

    FString PatchJsonText;
    if (!FFileHelper::LoadFileToString(PatchJsonText, *PatchPath))
    {
        UE_LOG(LogTemp, Error, TEXT("IngredientBiomeRangePatch: не удалось прочитать %s"), *PatchPath);
        return 1;
    }

    TArray<TSharedPtr<FJsonValue>> PatchRows;
    TSharedRef<TJsonReader<TCHAR>> PatchReader = TJsonReaderFactory<TCHAR>::Create(PatchJsonText);
    if (!FJsonSerializer::Deserialize(PatchReader, PatchRows))
    {
        UE_LOG(LogTemp, Error, TEXT("IngredientBiomeRangePatch: не удалось разобрать JSON %s"), *PatchPath);
        return 1;
    }

    const TCHAR* AssetPath = TEXT("/Game/Herbalist/Data/DT_IngredientClass");
    UDataTable* Table = LoadObject<UDataTable>(nullptr, AssetPath);
    if (!Table)
    {
        UE_LOG(LogTemp, Error, TEXT("IngredientBiomeRangePatch: не удалось загрузить %s"), AssetPath);
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
            FName(*RowName), TEXT("IngredientBiomeRangePatch"), /*bWarnIfRowMissing=*/false);
        if (!Row)
        {
            UE_LOG(LogTemp, Error, TEXT("IngredientBiomeRangePatch: ряд '%s' из патча не найден в живой таблице"), *RowName);
            return 1;
        }

        const TArray<TSharedPtr<FJsonValue>>* BiomeArray = nullptr;
        if (!Obj->TryGetArrayField(TEXT("AllowedBiomes"), BiomeArray) || !BiomeArray || BiomeArray->Num() == 0)
        {
            UE_LOG(LogTemp, Error, TEXT("IngredientBiomeRangePatch: ряд '%s' -- отсутствует или пуст AllowedBiomes"), *RowName);
            return 1;
        }

        TArray<EBiomeType> NewBiomes;
        NewBiomes.Reserve(BiomeArray->Num());
        for (const TSharedPtr<FJsonValue>& BiomeValue : *BiomeArray)
        {
            FString BiomeStr;
            EBiomeType Biome;
            if (!BiomeValue->TryGetString(BiomeStr) || !StringToBiomeType(BiomeStr, Biome))
            {
                UE_LOG(LogTemp, Error, TEXT("IngredientBiomeRangePatch: ряд '%s' -- неизвестное значение биома '%s'"), *RowName, *BiomeStr);
                return 1;
            }
            NewBiomes.AddUnique(Biome);
        }

        Row->AllowedBiomes = NewBiomes;
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
        UE_LOG(LogTemp, Error, TEXT("IngredientBiomeRangePatch: не удалось сохранить пакет %s"), *PackageFileName);
        return 1;
    }

    UE_LOG(LogTemp, Display, TEXT("IngredientBiomeRangePatch: %s -- пропатчено %d рядов, остальные %d не тронуты байтово"),
        AssetPath, PatchedCount, Table->GetRowMap().Num() - PatchedCount);
    return 0;
}
