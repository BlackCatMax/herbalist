// Source/ProjectHerbalistTests/Private/Tests/EntityActorTest.cpp
//
// Родительские классы сущностей бестиария (2026-08-30, "заводим родительские
// классы для сущностей и связки") — AHerbalistEntityActor и три под-класса
// по рангу (AAmbientEntityActor/ALandmarkEntityActor/ALegendaryEntityActor),
// AGridWorldManager::SyncManifestedEntityActor. Существующие тесты
// (AmbientEntityTest.cpp/LandmarkTest.cpp/LegendaryEntityTest.cpp) уже
// проверяют, что Cell.ManifestedEntityID/TargetState считаются правильно —
// здесь проверяется НОВОЕ: что физический актор реально появляется/исчезает
// синхронно с этим ID, для всех трёх рангов, плюс что старые интерактивные
// акторы (StorageContainer/AlchemyTableActor/MemoryFragmentActor) остались
// совместимы с новым общим IInteractable.

#include "Core/World/GridWorldManager.h"
#include "Core/Entities/HerbalistEntityActor.h"
#include "Core/Entities/AmbientEntityActor.h"
#include "Core/Entities/LandmarkEntityActor.h"
#include "Core/Entities/LegendaryEntityActor.h"
#include "Core/Entities/LegendaryEntityTypes.h"
#include "Core/Interaction/Interactable.h"
#include "Core/Storage/StorageContainer.h"
#include "Core/Storage/AlchemyTableActor.h"
#include "Core/Zaryana/MemoryFragmentActor.h"
#include "Core/BiomeGraph/BiomeGraphSubsystem.h"
#include "Core/BiomeGraph/BiomeGraphAsset.h"
#include "Core/Types/BiomeTypes.h"
#include "Misc/AutomationTest.h"
#include "Editor.h"
#include "Engine/World.h"

#if WITH_AUTOMATION_TESTS && WITH_EDITOR

#include "TestWorldHelpers.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistEntityActor_AmbientManifestationSpawnsAndDespawnsActor,
    "Herbalist.EntityActor.AmbientManifestationSpawnsAndDespawnsActor",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistEntityActor_AmbientManifestationSpawnsAndDespawnsActor::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;

    FGridCell* Cell = Manager->GetCell(0, 0);
    if (!TestNotNull(TEXT("Cell (0,0) exists"), Cell)) { Manager->Destroy(); return false; }
    Cell->Biome = EBiomeType::Bog;
    Cell->bIsWater = false;
    Cell->State.Meta.Corruption = 0.8f;   // выше порога Гнильников (0.6)
    Cell->TargetState.Meta.Purity = 0.5f;
    Manager->SetGameClockSeconds(10.0f * 60.0f);   // День, не Рассвет/Ночь

    Manager->UpdateEntityManifestations(1.0f);
    TestEqual(TEXT("Гнильники manifest"), Cell->ManifestedEntityID, FName(TEXT("Гнильники")));

    AHerbalistEntityActor* Spawned = Cell->ManifestedEntityActor.Get();
    if (TestNotNull(TEXT("Actor spawned for the manifested entity"), Spawned))
    {
        TestEqual(TEXT("Spawned actor's EntityID matches"), Spawned->GetEntityID(), FName(TEXT("Гнильники")));
        TestTrue(TEXT("Spawned actor is the Ambient-tier class"), Spawned->IsA(AAmbientEntityActor::StaticClass()));
        TestEqual(TEXT("Spawned actor's GridCell matches"), Spawned->GetGridCell(), FIntPoint(0, 0));
    }

    // Условие больше не выполняется -- актор должен исчезнуть вместе с ID.
    // Stability явно выше порога Ржавых духов (тоже Болото, Stability < 0.3)
    // -- иначе дефолтная Stability=0.0 сама подхватила бы клетку другим
    // Низшим, и тест проверял бы переключение актора, а не его исчезновение.
    Cell->State.Meta.Corruption = 0.1f;
    Cell->State.Meta.Stability = 0.5f;
    Manager->UpdateEntityManifestations(1.0f);
    TestEqual(TEXT("Гнильники stop manifesting"), Cell->ManifestedEntityID, NAME_None);
    TestFalse(TEXT("Actor reference cleared after despawn"), Cell->ManifestedEntityActor.IsValid());

    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistEntityActor_LandmarkManifestationSpawnsActor,
    "Herbalist.EntityActor.LandmarkManifestationSpawnsActor",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistEntityActor_LandmarkManifestationSpawnsActor::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;

    FGridCell* Cell = Manager->GetCell(2, 2);
    if (!TestNotNull(TEXT("Cell (2,2) exists"), Cell)) { Manager->Destroy(); return false; }
    Cell->Biome = EBiomeType::ForestSteppe;
    Cell->bIsWater = false;

    FEntityLandmark Landmark;
    Landmark.EntityID = FName(TEXT("Полевик"));
    Landmark.Cell = FIntPoint(2, 2);
    Landmark.Respect = 0.8f;   // выше порога благословения (0.5)
    Manager->SetEntityLandmarks({ Landmark });

    Manager->UpdateEntityManifestations(1.0f);
    TestEqual(TEXT("Полевик manifests"), Cell->ManifestedEntityID, FName(TEXT("Полевик")));

    ALandmarkEntityActor* Spawned = Cast<ALandmarkEntityActor>(Cell->ManifestedEntityActor.Get());
    if (TestNotNull(TEXT("Landmark-tier actor spawned"), Spawned))
    {
        TestEqual(TEXT("Spawned actor's EntityID matches"), Spawned->GetEntityID(), FName(TEXT("Полевик")));
        const FEntityLandmark* Found = Spawned->GetLandmark();
        if (TestNotNull(TEXT("GetLandmark() finds the underlying FEntityLandmark"), Found))
        {
            TestEqual(TEXT("GetLandmark() reflects the same Respect"), Found->Respect, Landmark.Respect);
        }
    }

    Manager->Destroy();
    return true;
}

