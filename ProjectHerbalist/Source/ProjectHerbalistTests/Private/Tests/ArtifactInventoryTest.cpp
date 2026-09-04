// Source/ProjectHerbalistTests/Private/Tests/ArtifactInventoryTest.cpp
//
// Артефакты/перья как настоящие предметы инвентаря (2026-09-02) — тот же
// DispatchBeginPlay-паттерн, что уже TestWorldHelpers.h применяет к
// AGridWorldManager, здесь дополнительно применён к AHerbalistPlayerController
// (у него есть собственный InventoryComponent-субобъект в конструкторе,
// BeginPlay безопасен без Pawn/PlayerState — не требует владения пешкой,
// см. AHerbalistPlayerController::BeginPlay). Проверяет НОВУЮ логику
// (AddArtifactToInventory/RemoveArtifactFromInventory), не переповторяет
// уже покрытое ArtifactTest.cpp/ProphetFeatherTest.cpp (TryAcquireArtifact/
// TryAcquireProphetFeather сами по себе).

#include "Core/World/GridWorldManager.h"
#include "Core/Inventory/HerbalistInventoryComponent.h"
#include "Core/Subsystems/IngredientRegistrySubsystem.h"
#include "Player/HerbalistPlayerController.h"
#include "Misc/AutomationTest.h"
#include "Editor.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "Engine/Engine.h"

#if WITH_AUTOMATION_TESTS && WITH_EDITOR

#include "TestWorldHelpers.h"

namespace
{
    // SpawnControllerAndBeginPlay -- вынесено в TestWorldHelpers.h (2026-09-04,
    // ODR-дубликат с GardenPlantingTest.cpp, тот же класс проблемы, что уже
    // решён для SpawnAndBeginPlay). Manager явно передаётся и инжектируется
    // (SetWorldManagerForTests) — та же причина, что и раньше, см. комментарий
    // у SetWorldManagerForTests, HerbalistPlayerController.h.

