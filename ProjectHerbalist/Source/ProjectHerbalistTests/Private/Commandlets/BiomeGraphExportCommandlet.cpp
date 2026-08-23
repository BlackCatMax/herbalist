// BiomeGraphExportCommandlet.cpp
#include "Commandlets/BiomeGraphExportCommandlet.h"
#include "Core/BiomeGraph/BiomeGraphAsset.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/PlatformFileManager.h"

namespace
{
    TSharedRef<FJsonObject> NodeToJson(const FBiomeGraphNodeEntry& Entry)
    {
        TSharedRef<FJsonObject> Obj = MakeShared<FJsonObject>();
        Obj->SetStringField(TEXT("BiomeID"), Entry.BiomeID.ToString());
        Obj->SetNumberField(TEXT("MorokAffinity"), Entry.Node.MorokAffinity);
        Obj->SetNumberField(TEXT("ZaryanaAffinity"), Entry.Node.ZaryanaAffinity);
        Obj->SetNumberField(TEXT("Stability"), Entry.Node.Stability);
        return Obj;
    }

    TSharedRef<FJsonObject> EdgeToJson(const FBiomeGraphEdge& Edge)
    {
        TSharedRef<FJsonObject> Obj = MakeShared<FJsonObject>();
        Obj->SetStringField(TEXT("FromBiome"), Edge.FromBiome.ToString());
        Obj->SetStringField(TEXT("ToBiome"), Edge.ToBiome.ToString());
        Obj->SetNumberField(TEXT("MorokLeak"), Edge.MorokLeak);
        Obj->SetNumberField(TEXT("ZaryanaFlow"), Edge.ZaryanaFlow);
        return Obj;
    }
}

int32 UBiomeGraphExportCommandlet::Main(const FString& Params)
{
    const TCHAR* AssetPath = TEXT("/Game/Data/DA_BiomeGraph");
    UBiomeGraphAsset* Asset = LoadObject<UBiomeGraphAsset>(nullptr, AssetPath);
    if (!Asset)
    {
        UE_LOG(LogTemp, Error, TEXT("BiomeGraphExport: не удалось загрузить %s"), AssetPath);
        return 1;
    }

    TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
    Root->SetNumberField(TEXT("GlobalMorokDecay"), Asset->GlobalMorokDecay);
    Root->SetNumberField(TEXT("GlobalZaryanaDecay"), Asset->GlobalZaryanaDecay);
    Root->SetNumberField(TEXT("PotionCollapseThreshold"), Asset->PotionCollapseThreshold);
    Root->SetNumberField(TEXT("BiomeCollapseThreshold"), Asset->BiomeCollapseThreshold);
    Root->SetNumberField(TEXT("FixedTimeStep"), Asset->FixedTimeStep);
    Root->SetNumberField(TEXT("GlobalInfluenceScale"), Asset->GlobalInfluenceScale);
    Root->SetNumberField(TEXT("GridBlendFactor"), Asset->GridBlendFactor);

    TArray<TSharedPtr<FJsonValue>> NodesJson;
    for (const FBiomeGraphNodeEntry& Entry : Asset->Nodes)
    {
        NodesJson.Add(MakeShared<FJsonValueObject>(NodeToJson(Entry)));
    }
    Root->SetArrayField(TEXT("Nodes"), NodesJson);

    TArray<TSharedPtr<FJsonValue>> EdgesJson;
    for (const FBiomeGraphEdge& Edge : Asset->Edges)
    {
        EdgesJson.Add(MakeShared<FJsonValueObject>(EdgeToJson(Edge)));
    }
    Root->SetArrayField(TEXT("Edges"), EdgesJson);

    FString Output;
    TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> Writer =
        TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>::Create(&Output);
    FJsonSerializer::Serialize(Root, Writer);

    // Соседняя папка с DT_BiomeDefaults.json/DT_IngredientClass.json — тот же
    // "docs как источник обзора" каталог, herbalist_docs лежит рядом с
    // ProjectHerbalist/, не внутри него.
    const FString OutPath = FPaths::ConvertRelativePathToFull(
        FPaths::Combine(FPaths::ProjectDir(), TEXT(".."), TEXT("herbalist_docs"), TEXT("CSV_tabs"), TEXT("DA_BiomeGraph.json")));

    if (!FFileHelper::SaveStringToFile(Output, *OutPath))
    {
        UE_LOG(LogTemp, Error, TEXT("BiomeGraphExport: не удалось записать %s"), *OutPath);
        return 1;
    }

    UE_LOG(LogTemp, Display, TEXT("BiomeGraphExport: записано %s (%d узлов, %d рёбер)"),
        *OutPath, Asset->Nodes.Num(), Asset->Edges.Num());
    return 0;
}
