// PcgResourceSlotsSetupCommandlet.cpp

#include "PcgResourceSlotsSetupCommandlet.h"

#include "Core/PCG/PCGHerbalistWriteResourceSlots.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/Blueprint.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "PCGComponent.h"
#include "PCGEdge.h"
#include "PCGGraph.h"
#include "PCGNode.h"
#include "PCGPin.h"
#include "Metadata/PCGAttributePropertySelector.h"
#include "UObject/Package.h"
#include "UObject/UnrealType.h"
#include "UObject/SavePackage.h"
#include "Misc/PackageName.h"

namespace
{
    const TCHAR* SlotsGraphPackage = TEXT("/Game/PCG/PCG_ResourceSlots");
    const TCHAR* SlotsGraphPath = TEXT("/Game/PCG/PCG_ResourceSlots.PCG_ResourceSlots");
    const TCHAR* WaterVolumeBlueprintPath = TEXT("/Game/Blueprints/BP_WaterVolume.BP_WaterVolume");

    bool SaveSlotsSetupAsset(UObject* Asset)
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
    // настройки -- через отражение.
    UPCGNode* AddEngineNode(UPCGGraph* Graph, const TCHAR* ClassName, int32 PosX, int32 PosY, UPCGSettings*& OutSettings)
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

    bool SetSlotsSetting(UPCGSettings* Settings, const TCHAR* Name, const TCHAR* Value)
    {
        FProperty* Property = Settings ? Settings->GetClass()->FindPropertyByName(Name) : nullptr;
        const bool bSet = Property && Property->ImportText_InContainer(Value, Settings, Settings, PPF_None) != nullptr;
        if (!bSet)
        {
            UE_LOG(LogTemp, Error, TEXT("Настройка %s.%s = %s не принята"), Settings ? *Settings->GetClass()->GetName() : TEXT("?"), Name, Value);
        }
        return bSet;
    }

    // Add Attribute: строковая константа Value в атрибут SlotKind.
    bool SetSlotKindConstant(UPCGSettings* Settings, const TCHAR* Value)
    {
        // Селектор атрибута текстом принимает только формат экспорта с
        // метками -- имя ставим его собственным API.
        FStructProperty* Target = CastField<FStructProperty>(Settings->GetClass()->FindPropertyByName(TEXT("OutputTarget")));
        if (!Target || !Target->Struct->IsChildOf(FPCGAttributePropertySelector::StaticStruct()))
        {
            UE_LOG(LogTemp, Error, TEXT("У Add Attribute нет OutputTarget"));
            return false;
        }
        Target->ContainerPtrToValuePtr<FPCGAttributePropertySelector>(Settings)->SetAttributeName(TEXT("SlotKind"));
        FStructProperty* Constant = CastField<FStructProperty>(Settings->GetClass()->FindPropertyByName(TEXT("AttributeTypes")));
        if (!Constant)
        {
            return false;
        }
        void* Container = Constant->ContainerPtrToValuePtr<void>(Settings);
        const FEnumProperty* Type = CastField<FEnumProperty>(Constant->Struct->FindPropertyByName(TEXT("Type")));
        const FStrProperty* StringValue = CastField<FStrProperty>(Constant->Struct->FindPropertyByName(TEXT("StringValue")));
        if (!Type || !StringValue || !Type->ImportText_InContainer(TEXT("String"), Container, Settings, PPF_None))
        {
            UE_LOG(LogTemp, Error, TEXT("Константа SlotKind = %s не принята"), Value);
            return false;
        }
        StringValue->SetPropertyValue_InContainer(Container, FString(Value));
        return true;
    }

    bool HasSlotsEdge(const UPCGNode* From, const UPCGNode* To)
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

