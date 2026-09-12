// WorldLayoutSyncBuilder.cpp

#include "WorldLayoutSyncBuilder.h"

#include "Core/World/GridWorldManager.h"
#include "Core/World/WorldLayout.h"
#include "Engine/World.h"
#include "UObject/Package.h"
#include "WorldPartition/WorldPartition.h"
#include "WorldPartition/WorldPartitionHelpers.h"
#include "WorldPartition/WorldPartitionActorDescInstance.h"

UWorldLayoutSyncBuilder::UWorldLayoutSyncBuilder(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
}

bool UWorldLayoutSyncBuilder::RunInternal(UWorld* World, const FCellInfo& InCellInfo, FPackageSourceControlHelper& PackageHelper)
{
    UE_LOG(LogTemp, Display, TEXT("=== WorldLayoutSyncBuilder ==="));

    UWorldPartition* WorldPartition = World ? World->GetWorldPartition() : nullptr;
    if (!WorldPartition)
    {
        UE_LOG(LogTemp, Error, TEXT("Карта без World Partition -- менеджер правится кнопкой «Сверить с World Partition» в редакторе"));
        return false;
    }

    const bool bReportOnly = HasParam(TEXT("ReportOnly"));
    TArray<UPackage*> PackagesToSave;
    int32 ManagersFound = 0;

    FWorldPartitionHelpers::FForEachActorWithLoadingParams Params;
    Params.ActorClasses = { AGridWorldManager::StaticClass() };
    Params.OnPreGarbageCollect = [&PackagesToSave, &PackageHelper]()
    {
        UWorldPartitionBuilder::SavePackages(PackagesToSave, PackageHelper, /*bErrorsAsWarnings=*/true);
        PackagesToSave.Empty();
    };

    FWorldPartitionHelpers::ForEachActorWithLoading(WorldPartition,
        [World, bReportOnly, &PackagesToSave, &ManagersFound](const FWorldPartitionActorDescInstance* ActorDescInstance)
    {
        AGridWorldManager* Manager = Cast<AGridWorldManager>(ActorDescInstance->GetActor());
        if (!Manager)
        {
            return true;
        }
        ++ManagersFound;

        UE_LOG(LogTemp, Display, TEXT("Менеджер %s: сейчас сетка %dx%d по %.0f см от (%.0f, %.0f)"),
            *Manager->GetName(), Manager->GridSizeX, Manager->GridSizeY, Manager->CellSize,
            Manager->GetGridOrigin().X, Manager->GetGridOrigin().Y);

        TArray<FString> Warnings;
        const FHerbalistWorldLayoutSource Source = AGridWorldManager::GatherWorldLayoutSource(World, Warnings);
        UE_LOG(LogTemp, Display,
            TEXT("  ландшафт: %s, квад %.0f см, компонент %d, вершина (0,0) (%.0f, %.0f), границы (%.0f, %.0f)..(%.0f, %.0f)"),
            Source.bHasLandscape ? TEXT("есть") : TEXT("нет"), Source.QuadSizeCm, Source.ComponentSizeQuads,
            Source.LandscapeOrigin.X, Source.LandscapeOrigin.Y,
            Source.LandscapeMin.X, Source.LandscapeMin.Y, Source.LandscapeMax.X, Source.LandscapeMax.Y);
        UE_LOG(LogTemp, Display,
            TEXT("  стриминг: %s, разбиение %s, ячейка %.0f см, дальность %.0f см, начало (%.0f, %.0f)"),
            Source.bHasStreamingGrid ? TEXT("есть") : TEXT("нет"), *Source.StreamingGridName.ToString(),
            Source.StreamingCellSizeCm, Source.StreamingLoadingRangeCm,
            Source.StreamingGridOrigin.X, Source.StreamingGridOrigin.Y);

        if (bReportOnly)
        {
            Manager->BakedLayoutSource = Source;
            Manager->ResolveWorldLayout(Warnings);
            UE_LOG(LogTemp, Display, TEXT("  разметка (без сохранения): %s"), *FWorldLayoutSolver::Describe(Manager->ResolvedLayout));
        }
        else
        {
            Warnings.Reset();
            const bool bChanged = Manager->SyncWorldLayoutFromWorld(World, Warnings, /*bMarkModified=*/true);
            UE_LOG(LogTemp, Display, TEXT("  разметка%s: %s"), bChanged ? TEXT(" изменилась") : TEXT(" не изменилась"),
                *FWorldLayoutSolver::Describe(Manager->ResolvedLayout));
            if (bChanged)
            {
                PackagesToSave.Add(Manager->GetExternalPackage() ? Manager->GetExternalPackage() : Manager->GetPackage());
            }
        }

        for (const FString& Warning : Warnings)
        {
            UE_LOG(LogTemp, Warning, TEXT("  %s"), *Warning);
        }
        return true;
    }, Params);

    if (!PackagesToSave.IsEmpty())
    {
        UWorldPartitionBuilder::SavePackages(PackagesToSave, PackageHelper, /*bErrorsAsWarnings=*/true);
    }

    if (ManagersFound == 0)
    {
        UE_LOG(LogTemp, Error, TEXT("Менеджер сетки на карте не найден"));
        return false;
    }
    UE_LOG(LogTemp, Display, TEXT("Менеджеров: %d%s"), ManagersFound, bReportOnly ? TEXT(" (только отчёт)") : TEXT(""));
    return true;
}
