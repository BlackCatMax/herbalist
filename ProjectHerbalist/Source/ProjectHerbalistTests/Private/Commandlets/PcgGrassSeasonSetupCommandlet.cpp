// PcgGrassSeasonSetupCommandlet.cpp

#include "PcgGrassSeasonSetupCommandlet.h"

#include "Core/PCG/PCGHerbalistSampleCell.h"
#include "PCGEdge.h"
#include "PCGGraph.h"
#include "PCGNode.h"
#include "PCGPin.h"
#include "UObject/Package.h"
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

    // Ищем связь по типам узлов, не по именам: имена узлов меняются при правке
    // графа в редакторе.
    UPCGNode* Raycast = nullptr;
    UPCGNode* Noise = nullptr;
    FName RaycastLabel;
    FName NoiseLabel;
    for (UPCGNode* Node : Graph->GetNodes())
    {
        if (!IsPcgNodeOfClass(Node, TEXT("PCGWorldRaycastElementSettings")))
        {
            continue;
        }
        for (UPCGPin* Pin : Node->GetOutputPins())
        {
            for (UPCGEdge* Edge : Pin->Edges)
            {
                UPCGPin* To = Edge ? Edge->OutputPin.Get() : nullptr;
                if (To && IsPcgNodeOfClass(To->Node, TEXT("PCGAttributeNoiseSettings")))
                {
                    Raycast = Node;
                    Noise = To->Node;
                    RaycastLabel = Pin->Properties.Label;
                    NoiseLabel = To->Properties.Label;
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
    if (Result == 0)
    {
        UE_LOG(LogTemp, Display, TEXT("Sample Herbalist Cell уже в графе -- не трогаю."));
        return 0;
    }
    if (Result < 0)
    {
        UE_LOG(LogTemp, Error, TEXT("В %s нет связи World Raycast -> Attribute Noise или узел не связался -- граф не сохранён, вставить узел вручную"), PcgGrassPath);
        return 1;
    }
    if (!SavePcgGrassSeasonPackage(Graph))
    {
        UE_LOG(LogTemp, Error, TEXT("Не удалось сохранить %s"), PcgGrassPath);
        return 1;
    }
    UE_LOG(LogTemp, Display, TEXT("=== PcgGrassSeasonSetup: узел вставлен, %s сохранён ==="), PcgGrassPath);
    return 0;
}
