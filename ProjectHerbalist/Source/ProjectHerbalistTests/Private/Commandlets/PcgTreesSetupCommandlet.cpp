// PcgTreesSetupCommandlet.cpp

#include "PcgTreesSetupCommandlet.h"

#include "Core/PCG/PCGHerbalistBiomeTrees.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Engine/Blueprint.h"
#include "Engine/DataTable.h"
#include "Engine/SCS_Node.h"
#include "Engine/StaticMesh.h"
#include "Engine/SimpleConstructionScript.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Elements/PCGStaticMeshSpawner.h"
#include "MeshSelectors/PCGMeshSelectorBase.h"
#include "Metadata/PCGAttributePropertySelector.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "PCGComponent.h"
#include "PCGEdge.h"
#include "PCGGraph.h"
#include "PCGNode.h"
#include "PCGPin.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "UObject/UnrealType.h"

namespace
{
    const TCHAR* TreesGraphPackage = TEXT("/Game/PCG/PCG_Trees");
    const TCHAR* TreesGraphPath = TEXT("/Game/PCG/PCG_Trees.PCG_Trees");
    const TCHAR* TreeTablePackage = TEXT("/Game/Data/DT_BiomeTrees");
    const TCHAR* TreeTablePath = TEXT("/Game/Data/DT_BiomeTrees.DT_BiomeTrees");
    const TCHAR* BiomeVolumeBlueprintPath = TEXT("/Game/Blueprints/BP_BiomeVolume.BP_BiomeVolume");
    const TCHAR* TreesComponentName = TEXT("PCG_Trees");

    bool SaveTreesAsset(UObject* Asset)
    {
        UPackage* Package = Asset->GetOutermost();
        Package->MarkPackageDirty();
        const FString FileName = FPackageName::LongPackageNameToFilename(
            Package->GetName(), FPackageName::GetAssetPackageExtension());
        FSavePackageArgs Args;
        Args.TopLevelFlags = RF_Public | RF_Standalone;
        Args.SaveFlags = SAVE_NoError;
        return UPackage::SavePackage(Package, Asset, *FileName, Args);
    }

    // Классы узлов движка не экспортированы из модуля PCG -- по пути класса,
    // настройки -- через отражение (как в PcgResourceSlotsSetup).
    UPCGNode* AddTreesNode(UPCGGraph* Graph, const TCHAR* ClassName, int32 PosX, int32 PosY, UPCGSettings*& OutSettings)
    {
        OutSettings = nullptr;
        UClass* Class = LoadObject<UClass>(nullptr, *FString::Printf(TEXT("/Script/PCG.%s"), ClassName));
        UPCGNode* Node = Class ? Graph->AddNodeOfType(Class, OutSettings) : nullptr;
        if (Node)
        {
            Node->SetNodePosition(PosX, PosY);
        }
        return Node;
    }

    bool SetTreesSetting(UObject* Settings, const TCHAR* Name, const TCHAR* Value)
    {
        FProperty* Property = Settings ? Settings->GetClass()->FindPropertyByName(Name) : nullptr;
        const bool bSet = Property && Property->ImportText_InContainer(Value, Settings, Settings, PPF_None) != nullptr;
        if (!bSet)
        {
            UE_LOG(LogTemp, Error, TEXT("Настройка %s.%s = %s не принята"), Settings ? *Settings->GetClass()->GetName() : TEXT("?"), Name, Value);
        }
        return bSet;
    }

    // Get Spline Data: сплайн своего объёма по тегу компонента, фильтр
    // Original -- и на актерах разделов (как у травы, 2026-09-27).
    bool SetSplineSource(UPCGSettings* Settings, const TCHAR* ComponentTag)
    {
        const FString Actor = TEXT("(ActorFilter=Original,ActorSelection=ByTag,ActorSelectionTag=\"\")");
        const FString Component = FString::Printf(TEXT("(ComponentSelection=ByTag,ComponentSelectionTag=\"%s\")"), ComponentTag);
        return SetTreesSetting(Settings, TEXT("ActorSelector"), *Actor)
            && SetTreesSetting(Settings, TEXT("ComponentSelector"), *Component);
    }

