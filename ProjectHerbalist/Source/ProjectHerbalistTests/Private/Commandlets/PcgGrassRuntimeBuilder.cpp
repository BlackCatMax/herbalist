// PcgGrassRuntimeBuilder.cpp

#include "PcgGrassRuntimeBuilder.h"

#include "Core/World/BiomeRegionVolume.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/Blueprint.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/World.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "PCGComponent.h"
#include "PCGGraph.h"
#include "UObject/Package.h"
#include "UObject/UnrealType.h"
#include "UObject/SavePackage.h"
#include "Misc/PackageName.h"
#include "WorldPartition/WorldPartition.h"
#include "WorldPartition/WorldPartitionHelpers.h"
#include "WorldPartition/WorldPartitionActorDescInstance.h"

namespace
{
    const TCHAR* PcgGrassRuntimeGraphPath = TEXT("/Game/PCG/PCG_Grass.PCG_Grass");
    const TCHAR* PcgGrassRuntimeBlueprintPath = TEXT("/Game/Blueprints/BP_BiomeVolume.BP_BiomeVolume");

    bool UsesGrassGraph(const UPCGComponent* Component)
    {
        const UPCGGraph* Graph = Component ? Component->GetGraph() : nullptr;
        return Graph && Graph->GetPathName() == PcgGrassRuntimeGraphPath;
    }

    bool SavePcgGrassRuntimeBlueprint(UBlueprint* Blueprint)
    {
        UPackage* Package = Blueprint->GetOutermost();
        Package->MarkPackageDirty();
        const FString FileName = FPackageName::LongPackageNameToFilename(
            Package->GetName(), FPackageName::GetAssetPackageExtension());
        FSavePackageArgs Args;
        Args.TopLevelFlags = RF_Public | RF_Standalone;
        Args.SaveFlags = SAVE_NoError;
        return UPackage::SavePackage(Package, Blueprint, *FileName, Args);
    }
}

UPcgGrassRuntimeBuilder::UPcgGrassRuntimeBuilder(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
}

bool UPcgGrassRuntimeBuilder::ApplyRuntimeGeneration(UPCGComponent* Component)
{
    if (!Component)
    {
        return false;
    }
    bool bChanged = false;
    if (Component->GenerationTrigger != EPCGComponentGenerationTrigger::GenerateAtRuntime)
    {
        Component->GenerationTrigger = EPCGComponentGenerationTrigger::GenerateAtRuntime;
        bChanged = true;
    }
    if (FBoolProperty* Partitioned = FindFProperty<FBoolProperty>(UPCGComponent::StaticClass(), TEXT("bIsComponentPartitioned")))
    {
        if (!Partitioned->GetPropertyValue_InContainer(Component))
        {
            Partitioned->SetPropertyValue_InContainer(Component, true);
            bChanged = true;
        }
    }
    return bChanged;
}

namespace
{
    bool ImportIfDifferent(UObject* Object, const TCHAR* Name, const TCHAR* Value, bool& bOutChanged)
    {
        FProperty* Property = Object ? Object->GetClass()->FindPropertyByName(Name) : nullptr;
        if (!Property)
        {
            return false;
        }
        FString Current;
        Property->ExportTextItem_InContainer(Current, Object, nullptr, nullptr, PPF_None);
        if (Current == Value)
        {
            return true;
        }
        Object->Modify();
        if (!Property->ImportText_InContainer(Value, Object, Object, PPF_None))
        {
            return false;
        }
        bOutChanged = true;
        return true;
    }

    bool SavePcgGrassRuntimeAsset(UObject* Asset)
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
}

bool UPcgGrassRuntimeBuilder::ApplyRuntimeGrid(UPCGGraph* Graph)
{
    bool bChanged = false;
    const bool bOk = ImportIfDifferent(Graph, TEXT("bUseHierarchicalGeneration"), TEXT("True"), bChanged)
        && ImportIfDifferent(Graph, TEXT("HiGenGridSize"), TEXT("Grid64"), bChanged);
    return bOk && bChanged;
}

