// Source/ProjectHerbalistTests/Private/Tests/DiegeticPesterTest.cpp
//
// Диегетический интерфейс, этап 2 (DESIGN_Diegetic_Interface.md,
// 2026-09-21): пестерь вместо окна котомки. Клавиша котомки раскрывает его
// перед камерой, предметы лежат по мешочкам, на предмет смотрят и берут его
// в руку взаимодействием; с предметом в руке клавиша котомки убирает его, а
// клавиша сведений -- осматривает.

#include "Core/World/GridWorldManager.h"
#include "Core/Inventory/HerbalistInventoryComponent.h"
#include "Player/HerbalistPlayerController.h"
#include "Player/HeldItemComponent.h"
#include "Player/PesterComponent.h"
#include "Player/PesterItemActor.h"
#include "Misc/AutomationTest.h"
#include "Editor.h"
#include "Engine/World.h"

#if WITH_AUTOMATION_TESTS && WITH_EDITOR

#include "TestWorldHelpers.h"

namespace
{
    FInventoryItem MakePesterTestItem(FName ID, float CreationTime, int32 Count = 1)
    {
        FInventoryItem Item;
        Item.IngredientID = ID;
        Item.Count = Count;
        Item.CreationTime = CreationTime;
        Item.State.Magnitude = 0.5f;
        Item.State.Direction.Nature = 0.7f;
        Item.State.Direction.Body = 0.1f;
        Item.State.Direction.Mind = 0.1f;
        Item.State.Direction.Spirit = 0.1f;
        return Item;
    }

    int32 CountNonEmpty(const UHerbalistInventoryComponent* Inventory)
    {
        int32 Count = 0;
        for (const FInventoryItem& Item : Inventory->GetItems())
        {
            Count += Item.Count > 0 ? 1 : 0;
        }
        return Count;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistDiegetic_PouchesSortByKindOfMaterial,
    "Herbalist.Diegetic.PouchesSortByKindOfMaterial",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistDiegetic_PouchesSortByKindOfMaterial::RunTest(const FString& Parameters)
{
    const FInventoryItem Herb = MakePesterTestItem(FName(TEXT("bol_01")), 1.0f);
    FInventoryItem Water = MakePesterTestItem(FName(TEXT("BogWater")), 2.0f);
    Water.bIsWater = true;
    const FInventoryItem Potion = MakePesterTestItem(FName(TEXT("Potion")), 3.0f);

    TestTrue(TEXT("Трава -- в мешочек трав"), UPesterComponent::PouchForItem(Herb, EIngredientClass::Plant, true) == EPesterPouch::Herbs);
    TestTrue(TEXT("Гриб -- к грибам"), UPesterComponent::PouchForItem(Herb, EIngredientClass::Fungus, true) == EPesterPouch::Fungi);
    TestTrue(TEXT("Камень -- к камням"), UPesterComponent::PouchForItem(Herb, EIngredientClass::Mineral, true) == EPesterPouch::Stones);
    TestTrue(TEXT("Вода -- в склянки"), UPesterComponent::PouchForItem(Water, EIngredientClass::Plant, true) == EPesterPouch::Vials);
    TestTrue(TEXT("Зелье -- в склянки, даже без реестра"), UPesterComponent::PouchForItem(Potion, EIngredientClass::Unknown, false) == EPesterPouch::Vials);
    TestTrue(TEXT("Неизвестное -- в прочее"), UPesterComponent::PouchForItem(Herb, EIngredientClass::Unknown, false) == EPesterPouch::Other);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistDiegetic_PesterLaysOutTheBagAndFollowsIt,
    "Herbalist.Diegetic.PesterLaysOutTheBagAndFollowsIt",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistDiegetic_PesterLaysOutTheBagAndFollowsIt::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;
    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("Manager spawned"), Manager)) return false;
    AHerbalistPlayerController* PC = SpawnControllerAndBeginPlay(World, Manager);
    if (!TestNotNull(TEXT("Controller spawned"), PC)) { Manager->Destroy(); return false; }
    UPesterComponent* Pester = PC->PesterComponent;
    if (!TestNotNull(TEXT("У контроллера есть пестерь"), Pester)) { PC->Destroy(); Manager->Destroy(); return false; }

