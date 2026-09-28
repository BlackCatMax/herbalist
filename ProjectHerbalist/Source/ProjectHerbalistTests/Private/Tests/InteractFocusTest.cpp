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
#include "Player/PesterComponent.h"
#include "Player/PesterItemActor.h"
#include "Player/HeldItemActor.h"
#include "Core/Community/OfferingStoneActor.h"
#include "Core/Resources/AHerbalistResourceActor.h"
#include "Core/Inventory/HerbalistInventoryComponent.h"
#include "Core/World/GridWorldManager.h"
#include "Misc/AutomationTest.h"
#include "Editor.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/CollisionProfile.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Pawn.h"

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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistFocus_FarPlantIsNotHighlighted,
    "Herbalist.Focus.FarPlantIsNotHighlighted",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistFocus_FarPlantIsNotHighlighted::RunTest(const FString& Parameters)
{
    // Подсвечено -- значит сработает (2026-09-28, по PIE-логу: растение
    // горело за 10 м, а сбор отказывал «далеко -- 225 см»).
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;
    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("Manager spawned"), Manager)) return false;
    AHerbalistPlayerController* PC = SpawnControllerAndBeginPlay(World, Manager);
    if (!TestNotNull(TEXT("Controller spawned"), PC)) { Manager->Destroy(); return false; }
    APawn* Pawn = World->SpawnActor<APawn>(APawn::StaticClass(), FVector(0.0f, 0.0f, 90000.0f), FRotator::ZeroRotator);
    if (!TestNotNull(TEXT("Pawn"), Pawn)) { PC->Destroy(); Manager->Destroy(); return false; }
    PC->Possess(Pawn);
    PC->SetControlRotation(FRotator::ZeroRotator);

    FVector View;
    FRotator ViewRotation;
    PC->GetPlayerViewPoint(View, ViewRotation);
    AHerbalistResourceActor* Plant = World->SpawnActor<AHerbalistResourceActor>(AHerbalistResourceActor::StaticClass(),
        View + ViewRotation.Vector() * 150.0f, FRotator::ZeroRotator);
    if (!TestNotNull(TEXT("Растение"), Plant)) { Pawn->Destroy(); PC->Destroy(); Manager->Destroy(); return false; }
    TestEqual(TEXT("В пределах сбора -- подсвечено"), PC->LookHighlightComponent->RefreshFocus(), static_cast<AActor*>(Plant));

    Plant->SetActorLocation(View + ViewRotation.Vector() * (PC->MaxHarvestDistance + 100.0f));
    TestNull(TEXT("Дальше предела сбора -- не подсвечено"), PC->LookHighlightComponent->RefreshFocus());

    Plant->Destroy();
    Pawn->Destroy();
    PC->Destroy();
    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistFocus_BeltAimAssistCatchesNearMiss,
    "Herbalist.Focus.BeltAimAssistCatchesNearMiss",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistFocus_BeltAimAssistCatchesNearMiss::RunTest(const FString& Parameters)
{
    // Прицел чуть мимо заглушки пояса -- всё равно она (2026-09-28, по PIE:
    // «слишком мелкий трейс»). Далеко мимо -- ничего.
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
    FVector View;
    FRotator ViewRotation;
    PC->GetPlayerViewPoint(View, ViewRotation);
    const ABeltItemActor* First = Belt->GetShownItems()[0].Get();
    const FVector ToItem = (First->GetActorLocation() - View).GetSafeNormal();
    const FVector Side = FVector::CrossProduct(ToItem, FVector::UpVector).GetSafeNormal();

    const FVector NearMiss = ToItem.RotateAngleAxis(3.0f, FVector::CrossProduct(ToItem, Side).GetSafeNormal());
    TestNotNull(TEXT("На 3° мимо -- заглушка найдена"), Belt->FindItemUnderView(View, View + NearMiss * 200.0f));

    const FVector FarMiss = ToItem.RotateAngleAxis(40.0f, Side);
    TestNull(TEXT("На 40° мимо -- ничего"), Belt->FindItemUnderView(View, View + FarMiss * 200.0f));

    PC->Destroy();
    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistFocus_PesterItemNameWhileLookedAt,
    "Herbalist.Focus.PesterItemNameWhileLookedAt",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistFocus_PesterItemNameWhileLookedAt::RunTest(const FString& Parameters)
{
    // Имя предмета под взглядом (2026-09-28, решение пользователя): держится,
    // пока смотришь; ушёл взгляд -- ушло имя. Строку «взял в руку» на пару
    // секунд уход взгляда не снимает.
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;
    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("Manager spawned"), Manager)) return false;
    AHerbalistPlayerController* PC = SpawnControllerAndBeginPlay(World, Manager);
    if (!TestNotNull(TEXT("Controller spawned"), PC)) { Manager->Destroy(); return false; }

    FInventoryItem Herb;
    Herb.IngredientID = FName(TEXT("bol_01"));
    Herb.Count = 1;
    PC->InventoryComponent->AddItem(Herb, 1);
    PC->PesterComponent->Toggle();
    if (!TestTrue(TEXT("Пестерь разложен"), PC->PesterComponent->GetLaidOutItems().Num() > 0)) { PC->Destroy(); Manager->Destroy(); return false; }
    APesterItemActor* Item = PC->PesterComponent->GetLaidOutItems()[0].Get();
    const FString Name = PC->GetPerceivedDisplayName(Item->GetItem());

    PC->LookHighlightComponent->SetFocusedActor(Item);
    TestFalse(TEXT("Имя не пустое"), Name.IsEmpty());
    TestEqual(TEXT("Под взглядом -- имя"), PC->GetLastHintLine(), Name);
    TestTrue(TEXT("Держится без таймера"), PC->IsHintLineHeld());

    PC->LookHighlightComponent->SetFocusedActor(nullptr);
    TestTrue(TEXT("Взгляд ушёл -- имени нет"), PC->GetLastHintLine().IsEmpty());

    PC->LookHighlightComponent->SetFocusedActor(Item);
    PC->ShowHintLine(TEXT("Котомка полна."));
    PC->LookHighlightComponent->SetFocusedActor(nullptr);
    TestEqual(TEXT("Строку на пару секунд уход взгляда не снимает"), PC->GetLastHintLine(), FString(TEXT("Котомка полна.")));

    // Взяли последний из стопки, глядя на него: актор уничтожен раньше, чем
    // взгляд ушёл, -- имя всё равно снимается.
    PC->LookHighlightComponent->SetFocusedActor(Item);
    Item->Destroy();
    PC->LookHighlightComponent->SetFocusedActor(nullptr);
    TestTrue(TEXT("Предмет под взглядом исчез -- имени нет"), PC->GetLastHintLine().IsEmpty());

    PC->PesterComponent->Toggle();
    PC->Destroy();
    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistFocus_ItemShowsPlantMeshAtHandSize,
    "Herbalist.Focus.ItemShowsPlantMeshAtHandSize",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistFocus_ItemShowsPlantMeshAtHandSize::RunTest(const FString& Parameters)
{
    // Меш растения вместо шарика (2026-09-28): тот же размер горсти по
    // наибольшей стороне, в полтора раза крупнее шара. Зелье -- заглушка.
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;
    AHeldItemActor* Actor = World->SpawnActor<AHeldItemActor>();
    UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
    if (!TestTrue(TEXT("Актор и меш"), Actor && Cube)) { if (Actor) Actor->Destroy(); return false; }

    FInventoryItem Herb;
    Herb.IngredientID = FName(TEXT("bol_01"));
    Actor->SetShownSize(0.05f);
    Actor->ShowItem(Herb, false, false, Cube);
    TestTrue(TEXT("Показан меш растения"), Actor->IsShowingItemMesh());
    const float Longest = 2.0f * Cube->GetBounds().BoxExtent.GetMax();
    TestEqual(TEXT("Наибольшая сторона -- полторы горсти"), static_cast<float>(Actor->GetActorScale3D().X) * Longest, 0.05f * 1.5f * 100.0f, 0.01f);

    Actor->ShowItem(Herb, false, true, Cube);
    TestFalse(TEXT("Жидкость -- заглушка, не меш"), Actor->IsShowingItemMesh());
    TestEqual(TEXT("Заглушка -- размер как был"), static_cast<float>(Actor->GetActorScale3D().X), 0.05f, 0.0001f);

    FInventoryItem Potion;
    Potion.IngredientID = FName(TEXT("Potion"));
    TestNull(TEXT("У зелья меша растения нет"), AHeldItemActor::FindItemMesh(Actor, Potion));

    Actor->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistFocus_PotionKeyPoursOnlyThePotion,
    "Herbalist.Focus.PotionKeyPoursOnlyThePotion",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistFocus_PotionKeyPoursOnlyThePotion::RunTest(const FString& Parameters)
{
    // Клавиша зелья (2026-09-28, по PIE-логу: отладочный ApplyTest выливал два
    // первых предмета котомки -- ушли серп и корзина). Теперь -- только зелье.
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;
    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("Manager spawned"), Manager)) return false;
    AHerbalistPlayerController* PC = SpawnControllerAndBeginPlay(World, Manager);
    if (!TestNotNull(TEXT("Controller spawned"), PC)) { Manager->Destroy(); return false; }

    // Взгляд сверху на кубик в клетке (3,3) -- лучу есть во что попасть.
    const FVector Above = Manager->GetCellWorldPositionFlat(3, 3) + FVector(Manager->CellSize * 0.5f, Manager->CellSize * 0.5f, 0.0f);
    AStaticMeshActor* Floor = World->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), Above + FVector(0.0f, 0.0f, 100.0f), FRotator::ZeroRotator);
    UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
    if (!TestTrue(TEXT("Кубик"), Floor && Cube)) { PC->Destroy(); Manager->Destroy(); return false; }
    Floor->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
    Floor->GetStaticMeshComponent()->SetStaticMesh(Cube);
    Floor->GetStaticMeshComponent()->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
    Floor->SetActorScale3D(FVector(0.3f));
    static_cast<AActor*>(PC)->SetActorLocation(Above + FVector(0.0f, 0.0f, 500.0f));
    PC->SetControlRotation(FRotator(-89.9f, 0.0f, 0.0f));

    FInventoryItem Sickle;
    Sickle.IngredientID = FName(TEXT("bol_01"));
    Sickle.Count = 1;
    FInventoryItem Basket;
    Basket.IngredientID = FName(TEXT("tun_02"));
    Basket.Count = 1;
    FInventoryItem Potion;
    Potion.IngredientID = FName(TEXT("Potion"));
    Potion.Count = 1;
    Potion.State.Magnitude = 0.5f;
    PC->InventoryComponent->AddItem(Sickle, 1);
    PC->InventoryComponent->AddItem(Basket, 1);
    PC->InventoryComponent->AddItem(Potion, 1);

    PC->ApplyAlchemy();
    TestTrue(TEXT("Первый предмет котомки на месте"), PC->InventoryComponent->FindItemIndex(Sickle) != INDEX_NONE);
    TestTrue(TEXT("Второй предмет котомки на месте"), PC->InventoryComponent->FindItemIndex(Basket) != INDEX_NONE);
    TestEqual(TEXT("Зелье вылито"), PC->InventoryComponent->FindItemIndex(Potion), INDEX_NONE);

    Floor->Destroy();
    PC->Destroy();
    Manager->Destroy();
    return true;
}

#endif
