// PcgGrassSeasonSetupCommandlet.cpp

#include "PcgGrassSeasonSetupCommandlet.h"

#include "Core/PCG/PCGHerbalistSampleCell.h"
#include "PCGEdge.h"
#include "PCGGraph.h"
#include "PCGNode.h"
#include "PCGPin.h"
#include "UObject/Package.h"
#include "UObject/UnrealType.h"
#include "UObject/SavePackage.h"
#include "Misc/PackageName.h"

namespace
{
    // Классы узлов движка не экспортированы из модуля PCG (StaticClass не
    // слинкуется) -- сверяем по имени класса.
    bool IsPcgNodeOfClass(const UPCGNode* Node, const TCHAR* ClassName)
    {
        return Node && Node->GetSettings() && Node->GetSettings()->GetClass()->GetName() == ClassName;
    }

    const TCHAR* PcgGrassPath = TEXT("/Game/PCG/PCG_Grass.PCG_Grass");

    bool SavePcgGrassSeasonPackage(UPCGGraph* Graph)
    {
        UPackage* Package = Graph->GetOutermost();
        Package->MarkPackageDirty();
        const FString FileName = FPackageName::LongPackageNameToFilename(
            Package->GetName(), FPackageName::GetAssetPackageExtension());
        FSavePackageArgs Args;
        Args.TopLevelFlags = RF_Public | RF_Standalone;
        Args.SaveFlags = SAVE_NoError;
        return UPackage::SavePackage(Package, Graph, *FileName, Args);
    }
}

int32 UPcgGrassSeasonSetupCommandlet::InsertSeasonSampler(UPCGGraph* Graph)
{
    if (!Graph)
    {
        return -1;
    }

    for (const UPCGNode* Node : Graph->GetNodes())
    {
        if (Node && Node->GetSettings() && Node->GetSettings()->IsA<UPCGHerbalistSampleCellSettings>())
        {
            return 0;
        }
    }

    // Узел встаёт перед Attribute Noise, откуда бы тот ни получал точки: сначала
    // это был World Raycast, после исправления исключения покраски Ground --
    // фильтр слоя. Поиск по типам узлов, не по именам: имена меняются при
    // правке графа в редакторе.
    UPCGNode* Raycast = nullptr;   // узел перед шумом
    UPCGNode* Noise = nullptr;
    FName RaycastLabel;
    FName NoiseLabel;
    for (UPCGNode* Node : Graph->GetNodes())
    {
        if (!IsPcgNodeOfClass(Node, TEXT("PCGAttributeNoiseSettings")))
        {
            continue;
        }
        for (UPCGPin* Pin : Node->GetInputPins())
        {
            for (UPCGEdge* Edge : Pin->Edges)
            {
                UPCGPin* From = Edge ? Edge->InputPin.Get() : nullptr;
                if (From && From->Node)
                {
                    Raycast = From->Node;
                    Noise = Node;
                    RaycastLabel = From->Properties.Label;
                    NoiseLabel = Pin->Properties.Label;
                }
            }
        }
    }
    if (!Raycast || !Noise)
    {
        return -1;
    }

    UPCGSettings* SamplerSettings = nullptr;
    UPCGNode* Sampler = Graph->AddNodeOfType(UPCGHerbalistSampleCellSettings::StaticClass(), SamplerSettings);
    if (!Sampler)
    {
        return -1;
    }
    int32 RaycastX = 0, RaycastY = 0, NoiseX = 0, NoiseY = 0;
    Raycast->GetNodePosition(RaycastX, RaycastY);
    Noise->GetNodePosition(NoiseX, NoiseY);
    Sampler->SetNodePosition((RaycastX + NoiseX) / 2, (RaycastY + NoiseY) / 2 + 200);

    Graph->RemoveEdge(Raycast, RaycastLabel, Noise, NoiseLabel);
    Graph->AddLabeledEdge(Raycast, RaycastLabel, Sampler, PCGPinConstants::DefaultInputLabel);
    Graph->AddLabeledEdge(Sampler, PCGPinConstants::DefaultOutputLabel, Noise, NoiseLabel);

    // AddLabeledEdge не сообщает, удалась ли связь, -- проверяем по пинам, иначе
    // Main сохранил бы полусвязанный граф.
    auto HasEdge = [](const UPCGNode* From, const UPCGNode* To)
    {
        for (const UPCGPin* Pin : From->GetOutputPins())
            for (const UPCGEdge* Edge : Pin->Edges)
                if (Edge && Edge->OutputPin && Edge->OutputPin->Node == To) return true;
        return false;
    };
    if (!HasEdge(Raycast, Sampler) || !HasEdge(Sampler, Noise) || HasEdge(Raycast, Noise))
    {
        return -1;
    }
    return 1;
}

