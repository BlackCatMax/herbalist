// Source/ProjectHerbalistTests/Private/Tests/DiegeticBeltTest.cpp
//
// Диегетический интерфейс, этап 4 (DESIGN_Diegetic_Interface.md,
// 2026-09-21): пояс. Инструмент, контейнер, оберег и семенной мешочек видны,
// когда смотришь вниз; надеваются из руки, снимаются в руку пустой рукой
// (решения пользователя). Карточки оберегов-кристаллов и контейнеров живут в
// реестре, которого в мире автотестов нет, -- здесь проверены инструмент,
// мешочек, серебряный оберег, снятие корзины и отказ без карточки.

#include "Core/World/GridWorldManager.h"
#include "Core/Inventory/HerbalistInventoryComponent.h"
#include "Player/HerbalistPlayerController.h"
#include "Player/HeldItemComponent.h"
#include "Player/BeltComponent.h"
#include "Player/BeltItemActor.h"
#include "Misc/AutomationTest.h"
#include "Editor.h"
#include "Engine/World.h"

#if WITH_AUTOMATION_TESTS && WITH_EDITOR

#include "TestWorldHelpers.h"

namespace
{
    int32 BeltIndexOf(const AHerbalistPlayerController* PC, FName ID)
    {
        return PC->InventoryComponent->GetItems().IndexOfByPredicate([ID](const FInventoryItem& Item) { return Item.IngredientID == ID; });
    }