    // Attribute Filter: Ground < GroundLayerMax (вес слоя от Projection).
    bool SetGroundFilter(UPCGSettings* Settings)
    {
        FStructProperty* Target = CastField<FStructProperty>(Settings->GetClass()->FindPropertyByName(TEXT("TargetAttribute")));
        FStructProperty* Constant = CastField<FStructProperty>(Settings->GetClass()->FindPropertyByName(TEXT("AttributeTypes")));
        if (!Target || !Constant || !Target->Struct->IsChildOf(FPCGAttributePropertySelector::StaticStruct()))
        {
            UE_LOG(LogTemp, Error, TEXT("У Attribute Filter нет TargetAttribute/AttributeTypes"));
            return false;
        }
        Target->ContainerPtrToValuePtr<FPCGAttributePropertySelector>(Settings)->SetAttributeName(TEXT("Ground"));
        void* Container = Constant->ContainerPtrToValuePtr<void>(Settings);
        const FEnumProperty* Type = CastField<FEnumProperty>(Constant->Struct->FindPropertyByName(TEXT("Type")));
        const FFloatProperty* FloatValue = CastField<FFloatProperty>(Constant->Struct->FindPropertyByName(TEXT("FloatValue")));
        if (!Type || !FloatValue || !Type->ImportText_InContainer(TEXT("Float"), Container, Settings, PPF_None))
        {
            UE_LOG(LogTemp, Error, TEXT("Порог Ground не принят"));
            return false;
        }
        FloatValue->SetPropertyValue_InContainer(Container, UPcgTreesSetupCommandlet::GroundLayerMax);
        return SetTreesSetting(Settings, TEXT("Operator"), TEXT("Lesser"))
            && SetTreesSetting(Settings, TEXT("bUseConstantThreshold"), TEXT("True"));
    }

    // Static Mesh Spawner: меш -- из атрибута TreeMesh.
    bool SetSpawnerByAttribute(UPCGSettings* Settings, FName Attribute)
    {
        UClass* ByAttribute = LoadObject<UClass>(nullptr, TEXT("/Script/PCG.PCGMeshSelectorByAttribute"));
        UPCGStaticMeshSpawnerSettings* Spawner = static_cast<UPCGStaticMeshSpawnerSettings*>(Settings);
        if (!ByAttribute || !Settings || Settings->GetClass()->GetName() != TEXT("PCGStaticMeshSpawnerSettings"))
        {
            return false;
        }
        Spawner->SetMeshSelectorType(ByAttribute);
        FObjectProperty* ParamsProperty = CastField<FObjectProperty>(Settings->GetClass()->FindPropertyByName(TEXT("MeshSelectorParameters")));
        UObject* Params = ParamsProperty ? ParamsProperty->GetObjectPropertyValue_InContainer(Settings) : nullptr;
        return Params && Params->IsA(ByAttribute) && SetTreesSetting(Params, TEXT("AttributeName"), *Attribute.ToString());
    }

    bool HasTreesEdge(const UPCGNode* From, const UPCGNode* To)
    {
        for (const UPCGPin* Pin : From->GetOutputPins())
        {
            for (const UPCGEdge* Edge : Pin->Edges)
            {
                if (Edge && Edge->OutputPin && Edge->OutputPin->Node == To) return true;
            }
        }
        return false;
    }
}