    // Ветка кромки или суши: точки по сплайну -> случайный сдвиг -> минус
    // вода -> на ландшафт -> SlotKind.
    UPCGNode* AddEdgeBranch(UPCGGraph* Graph, UPCGNode* Spline, UPCGNode* WaterSurface, UPCGNode* Landscape,
        float StepLocal, float OffsetCm, const TCHAR* Kind, int32 Row, TArray<TPair<UPCGNode*, UPCGNode*>>& OutEdges)
    {
        const int32 Y = Row * 250;
        UPCGSettings* SamplerSettings = nullptr;
        UPCGSettings* TransformSettings = nullptr;
        UPCGSettings* DifferenceSettings = nullptr;
        UPCGSettings* ProjectionSettings = nullptr;
        UPCGSettings* KindSettings = nullptr;
        UPCGNode* Sampler = AddEngineNode(Graph, TEXT("PCGSplineSamplerSettings"), 400, Y, SamplerSettings);
        UPCGNode* Transform = AddEngineNode(Graph, TEXT("PCGTransformPointsSettings"), 700, Y, TransformSettings);
        UPCGNode* Difference = AddEngineNode(Graph, TEXT("PCGDifferenceSettings"), 1000, Y, DifferenceSettings);
        UPCGNode* Projection = AddEngineNode(Graph, TEXT("PCGProjectionSettings"), 1300, Y, ProjectionSettings);
        UPCGNode* KindNode = AddEngineNode(Graph, TEXT("PCGAddAttributeSettings"), 1600, Y, KindSettings);
        if (!Sampler || !Transform || !Difference || !Projection || !KindNode)
        {
            UE_LOG(LogTemp, Error, TEXT("Узлы ветки %s не созданы"), Kind);
            return nullptr;
        }

        const FString SamplerParams = FString::Printf(TEXT("(Dimension=OnSpline,Mode=Distance,DistanceIncrement=%.1f)"), StepLocal);
        const FString OffsetMin = FString::Printf(TEXT("(X=%.1f,Y=%.1f,Z=0.0)"), -OffsetCm, -OffsetCm);
        const FString OffsetMax = FString::Printf(TEXT("(X=%.1f,Y=%.1f,Z=0.0)"), OffsetCm, OffsetCm);
        const bool bConfigured = SetSlotsSetting(SamplerSettings, TEXT("SamplerParams"), *SamplerParams)
            && SetSlotsSetting(TransformSettings, TEXT("bAbsoluteOffset"), TEXT("True"))
            && SetSlotsSetting(TransformSettings, TEXT("OffsetMin"), *OffsetMin)
            && SetSlotsSetting(TransformSettings, TEXT("OffsetMax"), *OffsetMax)
            && SetSlotsSetting(DifferenceSettings, TEXT("DensityFunction"), TEXT("Binary"))
            && SetSlotsSetting(DifferenceSettings, TEXT("Mode"), TEXT("Discrete"))
            // Высота -- с ландшафта; поворот и масштаб слоту не нужны.
            && SetSlotsSetting(ProjectionSettings, TEXT("ProjectionParams"), TEXT("(bProjectPositions=True,bProjectRotations=False,bProjectScales=False)"))
            && SetSlotKindConstant(KindSettings, Kind);
        if (!bConfigured)
        {
            return nullptr;
        }

        Graph->AddLabeledEdge(Spline, PCGPinConstants::DefaultOutputLabel, Sampler, TEXT("Spline"));
        Graph->AddLabeledEdge(Sampler, PCGPinConstants::DefaultOutputLabel, Transform, PCGPinConstants::DefaultInputLabel);
        Graph->AddLabeledEdge(Transform, PCGPinConstants::DefaultOutputLabel, Difference, TEXT("Source"));
        Graph->AddLabeledEdge(WaterSurface, PCGPinConstants::DefaultOutputLabel, Difference, TEXT("Differences"));
        Graph->AddLabeledEdge(Difference, PCGPinConstants::DefaultOutputLabel, Projection, PCGPinConstants::DefaultInputLabel);
        Graph->AddLabeledEdge(Landscape, PCGPinConstants::DefaultOutputLabel, Projection, TEXT("Projection Target"));
        Graph->AddLabeledEdge(Projection, PCGPinConstants::DefaultOutputLabel, KindNode, PCGPinConstants::DefaultInputLabel);
        OutEdges.Append({ { Spline, Sampler }, { Sampler, Transform }, { Transform, Difference }, { WaterSurface, Difference },
            { Difference, Projection }, { Landscape, Projection }, { Projection, KindNode } });
        return KindNode;
    }
}

