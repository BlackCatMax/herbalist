// Source/ProjectHerbalistTests/Private/Tests/InteractFocusTest.cpp
//
// Одна клавиша, одна цель (2026-09-27, решение пользователя по образцу
// CustomizableInteractionPlugin): цель под прицелом ищет подсветка взгляда
// (ULookHighlightComponent::RefreshFocus) -- прямой луч, а мимо -- сфера
// «помощи в наведении»; «Взаимодействие» бьёт ровно в неё. Пестерь и пояс
// держат поворот, запомненный при открытии, -- мышь водит прицелом по ним
// (Player/ViewAnchor.h).

#include "Player/HerbalistPlayerController.h"
#include "Player/LookHighlightComponent.h"
#include "Player/HeldItemComponent.h"
#include "Player/BeltComponent.h"
#include "Player/BeltItemActor.h"
#include "Player/ViewAnchor.h"
#include "Core/Community/OfferingStoneActor.h"
#include "Core/Resources/AHerbalistResourceActor.h"
#include "Core/Inventory/HerbalistInventoryComponent.h"
#include "Core/World/GridWorldManager.h"
#include "Misc/AutomationTest.h"
#include "Editor.h"
#include "Engine/World.h"

#if WITH_AUTOMATION_TESTS && WITH_EDITOR