int32 UPcgTreesSetupCommandlet::BuildTreesGraph(UPCGGraph* Graph)
{
    if (!Graph)
    {
        return -1;
    }
    // Граф уже не пустой (собран раньше или правлен художником) -- не трогаем.
    if (Graph->GetNodes().Num() > 0)
    {
        return 0;
    }

    UPCGSettings* RegionSettings = nullptr;
    UPCGSettings* RegionSurfaceSettings = nullptr;
    UPCGSettings* SamplerSettings = nullptr;
    UPCGSettings* LandscapeSettings = nullptr;
    UPCGSettings* ProjectionSettings = nullptr;
    UPCGSettings* GroundSettings = nullptr;
    UPCGSettings* PruningSettings = nullptr;
    UPCGSettings* SpawnerSettings = nullptr;
    UPCGNode* Region = AddTreesNode(Graph, TEXT("PCGGetSplineSettings"), 0, 0, RegionSettings);
    UPCGNode* RegionSurface = AddTreesNode(Graph, TEXT("PCGCreateSurfaceFromSplineSettings"), 300, 0, RegionSurfaceSettings);
    UPCGNode* Sampler = AddTreesNode(Graph, TEXT("PCGSurfaceSamplerSettings"), 600, 0, SamplerSettings);
    UPCGNode* Landscape = AddTreesNode(Graph, TEXT("PCGGetLandscapeSettings"), 900, 300, LandscapeSettings);
    UPCGNode* Projection = AddTreesNode(Graph, TEXT("PCGProjectionSettings"), 1200, 0, ProjectionSettings);
    UPCGNode* Ground = AddTreesNode(Graph, TEXT("PCGAttributeFilteringSettings"), 1500, 0, GroundSettings);
    UPCGNode* Pruning = AddTreesNode(Graph, TEXT("PCGSelfPruningSettings"), 2100, 0, PruningSettings);
    UPCGNode* Spawner = AddTreesNode(Graph, TEXT("PCGStaticMeshSpawnerSettings"), 2400, 0, SpawnerSettings);
    UPCGSettings* TreesSettings = nullptr;
    UPCGNode* Trees = Graph->AddNodeOfType(UPCGHerbalistBiomeTreesSettings::StaticClass(), TreesSettings);
    if (!Region || !RegionSurface || !Sampler || !Landscape
        || !Projection || !Ground || !Pruning || !Spawner || !Trees)
    {
        UE_LOG(LogTemp, Error, TEXT("Узлы графа деревьев не созданы"));
        return -1;
    }
    Trees->SetNodePosition(1800, 0);
    CastChecked<UPCGHerbalistBiomeTreesSettings>(TreesSettings)->SamplerTreesPer100SquareMeters = SamplerTreesPer100SquareMeters;

    const bool bConfigured = SetSplineSource(RegionSettings, TEXT("Biome"))
        && SetTreesSetting(SamplerSettings, TEXT("PointsPerSquaredMeter"), *FString::SanitizeFloat(SamplerTreesPer100SquareMeters / 100.0f))
        && SetTreesSetting(SamplerSettings, TEXT("bUnbounded"), TEXT("True"))
        // Высота и веса слоёв -- с ландшафта; ствол стоит прямо, не по склону.
        && SetTreesSetting(ProjectionSettings, TEXT("ProjectionParams"), TEXT("(bProjectPositions=True,bProjectRotations=False,bProjectScales=False)"))
        && SetGroundFilter(GroundSettings)
        && SetSpawnerByAttribute(SpawnerSettings, CastChecked<UPCGHerbalistBiomeTreesSettings>(TreesSettings)->MeshAttribute);
    if (!bConfigured)
    {
        return -1;
    }

    TArray<TPair<UPCGNode*, UPCGNode*>> Edges;
    auto Link = [Graph, &Edges](UPCGNode* From, FName FromPin, UPCGNode* To, FName ToPin)
    {
        Graph->AddLabeledEdge(From, FromPin, To, ToPin);
        Edges.Add({ From, To });
    };
    const FName Out = PCGPinConstants::DefaultOutputLabel;
    const FName In = PCGPinConstants::DefaultInputLabel;
    Link(Region, Out, RegionSurface, In);
    Link(RegionSurface, Out, Sampler, TEXT("Surface"));
    // Воду отбрасывает узел деревьев (IsOverWater): вычитание поверхности
    // сплайна пруда здесь сравнивало точки в 3D, а пруд и регион -- на разной
    // высоте (2026-09-28, по PIE: деревья в воде).
    Link(Sampler, Out, Projection, In);
    Link(Landscape, Out, Projection, TEXT("Projection Target"));
    Link(Projection, Out, Ground, In);
    Link(Ground, PCGPinConstants::DefaultInFilterLabel, Trees, In);
    Link(Trees, Out, Pruning, In);
    Link(Pruning, Out, Spawner, In);

    // AddLabeledEdge не сообщает, удалась ли связь (неверная метка пина молчит)
    // -- проверяем каждую, иначе сохранился бы полусвязанный граф.
    for (const TPair<UPCGNode*, UPCGNode*>& Edge : Edges)
    {
        if (!HasTreesEdge(Edge.Key, Edge.Value))
        {
            UE_LOG(LogTemp, Error, TEXT("Нет связи %s -> %s"), *Edge.Key->GetNodeTitle(EPCGNodeTitleType::ListView).ToString(),
                *Edge.Value->GetNodeTitle(EPCGNodeTitleType::ListView).ToString());
            return -1;
        }
    }
    return 1;
}

