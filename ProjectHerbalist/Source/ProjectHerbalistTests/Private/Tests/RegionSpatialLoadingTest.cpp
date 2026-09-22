// Source/ProjectHerbalistTests/Private/Tests/RegionSpatialLoadingTest.cpp
//
// Разметка мира, этап 5 (2026-09-12) -- регионы биомов без пространственной
// загрузки (решение пользователя 8, DESIGN_World_Layout.md §7). Клетки, а
// дальше страницы строят основу из регионов; регион, не загруженный в этот
// момент, молча выпал бы из расчёта.

#include "Core/World/BiomeRegionVolume.h"
#include "Core/World/WaterRegionVolume.h"
#include "Core/World/GridWorldManager.h"
#include "Core/Shrine/ShrineActor.h"
#include "Core/Storage/AlchemyTableActor.h"
#include "Misc/AutomationTest.h"
#include "Editor.h"
#include "Engine/World.h"
#include "WorldPartition/WorldPartition.h"
#include "WorldPartition/WorldPartitionHelpers.h"
#include "WorldPartition/WorldPartitionActorDescInstance.h"

#if WITH_AUTOMATION_TESTS && WITH_EDITOR

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistRegionLoading_RegionsAndManagerStayLoaded,
    "Herbalist.WorldLayout.Regions.RegionsAndManagerStayLoaded",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistRegionLoading_RegionsAndManagerStayLoaded::RunTest(const FString& Parameters)
{
    TestFalse(TEXT("Регион биома не загружается пространственно"), GetDefault<ABiomeRegionVolume>()->GetIsSpatiallyLoaded());
    TestFalse(TEXT("Регион воды наследует это"), GetDefault<AWaterRegionVolume>()->GetIsSpatiallyLoaded());

    // Найдено ревью: без запрета галку можно было включить обратно у одного
    // региона в панели деталей, и он снова выпал бы из расчёта.
    TestFalse(TEXT("Флаг региона в редакторе не меняется"), GetDefault<ABiomeRegionVolume>()->CanChangeIsSpatiallyLoadedFlag());
    TestFalse(TEXT("Флаг менеджера в редакторе не меняется"), GetDefault<AGridWorldManager>()->CanChangeIsSpatiallyLoadedFlag());

    // Котёл (2026-09-21) и капище (2026-09-22) регистрируются в симуляции в
    // BeginPlay -- выгрузка вдали уносила бы Домового, заложенное и капище из
    // условия Буяна.
    TestFalse(TEXT("Котёл не загружается пространственно"), GetDefault<AAlchemyTableActor>()->GetIsSpatiallyLoaded());
    TestFalse(TEXT("Капище не загружается пространственно"), GetDefault<AShrineActor>()->GetIsSpatiallyLoaded());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistRegionLoading_PlacedRegionDescriptorsAreNotSpatiallyLoaded,
    "Herbalist.WorldLayout.Regions.PlacedRegionDescriptorsAreNotSpatiallyLoaded",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistRegionLoading_PlacedRegionDescriptorsAreNotSpatiallyLoaded::RunTest(const FString& Parameters)
{
    // По дескрипторам World Partition -- тем, по которым движок строит ячейки
    // стриминга, а не по классу. Регионы на карте расставлены до этого этапа и
    // не пересохранялись: дескриптор хранит флаг дельтой к дескриптору класса,
    // и расставленные регионы обязаны уже быть непространственными (ревью).
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    UWorldPartition* WorldPartition = World->GetWorldPartition();
    const FString MapName = World->GetOutermost()->GetName();
    if (!WorldPartition)
    {
        AddInfo(FString::Printf(TEXT("Карта %s без World Partition -- дескрипторов нет"), *MapName));
        return true;
    }

    int32 RegionCount = 0;
    FWorldPartitionHelpers::ForEachActorDescInstance<ABiomeRegionVolume>(WorldPartition,
        [this, &RegionCount](const FWorldPartitionActorDescInstance* Desc)
        {
            ++RegionCount;
            TestFalse(FString::Printf(TEXT("Регион %s не загружается пространственно"), *Desc->GetActorLabelOrName().ToString()),
                Desc->GetIsSpatiallyLoaded());
            return true;
        });
    AddInfo(FString::Printf(TEXT("Карта %s: регионов в дескрипторах %d"), *MapName, RegionCount));

    // Капища и котлы, уже расставленные на карте, получают флаг из дескриптора
    // класса без пересохранения.
    int32 AlwaysLoadedCount = 0;
    auto CheckAlwaysLoaded = [this, &AlwaysLoadedCount](const FWorldPartitionActorDescInstance* Desc)
    {
        ++AlwaysLoadedCount;
        TestFalse(FString::Printf(TEXT("%s не загружается пространственно"), *Desc->GetActorLabelOrName().ToString()),
            Desc->GetIsSpatiallyLoaded());
        return true;
    };
    FWorldPartitionHelpers::ForEachActorDescInstance<AShrineActor>(WorldPartition, CheckAlwaysLoaded);
    FWorldPartitionHelpers::ForEachActorDescInstance<AAlchemyTableActor>(WorldPartition, CheckAlwaysLoaded);
    AddInfo(FString::Printf(TEXT("Карта %s: капищ и котлов в дескрипторах %d"), *MapName, AlwaysLoadedCount));

    // На L_TestDev расставлены два региона (DESIGN_World_Layout.md §1, ревью).
    if (MapName.EndsWith(TEXT("L_TestDev")))
    {
        TestTrue(TEXT("На L_TestDev дескрипторы регионов найдены"), RegionCount >= 2);
    }
    return true;
}

#endif // WITH_AUTOMATION_TESTS && WITH_EDITOR
