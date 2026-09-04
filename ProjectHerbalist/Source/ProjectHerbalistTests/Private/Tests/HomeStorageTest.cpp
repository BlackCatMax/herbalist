// Source/ProjectHerbalistTests/Private/Tests/HomeStorageTest.cpp
//
// Домашние хранилища (DESIGN_Community_And_Homestead.md §2.2, 2026-09-04,
// прямой запрос пользователя): "домашние хранилища — это НЕ ниша сада, а
// буквальное расширение дома... погреб условно выкопать надо", при этом
// сама постройка МГНОВЕННА при выполнении условий (тот же принцип "мягкой
// прокачки" §2.2 — материалы + Respect хозяина, не число опыта), не
// растянутый во времени процесс.
//
// Два уровня, тот же приём границы, что уже PlantSeedInCell/PlantSeed и
// ApplyFertilizerToCell/ApplyFertilizer:
//   - AGridWorldManager::SpawnHomeStorageContainer -- только сам эффект
//     (спавн AStorageContainer у клетки), НЕ трогает GameInstance,
//     тестируется напрямую.
//   - AHerbalistPlayerController::BuildHomeStorage -- резолв владения
//     (Respect Домового через FindLandmarkAt, материал через прямой поиск
//     в инвентаре по фиксированному ID "broad_10"). В отличие от
//     PlantSeed/ActivateWard/EquipContainer, здесь НЕТ резолва произвольной
//     строки через IngredientRegistrySubsystem вовсе (материал -- константа
//     кода, тот же приём, что уже ApplyFertilizer/PeregnoyIngredientID) --
//     поэтому, в отличие от того класса пробела, BuildHomeStorage тоже
//     напрямую тестируем без GameInstance.

#include "Core/World/GridWorldManager.h"
#include "Core/Storage/StorageContainer.h"
#include "Core/Storage/AlchemyTableActor.h"
#include "Core/Inventory/HerbalistInventoryComponent.h"
#include "Core/Config/HerbalistSettings.h"
#include "Player/HerbalistPlayerController.h"
#include "Misc/AutomationTest.h"
#include "Editor.h"
#include "Engine/World.h"
#include "EngineUtils.h"

#if WITH_AUTOMATION_TESTS && WITH_EDITOR

#include "TestWorldHelpers.h"

namespace
{
    int32 CountStorageContainersOfType(UWorld* World, EStorageContainerType Type)
    {
        int32 Count = 0;
        for (TActorIterator<AStorageContainer> It(World); It; ++It)
        {
            AStorageContainer* Container = *It;
            if (Container && Container->InventoryComponent && Container->InventoryComponent->ContainerType == Type)
            {
                ++Count;
            }
        }
        return Count;
    }
}

// ---------------------------------------------------------------------------
// SpawnHomeStorageContainer -- не трогает GameInstance (см. довод в
// GridWorldManager.h), напрямую тестируема, тот же приём, что и
// PlantSeedInCell/ApplyFertilizerToCell.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistHomeStorage_SpawnCreatesContainerWithRequestedType,
    "Herbalist.HomeStorage.SpawnCreatesContainerWithRequestedType",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistHomeStorage_SpawnCreatesContainerWithRequestedType::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;

    AStorageContainer* NewContainer = Manager->SpawnHomeStorageContainer(FIntPoint(5, 5), EStorageContainerType::Cellar);
    if (TestNotNull(TEXT("Container spawned"), NewContainer))
    {
        TestNotNull(TEXT("Container has an inventory component"), NewContainer->InventoryComponent);
        if (NewContainer->InventoryComponent)
        {
            TestEqual(TEXT("Container type is what was requested (Cellar), not the constructor default (Basket)"),
                NewContainer->InventoryComponent->ContainerType, EStorageContainerType::Cellar);
        }
        NewContainer->Destroy();
    }

    // Другой тип на другой клетке -- убеждаемся, что тип не захардкожен.
    AStorageContainer* JarContainer = Manager->SpawnHomeStorageContainer(FIntPoint(6, 6), EStorageContainerType::Jar);
    if (TestNotNull(TEXT("Second container spawned"), JarContainer))
    {
        if (TestNotNull(TEXT("Second container has an inventory component"), JarContainer->InventoryComponent))
        {
            TestEqual(TEXT("Second container type is Jar"), JarContainer->InventoryComponent->ContainerType, EStorageContainerType::Jar);
        }
        JarContainer->Destroy();
    }

    // Клетка вне сетки -- отказ, не крэш.
    AStorageContainer* OutOfRange = Manager->SpawnHomeStorageContainer(FIntPoint(999999, 999999), EStorageContainerType::Cellar);
    TestNull(TEXT("Out-of-grid cell refuses to spawn a container"), OutOfRange);

    Manager->Destroy();
    return true;
}