int32 UPcgTreesSetupCommandlet::FillTreeTable(UDataTable* Table, const FString& JsonText)
{
    if (!Table || Table->GetRowStruct() != FHerbalistBiomeTreeRow::StaticStruct())
    {
        return -1;
    }
    TArray<TSharedPtr<FJsonValue>> Rows;
    const TSharedRef<TJsonReader<TCHAR>> Reader = TJsonReaderFactory<TCHAR>::Create(JsonText);
    if (!FJsonSerializer::Deserialize(Reader, Rows))
    {
        UE_LOG(LogTemp, Error, TEXT("biome_trees.json не разобран"));
        return -1;
    }
    const UEnum* BiomeEnum = StaticEnum<EBiomeType>();
    TArray<TPair<FName, FHerbalistBiomeTreeRow>> Parsed;
    for (const TSharedPtr<FJsonValue>& Value : Rows)
    {
        const TSharedPtr<FJsonObject> Obj = Value.IsValid() ? Value->AsObject() : nullptr;
        if (!Obj.IsValid()) continue;
        FHerbalistBiomeTreeRow Row;
        const FString Name = Obj->GetStringField(TEXT("Name"));
        const int64 Biome = BiomeEnum->GetValueByNameString(Obj->GetStringField(TEXT("Biome")));
        const FString MeshPath = Obj->GetStringField(TEXT("Mesh"));
        if (Name.IsEmpty() || Biome == INDEX_NONE || MeshPath.IsEmpty() || !LoadObject<UStaticMesh>(nullptr, *MeshPath))
        {
            UE_LOG(LogTemp, Error, TEXT("Строка деревьев '%s': биом '%s' или меш '%s' не найден"), *Name,
                *Obj->GetStringField(TEXT("Biome")), *MeshPath);
            return -1;
        }
        Row.Biome = static_cast<EBiomeType>(Biome);
        Row.Mesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(MeshPath));
        Row.TreesPer100SquareMeters = static_cast<float>(Obj->GetNumberField(TEXT("TreesPer100SquareMeters")));
        Row.ScaleMin = static_cast<float>(Obj->GetNumberField(TEXT("ScaleMin")));
        Row.ScaleMax = static_cast<float>(Obj->GetNumberField(TEXT("ScaleMax")));
        Row.SpacingMeters = static_cast<float>(Obj->GetNumberField(TEXT("SpacingMeters")));
        Parsed.Add({ FName(*Name), Row });
    }
    // Всё проверено -- теперь менять.
    Table->EmptyTable();
    for (const TPair<FName, FHerbalistBiomeTreeRow>& Row : Parsed)
    {
        Table->AddRow(Row.Key, Row.Value);
    }
    return Parsed.Num();
}

int32 UPcgTreesSetupCommandlet::EnsureTreesComponent(UBlueprint* Blueprint, UPCGGraph* Graph)
{
    if (!Blueprint || !Blueprint->SimpleConstructionScript || !Graph)
    {
        return -1;
    }
    USimpleConstructionScript* Construction = Blueprint->SimpleConstructionScript;
    UPCGComponent* Template = nullptr;
    for (USCS_Node* Node : Construction->GetAllNodes())
    {
        UPCGComponent* Candidate = Node ? Cast<UPCGComponent>(Node->ComponentTemplate) : nullptr;
        if (Candidate && Candidate->GetGraph() == Graph)
        {
            Template = Candidate;
        }
    }
    bool bChanged = false;
    if (!Template)
    {
        USCS_Node* Node = Construction->CreateNode(UPCGComponent::StaticClass(), TreesComponentName);
        Template = Node ? Cast<UPCGComponent>(Node->ComponentTemplate) : nullptr;
        if (!Template)
        {
            return -1;
        }
        Construction->AddNode(Node);
        Template->SetGraph(Graph);
        bChanged = true;
    }
    // По запросу: в игре граф не работает, запекание -- билдером мира.
    if (Template->GenerationTrigger != EPCGComponentGenerationTrigger::GenerateOnDemand)
    {
        Template->Modify();
        Template->GenerationTrigger = EPCGComponentGenerationTrigger::GenerateOnDemand;
        bChanged = true;
    }
    return bChanged ? 1 : 0;
}