bool UPcgGrassRuntimeBuilder::ApplyLandscapeCacheForRuntime(AActor* PcgWorldActor)
{
    const FObjectPropertyBase* CacheProperty = PcgWorldActor
        ? FindFProperty<FObjectPropertyBase>(PcgWorldActor->GetClass(), TEXT("LandscapeCacheObject")) : nullptr;
    UObject* Cache = CacheProperty ? CacheProperty->GetObjectPropertyValue_InContainer(PcgWorldActor) : nullptr;
    if (!Cache)
    {
        return false;
    }
    bool bChanged = false;
    ImportIfDifferent(Cache, TEXT("SerializationMode"), TEXT("SerializeOnlyAtCook"), bChanged);
    ImportIfDifferent(Cache, TEXT("CookedSerializedContents"), TEXT("SerializeAll"), bChanged);
    return bChanged;
}

bool UPcgGrassRuntimeBuilder::RunInternal(UWorld* World, const FCellInfo& InCellInfo, FPackageSourceControlHelper& PackageHelper)
{
    UE_LOG(LogTemp, Display, TEXT("=== PcgGrassRuntimeBuilder ==="));
    const bool bReportOnly = HasParam(TEXT("ReportOnly"));

    UWorldPartition* WorldPartition = World ? World->GetWorldPartition() : nullptr;
    if (!WorldPartition)
    {
        UE_LOG(LogTemp, Error, TEXT("Карта без World Partition"));
        return false;
    }

    // ---- 1. Шаблон в Blueprint ----
    if (!bReportOnly)
    {
        UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, PcgGrassRuntimeBlueprintPath);
        if (!Blueprint || !Blueprint->SimpleConstructionScript)
        {
            UE_LOG(LogTemp, Error, TEXT("Нет %s"), PcgGrassRuntimeBlueprintPath);
            return false;
        }
        bool bTemplateChanged = false;
        for (USCS_Node* Node : Blueprint->SimpleConstructionScript->GetAllNodes())
        {
            UPCGComponent* Template = Node ? Cast<UPCGComponent>(Node->ComponentTemplate) : nullptr;
            if (UsesGrassGraph(Template))
            {
                Template->Modify();
                bTemplateChanged |= ApplyRuntimeGeneration(Template);
            }
        }
        if (bTemplateChanged)
        {
            FKismetEditorUtilities::CompileBlueprint(Blueprint);
            if (!SavePcgGrassRuntimeBlueprint(Blueprint))
            {
                UE_LOG(LogTemp, Error, TEXT("Не удалось сохранить %s"), PcgGrassRuntimeBlueprintPath);
                return false;
            }
            UE_LOG(LogTemp, Display, TEXT("Шаблон PCG в BP_BiomeVolume: GenerateAtRuntime, разбиение на ячейки"));
        }
        else
        {
            UE_LOG(LogTemp, Display, TEXT("Шаблон PCG в BP_BiomeVolume уже в рантайме"));
        }
    }

    // ---- 2. Сетка генерации графа ----
    if (!bReportOnly)
    {
        UPCGGraph* Graph = LoadObject<UPCGGraph>(nullptr, PcgGrassRuntimeGraphPath);
        if (!Graph)
        {
            UE_LOG(LogTemp, Error, TEXT("Нет %s"), PcgGrassRuntimeGraphPath);
            return false;
        }
        if (ApplyRuntimeGrid(Graph))
        {
            if (!SavePcgGrassRuntimeAsset(Graph))
            {
                UE_LOG(LogTemp, Error, TEXT("Не удалось сохранить %s"), PcgGrassRuntimeGraphPath);
                return false;
            }
            UE_LOG(LogTemp, Display, TEXT("PCG_Grass: иерархическая генерация, сетка 64 м"));
        }
        else if (!Graph->IsHierarchicalGenerationEnabled())
        {
            UE_LOG(LogTemp, Error, TEXT("PCG_Grass: иерархическую генерацию включить не удалось"));
            return false;
        }
    }

    // ---- 3, 4. PCG World Actor и экземпляры на карте ----
    UClass* PcgWorldActorClass = LoadObject<UClass>(nullptr, TEXT("/Script/PCG.PCGWorldActor"));
    int32 WorldActorsFound = 0;
    TArray<UPackage*> PackagesToSave;
    int32 ComponentsFound = 0;
    int64 TotalInstances = 0;
    double TotalAreaM2 = 0.0;

    FWorldPartitionHelpers::FForEachActorWithLoadingParams Params;
    Params.ActorClasses = { ABiomeRegionVolume::StaticClass() };
    if (PcgWorldActorClass)
    {
        Params.ActorClasses.Add(PcgWorldActorClass);
    }
    Params.OnPreGarbageCollect = [&PackagesToSave, &PackageHelper]()
    {
        UWorldPartitionBuilder::SavePackages(PackagesToSave, PackageHelper, /*bErrorsAsWarnings=*/true);
        PackagesToSave.Empty();
    };

    FWorldPartitionHelpers::ForEachActorWithLoading(WorldPartition,
        [bReportOnly, PcgWorldActorClass, &WorldActorsFound, &PackagesToSave, &ComponentsFound, &TotalInstances, &TotalAreaM2](const FWorldPartitionActorDescInstance* ActorDescInstance)
    {
        AActor* Actor = ActorDescInstance->GetActor();
        if (!Actor)
        {
            return true;
        }
        if (PcgWorldActorClass && Actor->IsA(PcgWorldActorClass))
        {
            ++WorldActorsFound;
            if (!bReportOnly && ApplyLandscapeCacheForRuntime(Actor))
            {
                Actor->Modify();
                PackagesToSave.AddUnique(Actor->GetExternalPackage() ? Actor->GetExternalPackage() : Actor->GetPackage());
                UE_LOG(LogTemp, Display, TEXT("  %s: кэш ландшафта SerializeOnlyAtCook"), *Actor->GetName());
            }
            return true;
        }
        TArray<UPCGComponent*> Components;
        Actor->GetComponents(Components);
        for (UPCGComponent* Component : Components)
        {
            if (!UsesGrassGraph(Component))
            {
                continue;
            }
            ++ComponentsFound;

            // Замер запечённой травы: экземпляры ISM на акторе и площадь
            // последней генерации.
            int64 Instances = 0;
            TArray<UInstancedStaticMeshComponent*> Ism;
            Actor->GetComponents(Ism);
            for (const UInstancedStaticMeshComponent* Mesh : Ism)
            {
                Instances += Mesh->GetInstanceCount();
            }
            const FBox Bounds = Component->GetLastGeneratedBounds();
            const double AreaM2 = Bounds.IsValid ? (Bounds.GetSize().X / 100.0) * (Bounds.GetSize().Y / 100.0) : 0.0;
            TotalInstances += Instances;
            TotalAreaM2 += AreaM2;
            UE_LOG(LogTemp, Display, TEXT("  %s: режим %s, запечено %lld экземпляров ISM на %.0f м² (%.2f на м²)"),
                *Actor->GetActorLabel(), *UEnum::GetValueAsString(Component->GenerationTrigger), Instances, AreaM2,
                AreaM2 > 0.0 ? Instances / AreaM2 : 0.0);

            if (bReportOnly)
            {
                continue;
            }
            Actor->Modify();
            Component->Modify();
            if (Component->bGenerated || Instances > 0)
            {
                Component->CleanupLocalImmediate(/*bRemoveComponents=*/true);
            }
            ApplyRuntimeGeneration(Component);
            PackagesToSave.AddUnique(Actor->GetExternalPackage() ? Actor->GetExternalPackage() : Actor->GetPackage());
            UE_LOG(LogTemp, Display, TEXT("    запечённое очищено, режим GenerateAtRuntime, разбиение на ячейки"));
        }
        return true;
    }, Params);

    if (!PackagesToSave.IsEmpty())
    {
        UWorldPartitionBuilder::SavePackages(PackagesToSave, PackageHelper, /*bErrorsAsWarnings=*/true);
    }

    UE_LOG(LogTemp, Display, TEXT("Компонентов травы: %d; запечено всего %lld экземпляров на %.0f м²; PCG World Actor: %d"),
        ComponentsFound, TotalInstances, TotalAreaM2, WorldActorsFound);
    if (WorldActorsFound == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("PCG World Actor на карте не найден -- кэш ландшафта в рантайме не настроен"));
    }
    if (ComponentsFound == 0)
    {
        UE_LOG(LogTemp, Error, TEXT("PCG-компонентов с PCG_Grass на карте не найдено"));
        return false;
    }
    UE_LOG(LogTemp, Display, TEXT("=== PcgGrassRuntimeBuilder: готово ==="));
    return true;
}