    int32 CountItemsWithID(UHerbalistInventoryComponent* Inventory, FName ID)
    {
        int32 Count = 0;
        for (const FInventoryItem& Item : Inventory->GetItems())
        {
            if (Item.IngredientID == ID) ++Count;
        }
        return Count;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistArtifactInventory_HonestOfferingAddsArtifactToPlayerInventory,
    "Herbalist.ArtifactInventory.HonestOfferingAddsArtifactToPlayerInventory",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistArtifactInventory_HonestOfferingAddsArtifactToPlayerInventory::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("Manager spawned"), Manager)) return false;
    AHerbalistPlayerController* PC = SpawnControllerAndBeginPlay(World, Manager);
    if (!TestNotNull(TEXT("Controller spawned"), PC)) { Manager->Destroy(); return false; }
    if (!TestNotNull(TEXT("Controller has an InventoryComponent"), PC->InventoryComponent))
    {
        Manager->Destroy(); PC->Destroy(); return false;
    }

    const FName LegendaryID(TEXT("Индрик-зверь"));
    const FIntPoint* Anchor = Manager->GetLegendaryAnchors().Find(LegendaryID);
    if (!TestNotNull(TEXT("Индрик-зверь has a seeded anchor cell"), Anchor))
    {
        Manager->Destroy(); PC->Destroy(); return false;
    }
    if (FGridCell* Cell = Manager->GetCell(Anchor->X, Anchor->Y))
    {
        Cell->ManifestedEntityID = LegendaryID;
    }

    FInventoryItem Offering;
    Offering.IngredientID = FName(TEXT("TestOffering"));
    Offering.State.Meta.Purity = 0.9f;
    Offering.Count = 1;
    PC->InventoryComponent->AddItem(Offering);

    TestEqual(TEXT("Not in inventory before acquisition"), CountItemsWithID(PC->InventoryComponent, FName(TEXT("Рог"))), 0);

    PC->OfferForArtifact(TEXT("Рог"), TEXT("TestOffering"));

    TestEqual(TEXT("Offered item consumed"), CountItemsWithID(PC->InventoryComponent, FName(TEXT("TestOffering"))), 0);
    TestEqual(TEXT("Рог now appears in the player's own inventory"), CountItemsWithID(PC->InventoryComponent, FName(TEXT("Рог"))), 1);

    Manager->Destroy();
    PC->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistArtifactInventory_ConsumedArtifactIsRemovedFromInventory,
    "Herbalist.ArtifactInventory.ConsumedArtifactIsRemovedFromInventory",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistArtifactInventory_ConsumedArtifactIsRemovedFromInventory::RunTest(const FString& Parameters)
{
    // Гребень/Яблоко/перья -- расходуемые: успешное применение должно
    // убрать "квитанцию" из инвентаря, не оставлять предмет-призрак.
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("Manager spawned"), Manager)) return false;
    AHerbalistPlayerController* PC = SpawnControllerAndBeginPlay(World, Manager);
    if (!TestNotNull(TEXT("Controller spawned"), PC)) { Manager->Destroy(); return false; }

    FAcquiredArtifact Comb;
    Comb.ArtifactID = FName(TEXT("Гребень"));
    Manager->SetAcquiredArtifacts({ Comb });

    FInventoryItem CombItem;
    CombItem.IngredientID = FName(TEXT("Гребень"));
    CombItem.Count = 1;
    PC->InventoryComponent->AddItem(CombItem);
    TestEqual(TEXT("Гребень starts in the inventory"), CountItemsWithID(PC->InventoryComponent, FName(TEXT("Гребень"))), 1);

    PC->UseComb(2, 2);

    TestEqual(TEXT("Гребень removed from the inventory after being spent"), CountItemsWithID(PC->InventoryComponent, FName(TEXT("Гребень"))), 0);
    TestEqual(TEXT("Гребень also gone from AcquiredArtifacts (mechanic itself, unaffected by this change)"),
        Manager->GetAcquiredArtifacts().Num(), 0);

    Manager->Destroy();
    PC->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistArtifactInventory_ReusableArtifactStaysInInventoryAfterUse,
    "Herbalist.ArtifactInventory.ReusableArtifactStaysInInventoryAfterUse",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistArtifactInventory_ReusableArtifactStaysInInventoryAfterUse::RunTest(const FString& Parameters)
{
    // Шапка-невидимка -- НЕ расходуется (§21.3), её квитанция должна
    // остаться в инвентаре после реального успешного применения (не просто
    // "контроллер не трогал её потому что рано вышел" -- нужен настоящий
    // Pawn, чтобы UseInvisibilityCap() дошёл до WorldManager).
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("Manager spawned"), Manager)) return false;
    AHerbalistPlayerController* PC = SpawnControllerAndBeginPlay(World, Manager);
    if (!TestNotNull(TEXT("Controller spawned"), PC)) { Manager->Destroy(); return false; }

    APawn* Pawn = World->SpawnActor<APawn>();
    if (!TestNotNull(TEXT("Pawn spawned"), Pawn)) { Manager->Destroy(); PC->Destroy(); return false; }
    Pawn->SetActorLocation(Manager->GetCellWorldPosition(5, 5));
    PC->Possess(Pawn);

    FAcquiredArtifact Cap;
    Cap.ArtifactID = FName(TEXT("Шапка-невидимка"));
    Manager->SetAcquiredArtifacts({ Cap });

    FInventoryItem CapItem;
    CapItem.IngredientID = FName(TEXT("Шапка-невидимка"));
    CapItem.Count = 1;
    PC->InventoryComponent->AddItem(CapItem);

    PC->UseInvisibilityCap();
    TestTrue(TEXT("Шапка actually activated (sanity check the real path was exercised)"), Manager->IsInvisibilityCapActive());
    TestEqual(TEXT("Шапка stays in inventory after a real, successful, reusable use"),
        CountItemsWithID(PC->InventoryComponent, FName(TEXT("Шапка-невидимка"))), 1);

    Manager->Destroy();
    PC->Destroy();
    Pawn->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistArtifactInventory_FeatherAcquisitionAndConsumptionUpdateInventory,
    "Herbalist.ArtifactInventory.FeatherAcquisitionAndConsumptionUpdateInventory",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistArtifactInventory_FeatherAcquisitionAndConsumptionUpdateInventory::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("Manager spawned"), Manager)) return false;
    AHerbalistPlayerController* PC = SpawnControllerAndBeginPlay(World, Manager);
    if (!TestNotNull(TEXT("Controller spawned"), PC)) { Manager->Destroy(); return false; }

    const FName EntityID(TEXT("жар-птица"));
    const FIntPoint* Anchor = Manager->GetLegendaryAnchors().Find(EntityID);
    if (!TestNotNull(TEXT("жар-птица has a seeded anchor cell"), Anchor))
    {
        Manager->Destroy(); PC->Destroy(); return false;
    }
    if (FGridCell* Cell = Manager->GetCell(Anchor->X, Anchor->Y))
    {
        Cell->ManifestedEntityID = EntityID;
    }

    PC->AcquireFeather(TEXT("Перо Жар-птицы"));
    TestEqual(TEXT("Feather appears in the player's inventory on acquisition"),
        CountItemsWithID(PC->InventoryComponent, FName(TEXT("Перо Жар-птицы"))), 1);

    PC->UseZharPtitsaFeather(4, 4);
    TestEqual(TEXT("Feather removed from inventory after being spent"),
        CountItemsWithID(PC->InventoryComponent, FName(TEXT("Перо Жар-птицы"))), 0);
    TestTrue(TEXT("Cell actually marked eternally pure (the real effect, not just the receipt)"),
        Manager->IsCellEternallyPure(FIntPoint(4, 4)));

    Manager->Destroy();
    PC->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistArtifactInventory_DataTableRowsResolveDisplayNames,
    "Herbalist.ArtifactInventory.DataTableRowsResolveDisplayNames",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistArtifactInventory_DataTableRowsResolveDisplayNames::RunTest(const FString& Parameters)
{
    // Регрессия на сами ряды DT_IngredientClass (ArtifactIngredientAppendCommandlet) --
    // не только что контроллер зовёт AddItem, но что добавленный предмет
    // реально резолвится в реестре (GetItemDisplayName, HerbalistNameUtils.cpp),
    // не просто падает на голый IngredientID.ToString() фолбэк.
    //
    // UIngredientRegistrySubsystem наследует UGameInstanceSubsystem
    // (ClassWithin=UGameInstance) -- редакторский мир автотестов не несёт
    // настоящего GameInstance, тот же приём инъекции через временный
    // NewObject<UGameInstance>(GEngine), что уже IngredientRegistryTest.cpp
    // использует для этого класса.
    UGameInstance* OwnerGameInstance = NewObject<UGameInstance>(GEngine);
    UIngredientRegistrySubsystem* Registry = NewObject<UIngredientRegistrySubsystem>(OwnerGameInstance);
    if (!TestNotNull(TEXT("Ingredient registry subsystem constructed"), Registry)) return false;

    UDataTable* Table = LoadObject<UDataTable>(nullptr, TEXT("/Game/Herbalist/Data/DT_IngredientClass"));
    if (!TestNotNull(TEXT("DT_IngredientClass loads"), Table)) return false;
    Registry->LoadFromDataTable(Table);

    static const FName NamesToCheck[] = {
        FName(TEXT("Зеркальце")), FName(TEXT("Клубочек")), FName(TEXT("Рог")), FName(TEXT("Гребень")),
        FName(TEXT("Молодильное яблоко")), FName(TEXT("Шапка-невидимка")), FName(TEXT("Камень-оберег")), FName(TEXT("Фонарь")),
        FName(TEXT("Перо Гамаюна")), FName(TEXT("Перо Алконоста")), FName(TEXT("Перо Сирина")), FName(TEXT("Перо Жар-птицы")),
    };
    for (FName Name : NamesToCheck)
    {
        const FIngredientTableRow* Row = Registry->GetRow(Name);
        TestNotNull(*FString::Printf(TEXT("%s has a registry row"), *Name.ToString()), Row);
        if (Row)
        {
            TestEqual(*FString::Printf(TEXT("%s DisplayName matches its own ID"), *Name.ToString()),
                Row->DisplayName.ToString(), Name.ToString());
            TestFalse(*FString::Printf(TEXT("%s never spawns via harvest (empty AllowedBiomes)"), *Name.ToString()),
                Row->AllowedBiomes.Num() > 0);
        }
    }

    return true;
}

#endif // WITH_AUTOMATION_TESTS && WITH_EDITOR
