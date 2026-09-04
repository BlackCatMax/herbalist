// Source/ProjectHerbalistTests/Private/Tests/EquipContainerTest.cpp
//
// Переносные контейнеры игрока (Корзина/Мешок/Туёс, 2026-09-04, "разберём
// тщательно" систему хранения, прямой запрос пользователя). Тестирует
// UHerbalistInventoryComponent::TryEquipContainer напрямую -- тестируемое
// ядро, отделённое от Exec-обёртки AHerbalistPlayerController::
// EquipContainer, которая резолвит FIngredientTableRow::GrantsContainerType
// через IngredientRegistrySubsystem (недоступный в Editor-мире автотестов,
// тот же класс пробела, что уже у ActivateWard/TradeWithCommunity/PlantSeed,
// см. ROADMAP.md/комментарий у объявления EquipContainer в .h).

#include "Core/Inventory/HerbalistInventoryComponent.h"
#include "Core/Types/HerbalistCoreTypes.h"
#include "Misc/AutomationTest.h"
#include "Editor.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

#if WITH_AUTOMATION_TESTS && WITH_EDITOR

namespace
{
    // Тот же приём, что уже MeasureDecayFor в StorageContainerTest.cpp --
    // компонент обязан быть зарегистрирован на живом актора/мире.
    UHerbalistInventoryComponent* SpawnRegisteredInventory(UWorld* World, AActor*& OutOwner)
    {
        OutOwner = World->SpawnActor<AActor>();
        UHerbalistInventoryComponent* Inventory = NewObject<UHerbalistInventoryComponent>(OutOwner);
        Inventory->RegisterComponent();
        return Inventory;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistEquipContainer_OwnedItemSwitchesContainerType,
    "Herbalist.EquipContainer.OwnedItemSwitchesContainerType",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistEquipContainer_OwnedItemSwitchesContainerType::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AActor* Owner = nullptr;
    UHerbalistInventoryComponent* Inventory = SpawnRegisteredInventory(World, Owner);
    if (!TestNotNull(TEXT("Inventory registered"), Inventory)) return false;

    TestEqual(TEXT("Starts at None (test fixture default, not the real BeginPlay Basket default)"),
        Inventory->ContainerType, EStorageContainerType::None);

    FInventoryItem Tues;
    Tues.IngredientID = FName(TEXT("Туёс"));
    Tues.Count = 1;
    Inventory->AddItem(Tues);

    const bool bEquipped = Inventory->TryEquipContainer(FName(TEXT("Туёс")), EStorageContainerType::Tues);
    TestTrue(TEXT("TryEquipContainer succeeds when the item is owned"), bEquipped);
    TestEqual(TEXT("ContainerType switches to the granted type"), Inventory->ContainerType, EStorageContainerType::Tues);

    // Не расходуется -- контейнер носишь, не сжигаешь (тот же принцип, что
    // уже у активации оберегов).
    int32 Count = 0;
    for (const FInventoryItem& Item : Inventory->GetItems())
    {
        if (Item.IngredientID == FName(TEXT("Туёс"))) Count += Item.Count;
    }
    TestEqual(TEXT("Equipping does not consume the item"), Count, 1);

    Owner->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistEquipContainer_MissingItemRefusesAndLeavesContainerTypeUntouched,
    "Herbalist.EquipContainer.MissingItemRefusesAndLeavesContainerTypeUntouched",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistEquipContainer_MissingItemRefusesAndLeavesContainerTypeUntouched::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AActor* Owner = nullptr;
    UHerbalistInventoryComponent* Inventory = SpawnRegisteredInventory(World, Owner);
    if (!TestNotNull(TEXT("Inventory registered"), Inventory)) return false;

    // Ставим заведомо отличный от None тип, чтобы честно проверить "не
    // трогает", а не совпадение с умолчанием.
    Inventory->ContainerType = EStorageContainerType::Cellar;

    // Инвентарь пуст -- нет ни одного "Мешок".
    const bool bEquipped = Inventory->TryEquipContainer(FName(TEXT("Мешок")), EStorageContainerType::Sack);
    TestFalse(TEXT("TryEquipContainer refuses when the item is not owned"), bEquipped);
    TestEqual(TEXT("ContainerType stays untouched on refusal"), Inventory->ContainerType, EStorageContainerType::Cellar);

    Owner->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistEquipContainer_NonContainerRowRefusesEvenIfOwned,
    "Herbalist.EquipContainer.NonContainerRowRefusesEvenIfOwned",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistEquipContainer_NonContainerRowRefusesEvenIfOwned::RunTest(const FString& Parameters)
{
    // GrantsType == None (обычная трава, не карточка-контейнер) -- отказ,
    // даже если предмет с таким именем реально лежит в инвентаре. Это то,
    // что реальный AHerbalistPlayerController::EquipContainer резолвит через
    // FIngredientTableRow::GrantsContainerType до вызова этой функции --
    // здесь проверяется, что сама TryEquipContainer тоже честно отказывает,
    // не полагаясь только на вызывающую сторону.
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AActor* Owner = nullptr;
    UHerbalistInventoryComponent* Inventory = SpawnRegisteredInventory(World, Owner);
    if (!TestNotNull(TEXT("Inventory registered"), Inventory)) return false;

    FInventoryItem Herb;
    Herb.IngredientID = FName(TEXT("ОбычнаяТрава"));
    Herb.Count = 1;
    Inventory->AddItem(Herb);

    const bool bEquipped = Inventory->TryEquipContainer(FName(TEXT("ОбычнаяТрава")), EStorageContainerType::None);
    TestFalse(TEXT("TryEquipContainer refuses a GrantsType of None even when the named item is owned"), bEquipped);
    TestEqual(TEXT("ContainerType stays at the default"), Inventory->ContainerType, EStorageContainerType::None);

    Owner->Destroy();
    return true;
}

#endif // WITH_AUTOMATION_TESTS && WITH_EDITOR