int32 UPcgTreesSetupCommandlet::Main(const FString& Params)
{
    UE_LOG(LogTemp, Display, TEXT("=== PcgTreesSetup ==="));

    // 1. Таблица деревьев.
    UDataTable* Table = LoadObject<UDataTable>(nullptr, TreeTablePath, nullptr, LOAD_NoWarn | LOAD_Quiet);
    const bool bNewTable = Table == nullptr;
    if (bNewTable)
    {
        UPackage* Package = CreatePackage(TreeTablePackage);
        Table = NewObject<UDataTable>(Package, TEXT("DT_BiomeTrees"), RF_Public | RF_Standalone);
        Table->RowStruct = FHerbalistBiomeTreeRow::StaticStruct();
        FAssetRegistryModule::AssetCreated(Table);
    }
    if (bNewTable || FParse::Param(*Params, TEXT("refilltable")))
    {
        const FString JsonPath = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT(".."),
            TEXT("herbalist_docs"), TEXT("CSV_tabs"), TEXT("biome_trees.json")));
        FString JsonText;
        if (!FFileHelper::LoadFileToString(JsonText, *JsonPath))
        {
            UE_LOG(LogTemp, Error, TEXT("Не прочитан %s"), *JsonPath);
            return 1;
        }
        const int32 RowCount = FillTreeTable(Table, JsonText);
        if (RowCount < 0 || !SaveTreesAsset(Table))
        {
            UE_LOG(LogTemp, Error, TEXT("Таблица деревьев не заполнена -- %s не сохранена"), TreeTablePath);
            return 1;
        }
        UE_LOG(LogTemp, Display, TEXT("DT_BiomeTrees: %d строк"), RowCount);
    }
    else
    {
        UE_LOG(LogTemp, Display, TEXT("DT_BiomeTrees: уже есть, не трогаю (-refilltable заполнит заново)"));
    }

    // 2. Граф.
    UPCGGraph* Graph = LoadObject<UPCGGraph>(nullptr, TreesGraphPath, nullptr, LOAD_NoWarn | LOAD_Quiet);
    if (!Graph)
    {
        UPackage* Package = CreatePackage(TreesGraphPackage);
        Graph = NewObject<UPCGGraph>(Package, TEXT("PCG_Trees"), RF_Public | RF_Standalone);
        FAssetRegistryModule::AssetCreated(Graph);
    }
    if (FParse::Param(*Params, TEXT("rebuildgraph")) && Graph->GetNodes().Num() > 0)
    {
        TArray<UPCGNode*> Nodes = Graph->GetNodes();
        Graph->RemoveNodes(Nodes);
        UE_LOG(LogTemp, Display, TEXT("PCG_Trees: узлы убраны (-rebuildgraph), собираю заново"));
    }
    const int32 GraphResult = BuildTreesGraph(Graph);
    if (GraphResult < 0 || (GraphResult > 0 && !SaveTreesAsset(Graph)))
    {
        UE_LOG(LogTemp, Error, TEXT("Граф деревьев не собрался -- %s не сохранён"), TreesGraphPath);
        return 1;
    }
    UE_LOG(LogTemp, Display, TEXT("PCG_Trees: %s"), GraphResult == 0 ? TEXT("уже собран") : TEXT("собран"));

    // 3. Компонент в BP_BiomeVolume.
    UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, BiomeVolumeBlueprintPath);
    const int32 ComponentResult = EnsureTreesComponent(Blueprint, Graph);
    if (ComponentResult < 0)
    {
        UE_LOG(LogTemp, Error, TEXT("PCG-компонент деревьев в %s не добавлен"), BiomeVolumeBlueprintPath);
        return 1;
    }
    if (ComponentResult > 0)
    {
        FKismetEditorUtilities::CompileBlueprint(Blueprint);
        if (!SaveTreesAsset(Blueprint))
        {
            UE_LOG(LogTemp, Error, TEXT("Не удалось сохранить %s"), BiomeVolumeBlueprintPath);
            return 1;
        }
    }
    UE_LOG(LogTemp, Display, TEXT("BP_BiomeVolume: компонент деревьев %s"), ComponentResult == 0 ? TEXT("уже есть") : TEXT("добавлен"));
    UE_LOG(LogTemp, Display, TEXT("=== PcgTreesSetup: готово ==="));
    return 0;
}