    void EnsureBeltItem(AHerbalistPlayerController* PC, FName ID)
    {
        if (BeltIndexOf(PC, ID) == INDEX_NONE)
        {
            FInventoryItem Item;
            Item.IngredientID = ID;
            Item.Count = 1;
            Item.bSubjectToDecay = false;
            PC->InventoryComponent->AddItem(Item, 1);
        }
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistDiegetic_BeltWearsAndReleasesTheTool,
    "Herbalist.Diegetic.BeltWearsAndReleasesTheTool",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistDiegetic_BeltWearsAndReleasesTheTool::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;
    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("Manager spawned"), Manager)) return false;
    AHerbalistPlayerController* PC = SpawnControllerAndBeginPlay(World, Manager);
    if (!TestNotNull(TEXT("Controller spawned"), PC)) { Manager->Destroy(); return false; }
    UBeltComponent* Belt = PC->BeltComponent;
    if (!TestNotNull(TEXT("Пояс есть"), Belt)) { PC->Destroy(); Manager->Destroy(); return false; }

    const FName Blade(TEXT("Железный серп"));
    EnsureBeltItem(PC, Blade);
    PC->CurrentGatheringTool = EGatheringTool::BareHands;

    // Надеть серп из руки -- инструмент сбора, рука пустеет, серп в котомке.
    PC->HeldItemComponent->TakeFromInventory(BeltIndexOf(PC, Blade));
    TestTrue(TEXT("Серп на поясе"), Belt->PutOn(PC->HeldItemComponent->ResolveHeldIndex()));
    TestTrue(TEXT("Сбор серпом"), PC->CurrentGatheringTool == EGatheringTool::IronBlade);
    TestEqual(TEXT("Слот инструмента"), Belt->GetSlotItem(EBeltSlot::Tool), Blade);
    TestFalse(TEXT("Рука пуста"), PC->HeldItemComponent->IsHolding());
    TestTrue(TEXT("Серп в котомке"), BeltIndexOf(PC, Blade) != INDEX_NONE);

    // Снять пустой рукой -- в руку, сбор голыми руками.
    Belt->UseSlot(EBeltSlot::Tool);
    TestTrue(TEXT("Голыми руками"), PC->CurrentGatheringTool == EGatheringTool::BareHands);
    TestTrue(TEXT("Серп в руке"), PC->HeldItemComponent->IsHolding() && PC->HeldItemComponent->GetHeldItem().IngredientID == Blade);
    TestTrue(TEXT("Слот пуст"), Belt->GetSlotItem(EBeltSlot::Tool).IsNone());
    PC->HeldItemComponent->PutAway();

    // Надетый серп ушёл из котомки -- пояс отпускает его сам.
    Belt->PutOn(BeltIndexOf(PC, Blade));
    PC->InventoryComponent->RemoveItem(BeltIndexOf(PC, Blade), PC->InventoryComponent->GetItems()[BeltIndexOf(PC, Blade)].Count);
    Belt->ReleaseMissing();
    TestTrue(TEXT("Серпа нет -- голыми руками"), PC->CurrentGatheringTool == EGatheringTool::BareHands);

    // Трава -- не вещь для пояса.
    FInventoryItem Herb;
    Herb.IngredientID = FName(TEXT("bol_01"));
    Herb.Count = 1;
    PC->InventoryComponent->AddItem(Herb, 1);
    TestFalse(TEXT("Трава не надевается"), Belt->PutOn(BeltIndexOf(PC, Herb.IngredientID)));

    PC->Destroy();
    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistDiegetic_BeltPouchContainerAndSilverWard,
    "Herbalist.Diegetic.BeltPouchContainerAndSilverWard",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistDiegetic_BeltPouchContainerAndSilverWard::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;
    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("Manager spawned"), Manager)) return false;
    AHerbalistPlayerController* PC = SpawnControllerAndBeginPlay(World, Manager);
    if (!TestNotNull(TEXT("Controller spawned"), PC)) { Manager->Destroy(); return false; }
    UBeltComponent* Belt = PC->BeltComponent;

    // Мешочек: развязать -- сбор на семена, завязать -- обычный.
    PC->CurrentHarvestIntent = EHarvestIntent::Brew;
    Belt->UseSlot(EBeltSlot::SeedPouch);
    TestTrue(TEXT("Развязан -- на семена"), PC->CurrentHarvestIntent == EHarvestIntent::Seed);
    Belt->UseSlot(EBeltSlot::SeedPouch);
    TestTrue(TEXT("Завязан -- обычный"), PC->CurrentHarvestIntent == EHarvestIntent::Brew);

    // Стартовая корзина на поясе; снять -- в руку, контейнера нет.
    TestEqual(TEXT("На поясе корзина"), Belt->GetSlotItem(EBeltSlot::Container), FName(TEXT("Корзина")));
    Belt->UseSlot(EBeltSlot::Container);
    TestTrue(TEXT("Контейнера нет"), PC->InventoryComponent->ContainerType == EStorageContainerType::None);
    TestTrue(TEXT("Корзина в руке"), PC->HeldItemComponent->IsHolding() && PC->HeldItemComponent->GetHeldItem().IngredientID == FName(TEXT("Корзина")));
    PC->HeldItemComponent->PutAway();

    // Серебряный оберег действует, пока висит.
    const FName Silver(TEXT("Серебряный оберег"));
    EnsureBeltItem(PC, Silver);
    Manager->SetSilverWardActive(false);
    TestTrue(TEXT("Оберег повешен"), Belt->PutOn(BeltIndexOf(PC, Silver)));
    TestTrue(TEXT("Действует"), Manager->IsSilverWardActive());
    TestTrue(TEXT("Светится"), Belt->IsWardLit());
    Belt->UseSlot(EBeltSlot::Ward);
    TestFalse(TEXT("Снят -- не действует"), Manager->IsSilverWardActive());
    TestTrue(TEXT("Оберег в руке"), PC->HeldItemComponent->IsHolding() && PC->HeldItemComponent->GetHeldItem().IngredientID == Silver);

    // Ревью 2026-09-21: после загрузки серебряный оберег действует (флаг мира
    // сохраняется), а пояс его не помнит -- слот выводится из мира.
    PC->HeldItemComponent->PutAway();
    Manager->SetSilverWardActive(true);
    TestEqual(TEXT("После загрузки -- на поясе"), Belt->GetSlotItem(EBeltSlot::Ward), Silver);
    TestTrue(TEXT("И светится"), Belt->IsWardLit());

    PC->HeldItemComponent->PutAway();
    Manager->SetSilverWardActive(false);
    PC->Destroy();
    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistDiegetic_BeltShowsWhenLookingDown,
    "Herbalist.Diegetic.BeltShowsWhenLookingDown",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistDiegetic_BeltShowsWhenLookingDown::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;
    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("Manager spawned"), Manager)) return false;
    AHerbalistPlayerController* PC = SpawnControllerAndBeginPlay(World, Manager);
    if (!TestNotNull(TEXT("Controller spawned"), PC)) { Manager->Destroy(); return false; }
    UBeltComponent* Belt = PC->BeltComponent;

    PC->SetControlRotation(FRotator(0.0f, 0.0f, 0.0f));
    Belt->TickComponent(0.1f, LEVELTICK_All, nullptr);
    TestFalse(TEXT("Прямо -- пояса не видно"), Belt->IsInView());
    TestEqual(TEXT("Заглушек нет"), Belt->GetShownItems().Num(), 0);

    PC->SetControlRotation(FRotator(-70.0f, 0.0f, 0.0f));
    Belt->TickComponent(0.1f, LEVELTICK_All, nullptr);
    TestTrue(TEXT("Вниз -- пояс виден"), Belt->IsInView());
    TestTrue(TEXT("Хотя бы мешочек на месте"), Belt->GetShownItems().Num() >= 1);

    // Взгляд находит предмет пояса, мир его не видит.
    ABeltItemActor* Item = Belt->GetShownItems().Num() > 0 ? Belt->GetShownItems()[0].Get() : nullptr;
    if (TestNotNull(TEXT("Есть заглушка"), Item))
    {
        FVector ViewLocation;
        FRotator ViewRotation;
        PC->GetPlayerViewPoint(ViewLocation, ViewRotation);
        const FVector Through = ViewLocation + (Item->GetActorLocation() - ViewLocation) * 3.0f;
        TestEqual(TEXT("Взгляд находит"), Belt->FindItemUnderView(ViewLocation, Through), Item);
        FHitResult WorldHit;
        const bool bWorldHit = World->LineTraceSingleByChannel(WorldHit, ViewLocation, Through, ECC_Visibility);
        TestFalse(TEXT("Мир сквозь пояс"), bWorldHit && WorldHit.GetActor() == Item);
    }

    // Ревью 2026-09-21: заглушки обновляются на месте, а не пересоздаются --
    // иначе подсветка того, на что смотрят, мигала.
    Belt->TickComponent(0.3f, LEVELTICK_All, nullptr);
    TestTrue(TEXT("Та же заглушка после обновления"), Belt->GetShownItems().Num() > 0 && Belt->GetShownItems()[0].Get() == Item);

    PC->SetControlRotation(FRotator(0.0f, 0.0f, 0.0f));
    Belt->TickComponent(0.1f, LEVELTICK_All, nullptr);
    TestEqual(TEXT("Поднял взгляд -- убрано"), Belt->GetShownItems().Num(), 0);

    PC->Destroy();
    Manager->Destroy();
    return true;
}

#endif // WITH_AUTOMATION_TESTS && WITH_EDITOR