namespace
{
    UBiomeGraphSubsystem* InitGraphForEntityActorTest(UWorld* World)
    {
        UBiomeGraphSubsystem* Graph = World->GetSubsystem<UBiomeGraphSubsystem>();
        if (!Graph) return nullptr;
        UBiomeGraphAsset* Asset = LoadObject<UBiomeGraphAsset>(nullptr, TEXT("/Game/Data/DA_BiomeGraph"));
        if (!Asset) return nullptr;
        Graph->InitializeFromAsset(Asset);
        return Graph;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistEntityActor_LegendaryManifestationSpawnsActor,
    "Herbalist.EntityActor.LegendaryManifestationSpawnsActor",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistEntityActor_LegendaryManifestationSpawnsActor::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("Manager spawned"), Manager)) return false;
    Manager->SetGameClockSeconds(10.0f * 60.0f);

    UBiomeGraphSubsystem* Graph = InitGraphForEntityActorTest(World);
    if (!TestNotNull(TEXT("Graph initialized from DA_BiomeGraph"), Graph))
    {
        Manager->Destroy();
        return false;
    }

    const FIntPoint* Anchor = Manager->GetLegendaryAnchors().Find(FName(TEXT("Болотный царь")));
    if (!TestNotNull(TEXT("Болотный царь has a seeded anchor cell"), Anchor))
    {
        Graph->Deinitialize();
        Manager->Destroy();
        return false;
    }

    const FName BogID = FBiomeDefaults::BiomeTypeToName(EBiomeType::Bog);
    FBiomeGraphNode* Node = Graph->GetMutableNode(BogID);
    if (!TestNotNull(TEXT("Bog node exists in the graph"), Node))
    {
        Graph->Deinitialize();
        Manager->Destroy();
        return false;
    }

    Node->MorokField = 0.9f;   // спайк -- должен триггерить Malign-полюс
    Manager->UpdateEntityManifestations(1.0f);

    FGridCell* Cell = Manager->GetCell(Anchor->X, Anchor->Y);
    TestEqual(TEXT("Болотный царь manifests"), Cell->ManifestedEntityID, FName(TEXT("Болотный царь")));

    AHerbalistEntityActor* Spawned = Cell->ManifestedEntityActor.Get();
    if (TestNotNull(TEXT("Legendary-tier actor spawned"), Spawned))
    {
        TestTrue(TEXT("Spawned actor is the Legendary-tier class"), Spawned->IsA(ALegendaryEntityActor::StaticClass()));
        TestEqual(TEXT("Spawned actor's EntityID matches"), Spawned->GetEntityID(), FName(TEXT("Болотный царь")));
    }

    Graph->Deinitialize();
    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistEntityActor_ExistingInteractablesImplementTheSharedInterface,
    "Herbalist.EntityActor.ExistingInteractablesImplementTheSharedInterface",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistEntityActor_ExistingInteractablesImplementTheSharedInterface::RunTest(const FString& Parameters)
{
    // Статическая проверка (без спавна) -- ровно то, на что опирается
    // AHerbalistPlayerController::Interact() (Implements<UInteractable>()).
    TestTrue(TEXT("AStorageContainer implements IInteractable"),
        AStorageContainer::StaticClass()->ImplementsInterface(UInteractable::StaticClass()));
    TestTrue(TEXT("AAlchemyTableActor implements IInteractable"),
        AAlchemyTableActor::StaticClass()->ImplementsInterface(UInteractable::StaticClass()));
    TestTrue(TEXT("AMemoryFragmentActor implements IInteractable"),
        AMemoryFragmentActor::StaticClass()->ImplementsInterface(UInteractable::StaticClass()));
    TestTrue(TEXT("AHerbalistEntityActor (and its rank subclasses) implement IInteractable"),
        AHerbalistEntityActor::StaticClass()->ImplementsInterface(UInteractable::StaticClass()));
    return true;
}

#endif // WITH_AUTOMATION_TESTS && WITH_EDITOR
