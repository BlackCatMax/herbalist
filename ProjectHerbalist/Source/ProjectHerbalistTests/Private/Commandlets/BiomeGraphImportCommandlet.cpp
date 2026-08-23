// BiomeGraphImportCommandlet.cpp
#include "Commandlets/BiomeGraphImportCommandlet.h"
#include "Core/BiomeGraph/BiomeGraphAsset.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonReader.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/PackageName.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

int32 UBiomeGraphImportCommandlet::Main(const FString& Params)
{
    const FString InPath = FPaths::ConvertRelativePathToFull(
        FPaths::Combine(FPaths::ProjectDir(), TEXT(".."), TEXT("herbalist_docs"), TEXT("CSV_tabs"), TEXT("DA_BiomeGraph.json")));

    FString JsonText;
    if (!FFileHelper::LoadFileToString(JsonText, *InPath))
    {
        UE_LOG(LogTemp, Error, TEXT("BiomeGraphImport: не удалось прочитать %s"), *InPath);
        return 1;
    }

    TSharedPtr<FJsonObject> Root;
    TSharedRef<TJsonReader<TCHAR>> Reader = TJsonReaderFactory<TCHAR>::Create(JsonText);
    if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("BiomeGraphImport: не удалось разобрать JSON %s"), *InPath);
        return 1;
    }

    const TCHAR* AssetPath = TEXT("/Game/Data/DA_BiomeGraph");
    UBiomeGraphAsset* Asset = LoadObject<UBiomeGraphAsset>(nullptr, AssetPath);
    if (!Asset)
    {
        UE_LOG(LogTemp, Error, TEXT("BiomeGraphImport: не удалось загрузить %s"), AssetPath);
        return 1;
    }

    Asset->GlobalMorokDecay = Root->GetNumberField(TEXT("GlobalMorokDecay"));
    Asset->GlobalZaryanaDecay = Root->GetNumberField(TEXT("GlobalZaryanaDecay"));
    Asset->PotionCollapseThreshold = Root->GetNumberField(TEXT("PotionCollapseThreshold"));
    Asset->BiomeCollapseThreshold = Root->GetNumberField(TEXT("BiomeCollapseThreshold"));
    Asset->FixedTimeStep = Root->GetNumberField(TEXT("FixedTimeStep"));
    Asset->GlobalInfluenceScale = Root->GetNumberField(TEXT("GlobalInfluenceScale"));
    Asset->GridBlendFactor = Root->GetNumberField(TEXT("GridBlendFactor"));

    Asset->Nodes.Empty();
    for (const TSharedPtr<FJsonValue>& Value : Root->GetArrayField(TEXT("Nodes")))
    {
        const TSharedPtr<FJsonObject> NodeObj = Value->AsObject();
        if (!NodeObj.IsValid()) continue;

        FBiomeGraphNodeEntry Entry;
        Entry.BiomeID = FName(*NodeObj->GetStringField(TEXT("BiomeID")));
        Entry.Node.MorokAffinity = NodeObj->GetNumberField(TEXT("MorokAffinity"));
        Entry.Node.ZaryanaAffinity = NodeObj->GetNumberField(TEXT("ZaryanaAffinity"));
        Entry.Node.Stability = NodeObj->GetNumberField(TEXT("Stability"));
        Asset->Nodes.Add(Entry);
    }

    Asset->Edges.Empty();
    for (const TSharedPtr<FJsonValue>& Value : Root->GetArrayField(TEXT("Edges")))
    {
        const TSharedPtr<FJsonObject> EdgeObj = Value->AsObject();
        if (!EdgeObj.IsValid()) continue;

        FBiomeGraphEdge Edge;
        Edge.FromBiome = FName(*EdgeObj->GetStringField(TEXT("FromBiome")));
        Edge.ToBiome = FName(*EdgeObj->GetStringField(TEXT("ToBiome")));
        Edge.MorokLeak = EdgeObj->GetNumberField(TEXT("MorokLeak"));
        Edge.ZaryanaFlow = EdgeObj->GetNumberField(TEXT("ZaryanaFlow"));
        Asset->Edges.Add(Edge);
    }

    Asset->MarkPackageDirty();

    UPackage* Package = Asset->GetOutermost();
    const FString PackageFileName = FPackageName::LongPackageNameToFilename(
        Package->GetName(), FPackageName::GetAssetPackageExtension());

    FSavePackageArgs SaveArgs;
    SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
    SaveArgs.SaveFlags = SAVE_NoError;

    const bool bSuccess = UPackage::SavePackage(Package, Asset, *PackageFileName, SaveArgs);
    if (!bSuccess)
    {
        UE_LOG(LogTemp, Error, TEXT("BiomeGraphImport: не удалось сохранить пакет %s"), *PackageFileName);
        return 1;
    }

    UE_LOG(LogTemp, Display, TEXT("BiomeGraphImport: записано в %s (%d узлов, %d рёбер) из %s"),
        AssetPath, Asset->Nodes.Num(), Asset->Edges.Num(), *InPath);
    return 0;
}