    PC->InventoryComponent->AddItem(MakePesterTestItem(FName(TEXT("bol_01")), 11.0f, 4));

    // Клавиша котомки: окна нет, раскрывается пестерь.
    PC->Inventory();
    TestTrue(TEXT("Пестерь раскрыт"), Pester->IsOpen());
    TestFalse(TEXT("Окно не открыто"), PC->bIsAnyWidgetOpen);
    TestEqual(TEXT("Разложено всё, что есть в котомке"), Pester->GetLaidOutItems().Num(), CountNonEmpty(PC->InventoryComponent));

    // Котомка изменилась, пока пестерь открыт -- раскладка следом.
    PC->InventoryComponent->AddItem(MakePesterTestItem(FName(TEXT("les_06")), 12.0f));
    TestEqual(TEXT("Новый предмет лёг в пестерь"), Pester->GetLaidOutItems().Num(), CountNonEmpty(PC->InventoryComponent));

    // Закрыли -- раскладка убрана из мира.
    TArray<TWeakObjectPtr<APesterItemActor>> Weak;
    for (APesterItemActor* Actor : Pester->GetLaidOutItems()) Weak.Add(Actor);
    PC->Inventory();
    TestFalse(TEXT("Пестерь закрыт"), Pester->IsOpen());
    TestEqual(TEXT("Раскладки нет"), Pester->GetLaidOutItems().Num(), 0);
    bool bAnyAlive = false;
    for (const TWeakObjectPtr<APesterItemActor>& Actor : Weak) bAnyAlive = bAnyAlive || (Actor.IsValid() && !Actor->IsActorBeingDestroyed());
    TestFalse(TEXT("Заглушки уничтожены"), bAnyAlive);

