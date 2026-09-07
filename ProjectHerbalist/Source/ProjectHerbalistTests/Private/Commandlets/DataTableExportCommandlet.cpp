// DataTableExportCommandlet.cpp
#include "Commandlets/DataTableExportCommandlet.h"

#include "Core/BiomeGraph/BiomeGraphAsset.h"

#include "Engine/DataTable.h"
#include "JsonObjectConverter.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/Parse.h"

namespace
{
    // Все таблицы проекта, к которым есть парный экспорт в CSV_tabs либо
    // которые вообще стоит уметь посмотреть. Список явный, не сканирование
    // папки: так видно, что именно проверяется, и добавление таблицы --
    // осознанный шаг.
    const TCHAR* TablePaths[] = {
        TEXT("/Game/Data/DT_BiomeDefaults"),
        TEXT("/Game/Herbalist/Data/DT_IngredientClass"),
        TEXT("/Game/Herbalist/Data/DT_WaterTypes"),
        TEXT("/Game/Herbalist/Data/DT_AmbientEntities"),
        TEXT("/Game/Herbalist/Data/DT_Landmarks"),
        TEXT("/Game/Herbalist/Data/DT_LegendaryEntities"),
        TEXT("/Game/Herbalist/Data/DT_Artifacts"),
        TEXT("/Game/Herbalist/Data/DT_MemoryFragments"),
        TEXT("/Game/Herbalist/Data/DT_Dialogue"),
    };
}

int32 UDataTableExportCommandlet::Main(const FString& Params)
{
    FString OutDir;
    if (!FParse::Value(*Params, TEXT("out="), OutDir) || OutDir.IsEmpty())
    {
        UE_LOG(LogTemp, Error, TEXT("DataTableExport: нужен -out=<папка>"));
        return 1;
    }
    FPaths::NormalizeDirectoryName(OutDir);

    int32 Exported = 0, Failed = 0;

    for (const TCHAR* Path : TablePaths)
    {
        UDataTable* Table = LoadObject<UDataTable>(nullptr, Path);
        if (!Table)
        {
            UE_LOG(LogTemp, Warning, TEXT("  ! не загрузилась: %s"), Path);
            ++Failed;
            continue;
        }

        const FString Json = Table->GetTableAsJSON(EDataTableExportFlags::UseJsonObjectsForStructs);
        const FString FileName = FPaths::Combine(OutDir, Table->GetName() + TEXT(".actual.json"));
        if (!FFileHelper::SaveStringToFile(Json, *FileName, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
        {
            UE_LOG(LogTemp, Error, TEXT("  ! не записался: %s"), *FileName);
            ++Failed;
            continue;
        }

        UE_LOG(LogTemp, Display, TEXT("  + %s -- строк %d -> %s"), Table->GetName().GetCharArray().GetData(),
            Table->GetRowMap().Num(), *FileName);
        ++Exported;
    }

    // Биом-граф -- не DataTable, а UDataAsset: у него нет GetTableAsJSON,
    // выгружаем через общий конвертер UStruct->Json по его UPROPERTY.
    if (UBiomeGraphAsset* Graph = LoadObject<UBiomeGraphAsset>(nullptr, TEXT("/Game/Data/DA_BiomeGraph")))
    {
        FString Json;
        if (FJsonObjectConverter::UStructToJsonObjectString(UBiomeGraphAsset::StaticClass(), Graph, Json, 0, 0))
        {
            const FString FileName = FPaths::Combine(OutDir, TEXT("DA_BiomeGraph.actual.json"));
            if (FFileHelper::SaveStringToFile(Json, *FileName, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
            {
                UE_LOG(LogTemp, Display, TEXT("  + DA_BiomeGraph -> %s"), *FileName);
                ++Exported;
            }
            else
            {
                ++Failed;
            }
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("  ! DA_BiomeGraph: конвертер вернул ошибку"));
            ++Failed;
        }
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("  ! не загрузился: /Game/Data/DA_BiomeGraph"));
        ++Failed;
    }

    UE_LOG(LogTemp, Display, TEXT("DataTableExport: выгружено %d, не удалось %d"), Exported, Failed);
    return Failed > 0 ? 1 : 0;
}