namespace
{
    UPCGNode* FindPcgNodeOfClass(UPCGGraph* Graph, const TCHAR* ClassName)
    {
        for (UPCGNode* Node : Graph->GetNodes())
        {
            if (IsPcgNodeOfClass(Node, ClassName)) return Node;
        }
        return nullptr;
    }

    // Узел, в который ведёт выход From (по классу), и метки пинов.
    UPCGNode* FindDownstream(UPCGNode* From, const TCHAR* ClassName, FName* OutFromLabel = nullptr, FName* OutToLabel = nullptr)
    {
        for (UPCGPin* Pin : From->GetOutputPins())
        {
            for (UPCGEdge* Edge : Pin->Edges)
            {
                UPCGPin* To = Edge ? Edge->OutputPin.Get() : nullptr;
                if (To && IsPcgNodeOfClass(To->Node, ClassName))
                {
                    if (OutFromLabel) *OutFromLabel = Pin->Properties.Label;
                    if (OutToLabel) *OutToLabel = To->Properties.Label;
                    return To->Node;
                }
            }
        }
        return nullptr;
    }

    UPCGNode* FindUpstream(UPCGNode* To, const TCHAR* ClassName, FName* OutFromLabel = nullptr, FName* OutToLabel = nullptr)
    {
        for (UPCGPin* Pin : To->GetInputPins())
        {
            for (UPCGEdge* Edge : Pin->Edges)
            {
                UPCGPin* From = Edge ? Edge->InputPin.Get() : nullptr;
                if (From && IsPcgNodeOfClass(From->Node, ClassName))
                {
                    if (OutFromLabel) *OutFromLabel = From->Properties.Label;
                    if (OutToLabel) *OutToLabel = Pin->Properties.Label;
                    return From->Node;
                }
            }
        }
        return nullptr;
    }

    bool ImportPcgSetting(UPCGSettings* Settings, const TCHAR* Name, const TCHAR* Value)
    {
        FProperty* Property = Settings ? Settings->GetClass()->FindPropertyByName(Name) : nullptr;
        return Property && Property->ImportText_InContainer(Value, Settings, Settings, PPF_None) != nullptr;
    }
}

namespace
{
    // Константа порога фильтра -- типа Float со значением FloatValue. Тип по
    // умолчанию у константы PCG -- Double: с ним фильтр сравнивал бы Ground с
    // DoubleValue (0), и «Ground < 0» убрал бы всю траву. true -- поменялось.
    bool EnsureFloatThreshold(UPCGSettings* FilterSettings, bool& bOutOk)
    {
        bOutOk = false;
        FStructProperty* Constant = FilterSettings ? CastField<FStructProperty>(FilterSettings->GetClass()->FindPropertyByName(TEXT("AttributeTypes"))) : nullptr;
        if (!Constant)
        {
            return false;
        }
        void* Value = Constant->ContainerPtrToValuePtr<void>(FilterSettings);
        const FEnumProperty* Type = CastField<FEnumProperty>(Constant->Struct->FindPropertyByName(TEXT("Type")));
        const FFloatProperty* FloatValue = CastField<FFloatProperty>(Constant->Struct->FindPropertyByName(TEXT("FloatValue")));
        if (!Type || !FloatValue)
        {
            return false;
        }
        FString CurrentType;
        Type->ExportTextItem_InContainer(CurrentType, Value, nullptr, nullptr, PPF_None);
        bOutOk = true;
        if (CurrentType == TEXT("Float"))
        {
            return false;
        }
        const float Threshold = FloatValue->GetPropertyValue_InContainer(Value);
        FilterSettings->Modify();
        bOutOk = Type->ImportText_InContainer(TEXT("Float"), Value, FilterSettings, PPF_None) != nullptr;
        // FloatValue уже хранит порог пользователя -- не трогаем.
        UE_LOG(LogTemp, Display, TEXT("Порог фильтра Ground: тип %s -> Float, значение %.3f"), *CurrentType, Threshold);
        return bOutOk;
    }
}