// ---------------------------------------------------------------------------
// BuildHomeStorage -- полный гейт: клетка-якорь (AAlchemyTableActor), Respect
// Домового, материал в инвентаре. Ни один резолв не идёт через
// IngredientRegistrySubsystem (в отличие от PlantSeed/ActivateWard/
// EquipContainer) -- тестируем на уровне контроллера напрямую.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistHomeStorage_BuildRequiresRespectAndMaterial,
    "Herbalist.HomeStorage.BuildRequiresRespectAndMaterial",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistHomeStorage_BuildRequiresRespectAndMaterial::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    // Чистая база -- тот же принцип изоляции, что уже SpawnAndBeginPlay
    // применяет к ABiomeRegionVolume/AGridWorldManager (Destroy() не
    // гарантирует немедленное удаление из TActorIterator в том же кадре,
    // см. TestWorldHelpers.h). Два отдельных риска:
    //  - AStorageContainer, оставшиеся от SpawnCreatesContainerWithRequestedType
    //    выше или от предыдущего прогона, не должны сбить счёт этого теста
    //    (CountStorageContainersOfType ниже);
    //  - AAlchemyTableActor, оставшийся от ДРУГОГО теста (например
    //    ShrineActorTest.cpp спавнит свой на клетке (4,4) тоже) -- без
    //    очистки TActorIterator<AAlchemyTableActor> внутри BuildHomeStorage
    //    мог бы найти ЧУЖОЙ стол первым и читать Respect чужого Домового,
    //    не того, что этот тест только что поднял (найдено этим же тестом
    //    при первом прогоне -- симптом был "Respect всегда 0.00", хотя тест
    //    явно поднимал его перед шагом 3).
    for (TActorIterator<AStorageContainer> It(World); It; ++It)
    {
        if (AStorageContainer* Stale = *It) { Stale->Destroy(); }
    }
    for (TActorIterator<AAlchemyTableActor> It(World); It; ++It)
    {
        if (AAlchemyTableActor* Stale = *It) { Stale->Destroy(); }
    }

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;

    // AAlchemyTableActor::BeginPlay регистрирует Домового на своей клетке --
    // тот же путь, что и в реальной игре (RegisterDomovoi), не прямой вызов.
    // Ставится ровно в центр клетки (4,4) -- та же клетка, что уже
    // проверенно (Herbalist.ShrineActor.AlchemyTableNoLongerRegistersShrine,
    // ShrineActorTest.cpp) даёт Домовому зарегистрироваться в этом
    // персистентном тестовом мире, не занятая заранее SeedTestLandmarks.
    // Позиционирование тем же приёмом, что уже SpawnAtCell<T> в
    // ShrineActorTest.cpp (не переиспользован отсюда напрямую --
    // одноимённый template-хелпер в анонимном namespace другого файла того
    // же unity-модуля тестов рискует тем же MSVC C2084, что уже дважды
    // находился в этом проекте, см. TestWorldHelpers.h).
    const FVector TablePos = Manager->GetCellWorldPosition(4, 4);
    AAlchemyTableActor* Table = World->SpawnActor<AAlchemyTableActor>(AAlchemyTableActor::StaticClass(), TablePos, FRotator::ZeroRotator);
    if (!TestNotNull(TEXT("Alchemy table spawned"), Table)) { Manager->Destroy(); return false; }
    Table->DispatchBeginPlay();

    AHerbalistPlayerController* PC = SpawnControllerAndBeginPlay(World, Manager);
    if (!TestNotNull(TEXT("PlayerController spawned"), PC) || !TestNotNull(TEXT("InventoryComponent present"), PC->InventoryComponent))
    {
        Table->Destroy();
        Manager->Destroy();
        return false;
    }

    const UHerbalistSettings* Settings = GetHerbalistSettings();
    const float RespectThreshold = Settings ? Settings->HomeStorageRespectThreshold : 0.6f;
    const int32 MaterialCount = Settings ? Settings->HomeStorageMaterialCount : 3;

    const FIntPoint AnchorCell = Table->GetGridCoords();
    FEntityLandmark* Landmark = Manager->FindLandmarkAt(AnchorCell);
    if (!TestNotNull(TEXT("Домовой registered at the table's cell"), Landmark)) { Table->Destroy(); Manager->Destroy(); return false; }

    // 1) Ни Respect, ни материала -- refused, ничего не построено.
    PC->BuildHomeStorage(TEXT("cellar"));
    TestEqual(TEXT("No Respect, no material: nothing built"), CountStorageContainersOfType(World, EStorageContainerType::Cellar), 0);

    // 2) Материал есть, Respect всё ещё ниже порога -- refused.
    FInventoryItem Bark;
    Bark.IngredientID = FName(TEXT("broad_10"));
    Bark.Count = MaterialCount;
    PC->InventoryComponent->AddItem(Bark, MaterialCount);

    PC->BuildHomeStorage(TEXT("cellar"));
    TestEqual(TEXT("Material present but Respect too low: still refused"), CountStorageContainersOfType(World, EStorageContainerType::Cellar), 0);

    // 3) Respect поднят выше порога, материал по-прежнему есть -- succeeds.
    Landmark->Respect = RespectThreshold + 0.05f;
    PC->BuildHomeStorage(TEXT("cellar"));
    TestEqual(TEXT("Respect above threshold and material present: built exactly one"), CountStorageContainersOfType(World, EStorageContainerType::Cellar), 1);

    // Материал списан.
    bool bStillHasFullStack = false;
    for (const FInventoryItem& Item : PC->InventoryComponent->GetItems())
    {
        if (Item.IngredientID == FName(TEXT("broad_10")) && Item.Count >= MaterialCount)
        {
            bStillHasFullStack = true;
        }
    }
    TestFalse(TEXT("Material was consumed by a successful build"), bStillHasFullStack);

    // 4) Второй Погреб -- refused (v1: не больше одного хранилища одного
    // типа), даже если снова дать материал/Respect.
    FInventoryItem MoreBark;
    MoreBark.IngredientID = FName(TEXT("broad_10"));
    MoreBark.Count = MaterialCount;
    PC->InventoryComponent->AddItem(MoreBark, MaterialCount);
    PC->BuildHomeStorage(TEXT("cellar"));
    TestEqual(TEXT("A second Cellar is refused as a duplicate"), CountStorageContainersOfType(World, EStorageContainerType::Cellar), 1);

    // 5) Другой тип (Cabinet) -- не блокируется дублем Cellar, строится отдельно.
    PC->BuildHomeStorage(TEXT("cabinet"));
    TestEqual(TEXT("A different type (Cabinet) is not blocked by the existing Cellar"), CountStorageContainersOfType(World, EStorageContainerType::Cabinet), 1);

    // 6) Переносной тип (basket) -- не строится вовсе, независимо от Respect/материала.
    FInventoryItem EvenMoreBark;
    EvenMoreBark.IngredientID = FName(TEXT("broad_10"));
    EvenMoreBark.Count = MaterialCount;
    PC->InventoryComponent->AddItem(EvenMoreBark, MaterialCount);
    PC->BuildHomeStorage(TEXT("basket"));
    TestEqual(TEXT("Portable type 'basket' is refused, not buildable at home"), CountStorageContainersOfType(World, EStorageContainerType::Basket), 0);

    Table->Destroy();
    Manager->Destroy();
    return true;
}

#endif // WITH_AUTOMATION_TESTS && WITH_EDITOR