int32 UPcgResourceSlotsSetupCommandlet::BuildSlotsGraph(UPCGGraph* Graph)
{
    if (!Graph)
    {
        return -1;
    }
    // Граф уже не пустой (собран раньше или правлен художником) -- не трогаем:
    // досборка поверх правок задвоила бы узлы.
    if (Graph->GetNodes().Num() > 0)
    {
        return 0;
    }

    TArray<TPair<UPCGNode*, UPCGNode*>> Edges;
    UPCGSettings* SplineSettings = nullptr;
    UPCGSettings* SurfaceSettings = nullptr;
    UPCGSettings* LandscapeSettings = nullptr;
    UPCGSettings* WaterSamplerSettings = nullptr;
    UPCGSettings* WaterKindSettings = nullptr;
    UPCGNode* Spline = AddEngineNode(Graph, TEXT("PCGGetSplineSettings"), 0, 250, SplineSettings);   // Self по умолчанию
    UPCGNode* WaterSurface = AddEngineNode(Graph, TEXT("PCGCreateSurfaceFromSplineSettings"), 400, 750, SurfaceSettings);
    UPCGNode* Landscape = AddEngineNode(Graph, TEXT("PCGGetLandscapeSettings"), 1000, 750, LandscapeSettings);
    UPCGNode* WaterSampler = AddEngineNode(Graph, TEXT("PCGSurfaceSamplerSettings"), 1000, 0, WaterSamplerSettings);
    UPCGNode* WaterKind = AddEngineNode(Graph, TEXT("PCGAddAttributeSettings"), 1600, 0, WaterKindSettings);
    if (!Spline || !WaterSurface || !Landscape || !WaterSampler || !WaterKind)
    {
        UE_LOG(LogTemp, Error, TEXT("Узлы движка не созданы: %d%d%d%d%d"), !!Spline, !!WaterSurface, !!Landscape, !!WaterSampler, !!WaterKind);
        return -1;
    }
    // Вода -- выборка поверхности внутренности сплайна: плотность на м² мира.
    // Выборка сплайна «по внутренности» считает шаг в локальных единицах
    // сплайна и на растянутом акторе (L_TestDev: ×3.4) не дала ни точки.
    // Без границ актора: поверхность сама задаёт, где вода.
    if (!SetSlotsSetting(WaterSamplerSettings, TEXT("PointsPerSquaredMeter"), *FString::SanitizeFloat(WaterPointsPerM2))
        || !SetSlotsSetting(WaterSamplerSettings, TEXT("bUnbounded"), TEXT("True"))
        || !SetSlotKindConstant(WaterKindSettings, TEXT("Water")))
    {
        return -1;
    }
    Graph->AddLabeledEdge(Spline, PCGPinConstants::DefaultOutputLabel, WaterSurface, PCGPinConstants::DefaultInputLabel);
    Graph->AddLabeledEdge(WaterSurface, PCGPinConstants::DefaultOutputLabel, WaterSampler, TEXT("Surface"));
    Graph->AddLabeledEdge(WaterSampler, PCGPinConstants::DefaultOutputLabel, WaterKind, PCGPinConstants::DefaultInputLabel);
    Edges.Append({ { Spline, WaterSurface }, { WaterSurface, WaterSampler }, { WaterSampler, WaterKind } });

    UPCGNode* ShoreKind = AddEdgeBranch(Graph, Spline, WaterSurface, Landscape, ShoreStepLocal, ShoreBandCm, TEXT("Shore"), 1, Edges);
    UPCGNode* LandKind = AddEdgeBranch(Graph, Spline, WaterSurface, Landscape, LandStepLocal, LandRingCm, TEXT("Land"), 2, Edges);
    if (!ShoreKind || !LandKind)
    {
        return -1;
    }

    UPCGSettings* WriteSettings = nullptr;
    UPCGNode* Write = Graph->AddNodeOfType(UPCGHerbalistWriteResourceSlotsSettings::StaticClass(), WriteSettings);
    if (!Write)
    {
        return -1;
    }
    Write->SetNodePosition(1900, 250);
    for (UPCGNode* Kind : { WaterKind, ShoreKind, LandKind })
    {
        Graph->AddLabeledEdge(Kind, PCGPinConstants::DefaultOutputLabel, Write, PCGPinConstants::DefaultInputLabel);
        Edges.Add({ Kind, Write });
    }

    // AddLabeledEdge не сообщает, удалась ли связь (неверная метка пина молчит)
    // -- проверяем каждую, иначе сохранился бы полусвязанный граф.
    for (const TPair<UPCGNode*, UPCGNode*>& Edge : Edges)
    {
        if (!HasSlotsEdge(Edge.Key, Edge.Value))
        {
            UE_LOG(LogTemp, Error, TEXT("Нет связи %s -> %s"), *Edge.Key->GetNodeTitle(EPCGNodeTitleType::ListView).ToString(),
                *Edge.Value->GetNodeTitle(EPCGNodeTitleType::ListView).ToString());
            return -1;
        }
    }
    return 1;
}