    PC->Destroy();
    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistDiegetic_TakeFromPesterIntoHandThenInspectAndPutAway,
    "Herbalist.Diegetic.TakeFromPesterIntoHandThenInspectAndPutAway",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistDiegetic_TakeFromPesterIntoHandThenInspectAndPutAway::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;
    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("Manager spawned"), Manager)) return false;
    AHerbalistPlayerController* PC = SpawnControllerAndBeginPlay(World, Manager);
    if (!TestNotNull(TEXT("Controller spawned"), PC)) { Manager->Destroy(); return false; }

    PC->InventoryComponent->AddItem(MakePesterTestItem(FName(TEXT("bol_01")), 21.0f));
    PC->Inventory();

    // Предмет в пестере, на который «посмотрели» и нажали взаимодействие.
    APesterItemActor* Target = nullptr;
    for (APesterItemActor* Actor : PC->PesterComponent->GetLaidOutItems())
    {
        if (Actor && Actor->GetItem().CreationTime == 21.0f) Target = Actor;
    }
    if (!TestNotNull(TEXT("Трава лежит в пестере"), Target)) { PC->PesterComponent->Close(); PC->Destroy(); Manager->Destroy(); return false; }
    // Реализацию напрямую, не через Execute_OnInteract: в мире редактора
    // скриптовый вызов до кода не доходит (тот же приём, что KurganTest).
    Target->OnInteract_Implementation(PC);

    TestFalse(TEXT("Взяли -- пестерь закрылся"), PC->PesterComponent->IsOpen());
    TestTrue(TEXT("Трава в руке"), PC->HeldItemComponent->IsHolding());
    TestEqual(TEXT("Та самая трава"), PC->HeldItemComponent->GetHeldItem().CreationTime, 21.0f);

    // Клавиша сведений с предметом в руке -- осмотр.
    PC->Info();
    TestTrue(TEXT("Сведения -- осмотр предмета"), PC->HeldItemComponent->IsInspecting());

    // Клавиша котомки с предметом в руке -- убрать, не открывать пестерь.
    PC->Inventory();
    TestFalse(TEXT("Рука пуста"), PC->HeldItemComponent->IsHolding());
    TestFalse(TEXT("Пестерь не открылся"), PC->PesterComponent->IsOpen());

    PC->Destroy();
    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistDiegetic_PesterDoesNotBlockTheWorldButAnswersTheGaze,
    "Herbalist.Diegetic.PesterDoesNotBlockTheWorldButAnswersTheGaze",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistDiegetic_PesterDoesNotBlockTheWorldButAnswersTheGaze::RunTest(const FString& Parameters)
{
    // Ревью 2026-09-21: пестерь висит в полуметре перед камерой. Если его
    // заглушки ловят общие лучи мира, сбор, сведения о клетке и подсветка
    // упираются в собственную котомку. Мир их видеть не должен; взгляд
    // спрашивает пестерь отдельно.
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;
    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("Manager spawned"), Manager)) return false;
    AHerbalistPlayerController* PC = SpawnControllerAndBeginPlay(World, Manager);
    if (!TestNotNull(TEXT("Controller spawned"), PC)) { Manager->Destroy(); return false; }

    PC->InventoryComponent->AddItem(MakePesterTestItem(FName(TEXT("bol_01")), 31.0f));
    PC->Inventory();
    APesterItemActor* Item = PC->PesterComponent->GetLaidOutItems().Num() > 0 ? PC->PesterComponent->GetLaidOutItems()[0].Get() : nullptr;
    if (!TestNotNull(TEXT("Есть разложенный предмет"), Item)) { PC->PesterComponent->Close(); PC->Destroy(); Manager->Destroy(); return false; }

    // Луч от взгляда прямо сквозь заглушку.
    FVector ViewLocation;
    FRotator ViewRotation;
    PC->GetPlayerViewPoint(ViewLocation, ViewRotation);
    const FVector Through = ViewLocation + (Item->GetActorLocation() - ViewLocation) * 3.0f;

    FHitResult WorldHit;
    FCollisionQueryParams Params;
    const bool bWorldHit = World->LineTraceSingleByChannel(WorldHit, ViewLocation, Through, ECC_Visibility, Params);
    TestFalse(TEXT("Луч мира не упирается в пестерь"), bWorldHit && WorldHit.GetActor() == Item);

    TestEqual(TEXT("Взгляд находит предмет пестеря"), PC->PesterComponent->FindItemUnderView(ViewLocation, Through), Item);

    // Закрытый пестерь взгляду не отвечает.
    PC->PesterComponent->Close();
    TestTrue(TEXT("Закрытый -- мимо"), PC->PesterComponent->FindItemUnderView(ViewLocation, Through) == nullptr);

    PC->Destroy();
    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistDiegetic_OpeningAWindowClosesThePester,
    "Herbalist.Diegetic.OpeningAWindowClosesThePester",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistDiegetic_OpeningAWindowClosesThePester::RunTest(const FString& Parameters)
{
    // Ревью 2026-09-21: окно (стол, хранилище, Травник, заказы) могло
    // открыться поверх пестеря, и после него игрок видел забытый пестерь перед
    // носом. Пестерь закрывается сам, едва открыто любое окно.
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;
    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("Manager spawned"), Manager)) return false;
    AHerbalistPlayerController* PC = SpawnControllerAndBeginPlay(World, Manager);
    if (!TestNotNull(TEXT("Controller spawned"), PC)) { Manager->Destroy(); return false; }

    PC->Inventory();
    TestTrue(TEXT("Пестерь раскрыт"), PC->PesterComponent->IsOpen());

    PC->bIsAnyWidgetOpen = true;   // любое окно -- флаг один на все
    PC->PesterComponent->TickComponent(0.1f, LEVELTICK_All, nullptr);
    TestFalse(TEXT("Окно открылось -- пестерь закрылся"), PC->PesterComponent->IsOpen());
    TestEqual(TEXT("Раскладка убрана"), PC->PesterComponent->GetLaidOutItems().Num(), 0);

    PC->bIsAnyWidgetOpen = false;
    PC->Destroy();
    Manager->Destroy();
    return true;
}

#endif // WITH_AUTOMATION_TESTS && WITH_EDITOR