#include "TestWorldHelpers.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistFocus_AnchorYawFollowsOnlyPastTheEdge,
    "Herbalist.Focus.AnchorYawFollowsOnlyPastTheEdge",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistFocus_AnchorYawFollowsOnlyPastTheEdge::RunTest(const FString& Parameters)
{
    using HerbalistView::FollowAnchorYaw;
    TestEqual(TEXT("Взгляд в пределах края -- раскладка стоит"), FollowAnchorYaw(10.0f, 40.0f), 10.0f);
    TestEqual(TEXT("И в другую сторону"), FollowAnchorYaw(10.0f, -30.0f), 10.0f);
    TestEqual(TEXT("За краем -- подтягивается ровно на край"), FollowAnchorYaw(0.0f, 60.0f), 15.0f);
    TestEqual(TEXT("За краем влево"), FollowAnchorYaw(0.0f, -60.0f), -15.0f);
    // Через ±180: 170 и -170 -- соседи, а не противоположности.
    TestEqual(TEXT("Через границу 180 -- в пределах края"), FollowAnchorYaw(170.0f, -170.0f), 170.0f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistFocus_AssistCatchesNearMiss,
    "Herbalist.Focus.AssistCatchesNearMiss",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistFocus_AssistCatchesNearMiss::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;
    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("Manager spawned"), Manager)) return false;
    AHerbalistPlayerController* PC = SpawnControllerAndBeginPlay(World, Manager);
    if (!TestNotNull(TEXT("Controller spawned"), PC)) { Manager->Destroy(); return false; }
    ULookHighlightComponent* Look = PC->LookHighlightComponent;

    // Высоко над картой -- чтобы луч не встретил ничего с уровня.
    static_cast<AActor*>(PC)->SetActorLocation(FVector(0.0f, 0.0f, 90000.0f));
    PC->SetControlRotation(FRotator::ZeroRotator);
    FVector View;
    FRotator ViewRotation;
    PC->GetPlayerViewPoint(View, ViewRotation);
    const FVector Forward = ViewRotation.Vector();
    const FVector Right = FRotationMatrix(ViewRotation).GetUnitAxis(EAxis::Y);

    // Камень с зоной 60 см: прямо по лучу -- цель.
    AOfferingStoneActor* Stone = World->SpawnActor<AOfferingStoneActor>(AOfferingStoneActor::StaticClass(),
        View + Forward * 300.0f, FRotator::ZeroRotator);
    if (!TestNotNull(TEXT("Камень"), Stone)) { PC->Destroy(); Manager->Destroy(); return false; }
    TestEqual(TEXT("Прямо по лучу -- камень в фокусе"), Look->RefreshFocus(), static_cast<AActor*>(Stone));

    // Сдвинуть на 75 см вбок: луч проходит в 15 см от зоны -- ловит сфера помощи (20 см).
    Stone->SetActorLocation(View + Forward * 300.0f + Right * 75.0f);
    TestEqual(TEXT("Промах на 15 см -- помощь в наведении ловит"), Look->RefreshFocus(), static_cast<AActor*>(Stone));

    // На метр вбок -- уже не цель.
    Stone->SetActorLocation(View + Forward * 300.0f + Right * 160.0f);
    TestNull(TEXT("Далеко вбок -- цели нет"), Look->RefreshFocus());

    Stone->Destroy();
    PC->Destroy();
    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistFocus_BusyHandDoesNotPickThePlant,
    "Herbalist.Focus.BusyHandDoesNotPickThePlant",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistFocus_BusyHandDoesNotPickThePlant::RunTest(const FString& Parameters)
{
    // Одна клавиша на всё: с предметом в руке растение под прицелом не
    // срывается -- промах мимо цели для предмета не должен рвать куст.
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;
    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("Manager spawned"), Manager)) return false;
    AHerbalistPlayerController* PC = SpawnControllerAndBeginPlay(World, Manager);
    if (!TestNotNull(TEXT("Controller spawned"), PC)) { Manager->Destroy(); return false; }

    static_cast<AActor*>(PC)->SetActorLocation(FVector(0.0f, 0.0f, 90000.0f));
    PC->SetControlRotation(FRotator::ZeroRotator);
    FVector View;
    FRotator ViewRotation;
    PC->GetPlayerViewPoint(View, ViewRotation);
    AHerbalistResourceActor* Plant = World->SpawnActor<AHerbalistResourceActor>(AHerbalistResourceActor::StaticClass(),
        View + ViewRotation.Vector() * 150.0f, FRotator::ZeroRotator);
    if (!TestNotNull(TEXT("Растение"), Plant)) { PC->Destroy(); Manager->Destroy(); return false; }

    FInventoryItem Herb;
    Herb.IngredientID = FName(TEXT("bol_01"));
    Herb.Count = 1;
    PC->InventoryComponent->AddItem(Herb, 1);
    PC->HeldItemComponent->TakeFromInventory(PC->InventoryComponent->FindItemIndex(Herb));
    TestEqual(TEXT("Растение под прицелом"), PC->LookHighlightComponent->RefreshFocus(), static_cast<AActor*>(Plant));

    PC->Interact();
    TestFalse(TEXT("Рука занята -- не сорвано"), Plant->IsBeingHarvested());
    TestTrue(TEXT("Предмет всё ещё в руке"), PC->HeldItemComponent->IsHolding());

    Plant->Destroy();
    PC->Destroy();
    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistFocus_BeltHoldsStillUnderTheMouse,
    "Herbalist.Focus.BeltHoldsStillUnderTheMouse",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistFocus_BeltHoldsStillUnderTheMouse::RunTest(const FString& Parameters)
{
    // Пояс не поворачивается вместе со взглядом: мышь водит прицелом по
    // заглушкам. Ушёл за край -- пояс подтянулся.
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;
    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("Manager spawned"), Manager)) return false;
    AHerbalistPlayerController* PC = SpawnControllerAndBeginPlay(World, Manager);
    if (!TestNotNull(TEXT("Controller spawned"), PC)) { Manager->Destroy(); return false; }
    UBeltComponent* Belt = PC->BeltComponent;

    PC->SetControlRotation(FRotator(-70.0f, 0.0f, 0.0f));
    Belt->TickComponent(0.1f, LEVELTICK_All, nullptr);
    if (!TestTrue(TEXT("Пояс виден"), Belt->GetShownItems().Num() > 0)) { PC->Destroy(); Manager->Destroy(); return false; }
    const ABeltItemActor* First = Belt->GetShownItems()[0].Get();
    const FVector Before = First->GetActorLocation();

    PC->SetControlRotation(FRotator(-70.0f, 25.0f, 0.0f));
    Belt->TickComponent(0.1f, LEVELTICK_All, nullptr);
    TestTrue(TEXT("Повернул мышью на 25° -- пояс на месте"), First->GetActorLocation().Equals(Before, 0.5f));

    PC->SetControlRotation(FRotator(-70.0f, 120.0f, 0.0f));
    Belt->TickComponent(0.1f, LEVELTICK_All, nullptr);
    TestFalse(TEXT("Отвернулся за край -- пояс подтянулся"), First->GetActorLocation().Equals(Before, 0.5f));

    PC->Destroy();
    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistFocus_HintLineRemembersWithoutViewport,
    "Herbalist.Focus.HintLineRemembersWithoutViewport",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistFocus_HintLineRemembersWithoutViewport::RunTest(const FString& Parameters)
{
    // Строка «Котомка полна» (2026-09-27): без вьюпорта -- только запоминается,
    // без падений на создании виджета.
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;
    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("Manager spawned"), Manager)) return false;
    AHerbalistPlayerController* PC = SpawnControllerAndBeginPlay(World, Manager);
    if (!TestNotNull(TEXT("Controller spawned"), PC)) { Manager->Destroy(); return false; }

    PC->ShowHintLine(TEXT("Котомка полна."));
    TestEqual(TEXT("Строка запомнена"), PC->GetLastHintLine(), FString(TEXT("Котомка полна.")));

    PC->Destroy();
    Manager->Destroy();
    return true;
}

#endif