int32 UPcgGrassSeasonSetupCommandlet::FixGroundExclusion(UPCGGraph* Graph)
{
    if (!Graph)
    {
        return -1;
    }

    // Уже исправлено: Projection кормит фильтр атрибутов -- остаётся только
    // проверить тип порога (первая версия исправления его не ставила).
    if (UPCGNode* Projection = FindPcgNodeOfClass(Graph, TEXT("PCGProjectionSettings")))
    {
        if (UPCGNode* ExistingFilter = FindDownstream(Projection, TEXT("PCGAttributeFilteringSettings")))
        {
            bool bOk = false;
            const bool bChanged = EnsureFloatThreshold(ExistingFilter->GetSettings(), bOk);
            return !bOk ? -1 : (bChanged ? 1 : 0);
        }
    }

    UPCGNode* Filter = FindPcgNodeOfClass(Graph, TEXT("PCGAttributeFilteringSettings"));
    UPCGNode* Landscape = FindPcgNodeOfClass(Graph, TEXT("PCGGetLandscapeSettings"));
    UPCGNode* Raycast = FindPcgNodeOfClass(Graph, TEXT("PCGWorldRaycastElementSettings"));
    UPCGNode* Sampler = nullptr;
    for (UPCGNode* Node : Graph->GetNodes())
    {
        if (Node && Node->GetSettings() && Node->GetSettings()->IsA<UPCGHerbalistSampleCellSettings>()) Sampler = Node;
    }
    if (!Filter || !Landscape || !Raycast || !Sampler)
    {
        return -1;
    }

    // Старая ветка вычитания: Surface Sampler -> Transform Points -> фильтр ->
    // Filter Data (Spatial) -> Difference.Differences; Difference.Out -> дальше.
    UPCGNode* SurfaceSampler = FindDownstream(Landscape, TEXT("PCGSurfaceSamplerSettings"));
    UPCGNode* PreTransform = FindUpstream(Filter, TEXT("PCGTransformPointsSettings"));
    UPCGNode* SpatialFilter = FindDownstream(Filter, TEXT("PCGFilterByTypeSettings"));
    UPCGNode* SubtractNode = SpatialFilter ? FindDownstream(SpatialFilter, TEXT("PCGDifferenceSettings")) : nullptr;
    FName SubtractSourceFromLabel, SubtractSourceToLabel, SubtractOutLabel, AfterSubtractLabel;
    UPCGNode* SubtractSource = SubtractNode ? FindUpstream(SubtractNode, TEXT("PCGDifferenceSettings"), &SubtractSourceFromLabel, &SubtractSourceToLabel) : nullptr;
    UPCGNode* AfterSubtract = SubtractNode ? FindDownstream(SubtractNode, TEXT("PCGFilterByTypeSettings"), &SubtractOutLabel, &AfterSubtractLabel) : nullptr;
    if (!SurfaceSampler || !PreTransform || !SpatialFilter || !SubtractNode || !SubtractSource || !AfterSubtract)
    {
        return -1;
    }

    // Фильтр: постоянный порог (0.8 пользователя остаётся в AttributeTypes), «меньше».
    UPCGSettings* FilterSettings = Filter->GetSettings();
    bool bThresholdOk = false;
    EnsureFloatThreshold(FilterSettings, bThresholdOk);
    if (!ImportPcgSetting(FilterSettings, TEXT("bUseConstantThreshold"), TEXT("True"))
        || !ImportPcgSetting(FilterSettings, TEXT("Operator"), TEXT("Lesser"))
        || !bThresholdOk)
    {
        return -1;
    }

    // Projection: точки травы после World Raycast, цель -- данные ландшафта.
    UClass* ProjectionClass = LoadObject<UClass>(nullptr, TEXT("/Script/PCG.PCGProjectionSettings"));
    UPCGSettings* ProjectionSettings = nullptr;
    UPCGNode* Projection = ProjectionClass ? Graph->AddNodeOfType(ProjectionClass, ProjectionSettings) : nullptr;
    if (!Projection)
    {
        return -1;
    }
    // Позицию и поворот не трогать -- их уже поставил World Raycast; нужны
    // только атрибуты ландшафта (веса слоёв).
    if (!ImportPcgSetting(ProjectionSettings, TEXT("ProjectionParams"), TEXT("(bProjectPositions=False,bProjectRotations=False,bProjectScales=False)")))
    {
        Graph->RemoveNode(Projection);
        return -1;
    }

    int32 SamplerX = 0, SamplerY = 0;
    Sampler->GetNodePosition(SamplerX, SamplerY);
    Projection->SetNodePosition(SamplerX - 700, SamplerY + 350);
    Filter->SetNodePosition(SamplerX - 350, SamplerY + 350);

    FName RaycastOutLabel, SamplerInLabel;
    FindUpstream(Sampler, TEXT("PCGWorldRaycastElementSettings"), &RaycastOutLabel, &SamplerInLabel);
    if (RaycastOutLabel.IsNone())
    {
        return -1;
    }

    // Снять старую ветку целиком (узлы с их связями).
    Graph->RemoveNode(SurfaceSampler);
    Graph->RemoveNode(PreTransform);
    Graph->RemoveNode(SpatialFilter);
    Graph->RemoveNode(SubtractNode);
    Graph->AddLabeledEdge(SubtractSource, SubtractSourceFromLabel, AfterSubtract, AfterSubtractLabel);

    // Новая цепочка: Raycast -> Projection (+ ландшафт) -> фильтр -> Sample Herbalist Cell.
    Graph->RemoveEdge(Raycast, RaycastOutLabel, Sampler, SamplerInLabel);
    for (UPCGPin* Pin : Filter->GetInputPins())
    {
        TArray<UPCGEdge*> Edges = Pin->Edges;
        for (UPCGEdge* Edge : Edges)
        {
            if (Edge && Edge->InputPin && Edge->InputPin->Node)
            {
                Graph->RemoveEdge(Edge->InputPin->Node, Edge->InputPin->Properties.Label, Filter, Pin->Properties.Label);
            }
        }
    }
    Graph->AddLabeledEdge(Raycast, RaycastOutLabel, Projection, PCGPinConstants::DefaultInputLabel);
    Graph->AddLabeledEdge(Landscape, PCGPinConstants::DefaultOutputLabel, Projection, TEXT("Projection Target"));
    Graph->AddLabeledEdge(Projection, PCGPinConstants::DefaultOutputLabel, Filter, PCGPinConstants::DefaultInputLabel);
    Graph->AddLabeledEdge(Filter, TEXT("InsideFilter"), Sampler, SamplerInLabel);

    const bool bWired = FindDownstream(Raycast, TEXT("PCGProjectionSettings"))
        && FindDownstream(Landscape, TEXT("PCGProjectionSettings"))
        && FindDownstream(Projection, TEXT("PCGAttributeFilteringSettings"))
        && FindDownstream(SubtractSource, TEXT("PCGFilterByTypeSettings"))
        && !FindDownstream(Raycast, TEXT("PCGHerbalistSampleCellSettings"));
    bool bFilterFeedsSampler = false;
    for (UPCGPin* Pin : Filter->GetOutputPins())
        for (UPCGEdge* Edge : Pin->Edges)
            bFilterFeedsSampler |= Edge && Edge->OutputPin && Edge->OutputPin->Node == Sampler && Pin->Properties.Label == FName(TEXT("InsideFilter"));
    return bWired && bFilterFeedsSampler ? 1 : -1;
}