int32 UPcgResourceSlotsSetupCommandlet::EnsureSlotsComponent(UBlueprint* Blueprint, UPCGGraph* Graph)
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
        USCS_Node* Node = Construction->CreateNode(UPCGComponent::StaticClass(), TEXT("PCG_ResourceSlots"));
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

int32 UPcgResourceSlotsSetupCommandlet::Main(const FString& Params)
{
    UE_LOG(LogTemp, Display, TEXT("=== PcgResourceSlotsSetup ==="));

    UPCGGraph* Graph = LoadObject<UPCGGraph>(nullptr, SlotsGraphPath, nullptr, LOAD_NoWarn | LOAD_Quiet);
    if (!Graph)
    {
        UPackage* Package = CreatePackage(SlotsGraphPackage);
        Graph = NewObject<UPCGGraph>(Package, TEXT("PCG_ResourceSlots"), RF_Public | RF_Standalone);
        FAssetRegistryModule::AssetCreated(Graph);
    }
    const int32 GraphResult = BuildSlotsGraph(Graph);
    if (GraphResult < 0)
    {
        UE_LOG(LogTemp, Error, TEXT("Граф слотов не собрался -- %s не сохранён"), SlotsGraphPath);
        return 1;
    }
    if (GraphResult > 0 && !SaveSlotsSetupAsset(Graph))
    {
        UE_LOG(LogTemp, Error, TEXT("Не удалось сохранить %s"), SlotsGraphPath);
        return 1;
    }
    UE_LOG(LogTemp, Display, TEXT("PCG_ResourceSlots: %s"), GraphResult == 0 ? TEXT("уже собран") : TEXT("собран"));

    UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, WaterVolumeBlueprintPath);
    const int32 ComponentResult = EnsureSlotsComponent(Blueprint, Graph);
    if (ComponentResult < 0)
    {
        UE_LOG(LogTemp, Error, TEXT("PCG-компонент в %s не добавлен"), WaterVolumeBlueprintPath);
        return 1;
    }
    if (ComponentResult > 0)
    {
        FKismetEditorUtilities::CompileBlueprint(Blueprint);
        if (!SaveSlotsSetupAsset(Blueprint))
        {
            UE_LOG(LogTemp, Error, TEXT("Не удалось сохранить %s"), WaterVolumeBlueprintPath);
            return 1;
        }
    }
    UE_LOG(LogTemp, Display, TEXT("BP_WaterVolume: компонент слотов %s"), ComponentResult == 0 ? TEXT("уже есть") : TEXT("добавлен"));
    UE_LOG(LogTemp, Display, TEXT("=== PcgResourceSlotsSetup: готово ==="));
    return 0;
}