int32 UPcgGrassSeasonSetupCommandlet::Main(const FString& Params)
{
    UE_LOG(LogTemp, Display, TEXT("=== PcgGrassSeasonSetup ==="));
    UPCGGraph* Graph = LoadObject<UPCGGraph>(nullptr, PcgGrassPath);
    if (!Graph)
    {
        UE_LOG(LogTemp, Error, TEXT("Нет %s"), PcgGrassPath);
        return 1;
    }

    const int32 Result = InsertSeasonSampler(Graph);
    if (Result < 0)
    {
        UE_LOG(LogTemp, Error, TEXT("В %s нет связи World Raycast -> Attribute Noise или узел не связался -- граф не сохранён, вставить узел вручную"), PcgGrassPath);
        return 1;
    }
    UE_LOG(LogTemp, Display, TEXT("Sample Herbalist Cell: %s"), Result == 0 ? TEXT("уже в графе") : TEXT("вставлен"));

    const int32 GroundResult = FixGroundExclusion(Graph);
    if (GroundResult < 0)
    {
        UE_LOG(LogTemp, Error, TEXT("В %s не узнана ветка исключения покраски Ground -- граф не сохранён, править вручную"), PcgGrassPath);
        return 1;
    }
    UE_LOG(LogTemp, Display, TEXT("Исключение покраски Ground: %s"), GroundResult == 0 ? TEXT("уже исправлено") : TEXT("исправлено"));

    if (Result == 0 && GroundResult == 0)
    {
        UE_LOG(LogTemp, Display, TEXT("Менять нечего -- не сохраняю."));
        return 0;
    }
    if (!SavePcgGrassSeasonPackage(Graph))
    {
        UE_LOG(LogTemp, Error, TEXT("Не удалось сохранить %s"), PcgGrassPath);
        return 1;
    }
    UE_LOG(LogTemp, Display, TEXT("=== PcgGrassSeasonSetup: %s сохранён ==="), PcgGrassPath);
    return 0;
}
