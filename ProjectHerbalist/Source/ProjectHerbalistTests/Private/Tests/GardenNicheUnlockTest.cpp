// Source/ProjectHerbalistTests/Private/Tests/GardenNicheUnlockTest.cpp
//
// Экономика пристроек сада (DESIGN_Community_And_Homestead.md §2.2/§2.4,
// "полировка" 2026-09-06, прямой запрос "Делаем сад") — тот же приём
// "мягкая прокачка" (материалы + отношения), что уже BuildHomeStorage
// (HomeStorageTest.cpp), только порог отношений — Molva общины, не Respect
// конкретного хозяина (сад стоит у жилища игрока, не привязан к хозяину
// места), и Пещера — исключение без порога Molva вовсе (решение
// пользователя). Точные материалы/числа — черновик, принятый пользователем
// как временный ("пока принимаем так, потом изменим"), см.
// GardenNicheUnlockTypes.h.

#include "Core/World/GridWorldManager.h"
#include "Core/World/GardenNicheUnlockTypes.h"
#include "Core/Inventory/HerbalistInventoryComponent.h"
#include "Player/HerbalistPlayerController.h"
#include "Misc/AutomationTest.h"
#include "Editor.h"
#include "Engine/World.h"

#if WITH_AUTOMATION_TESTS && WITH_EDITOR

#include "TestWorldHelpers.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistGardenNicheUnlock_RefusesBelowMolvaThresholdEvenWithMaterial,
    "Herbalist.GardenNicheUnlock.RefusesBelowMolvaThresholdEvenWithMaterial",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistGardenNicheUnlock_RefusesBelowMolvaThresholdEvenWithMaterial::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("Manager spawned"), Manager)) return false;
    AHerbalistPlayerController* PC = SpawnControllerAndBeginPlay(World, Manager);
    if (!TestNotNull(TEXT("Controller spawned"), PC)) { Manager->Destroy(); return false; }

    const FGardenNicheUnlockCost* Cost = FindGardenNicheUnlockCost(EGardenNiche::Pond);
    if (!TestNotNull(TEXT("Pond has an unlock recipe"), Cost)) { Manager->Destroy(); PC->Destroy(); return false; }

    Manager->Molva = Cost->MinMolva - 0.05f;   // ниже порога

    FInventoryItem Material;
    Material.IngredientID = Cost->MaterialIngredientID;
    Material.Count = Cost->MaterialCount;
    PC->InventoryComponent->AddItem(Material, Cost->MaterialCount);

    PC->SetGardenPlot(0, 0, TEXT("pond"));
    TestEqual(TEXT("Molva below threshold refuses the build even with material present"),
        Manager->GardenPlots.FindRef(FIntPoint(0, 0)), EGardenNiche::None);

    Manager->Destroy();
    PC->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistGardenNicheUnlock_RefusesWithoutEnoughMaterialEvenAboveMolvaThreshold,
    "Herbalist.GardenNicheUnlock.RefusesWithoutEnoughMaterialEvenAboveMolvaThreshold",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistGardenNicheUnlock_RefusesWithoutEnoughMaterialEvenAboveMolvaThreshold::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("Manager spawned"), Manager)) return false;
    AHerbalistPlayerController* PC = SpawnControllerAndBeginPlay(World, Manager);
    if (!TestNotNull(TEXT("Controller spawned"), PC)) { Manager->Destroy(); return false; }

    const FGardenNicheUnlockCost* Cost = FindGardenNicheUnlockCost(EGardenNiche::Pond);
    if (!TestNotNull(TEXT("Pond has an unlock recipe"), Cost)) { Manager->Destroy(); PC->Destroy(); return false; }

    Manager->Molva = Cost->MinMolva + 0.05f;   // выше порога -- Molva не проблема

    // Материала меньше нужного (не хватает одной единицы до полного стека).
    FInventoryItem NotEnough;
    NotEnough.IngredientID = Cost->MaterialIngredientID;
    NotEnough.Count = Cost->MaterialCount - 1;
    PC->InventoryComponent->AddItem(NotEnough, Cost->MaterialCount - 1);

    PC->SetGardenPlot(0, 0, TEXT("pond"));
    TestEqual(TEXT("Not enough material refuses the build even with Molva satisfied"),
        Manager->GardenPlots.FindRef(FIntPoint(0, 0)), EGardenNiche::None);

    Manager->Destroy();
    PC->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistGardenNicheUnlock_SucceedsAndConsumesExactMaterialCount,
    "Herbalist.GardenNicheUnlock.SucceedsAndConsumesExactMaterialCount",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistGardenNicheUnlock_SucceedsAndConsumesExactMaterialCount::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("Manager spawned"), Manager)) return false;
    AHerbalistPlayerController* PC = SpawnControllerAndBeginPlay(World, Manager);
    if (!TestNotNull(TEXT("Controller spawned"), PC)) { Manager->Destroy(); return false; }

    const FGardenNicheUnlockCost* Cost = FindGardenNicheUnlockCost(EGardenNiche::Pond);
    if (!TestNotNull(TEXT("Pond has an unlock recipe"), Cost)) { Manager->Destroy(); PC->Destroy(); return false; }

    Manager->Molva = Cost->MinMolva + 0.05f;

    FInventoryItem Material;
    Material.IngredientID = Cost->MaterialIngredientID;
    Material.Count = Cost->MaterialCount;
    PC->InventoryComponent->AddItem(Material, Cost->MaterialCount);

    PC->SetGardenPlot(0, 0, TEXT("pond"));
    TestEqual(TEXT("Niche registered once both Molva and material are satisfied"),
        Manager->GardenPlots.FindRef(FIntPoint(0, 0)), EGardenNiche::Pond);

    bool bHasAnyLeft = false;
    for (const FInventoryItem& Item : PC->InventoryComponent->GetItems())
    {
        if (Item.IngredientID == Cost->MaterialIngredientID && Item.Count > 0) bHasAnyLeft = true;
    }
    TestFalse(TEXT("Material fully consumed by the build"), bHasAnyLeft);

    Manager->Destroy();
    PC->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistGardenNicheUnlock_CaveIgnoresMolvaThreshold,
    "Herbalist.GardenNicheUnlock.CaveIgnoresMolvaThreshold",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistGardenNicheUnlock_CaveIgnoresMolvaThreshold::RunTest(const FString& Parameters)
{
    // Решение пользователя 2026-09-06: Пещера -- личная постройка, не
    // общинная, без порога Molva вовсе (MinMolva=-1.0f сентинел).
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("Manager spawned"), Manager)) return false;
    AHerbalistPlayerController* PC = SpawnControllerAndBeginPlay(World, Manager);
    if (!TestNotNull(TEXT("Controller spawned"), PC)) { Manager->Destroy(); return false; }

    const FGardenNicheUnlockCost* Cost = FindGardenNicheUnlockCost(EGardenNiche::Cave);
    if (!TestNotNull(TEXT("Cave has an unlock recipe"), Cost)) { Manager->Destroy(); PC->Destroy(); return false; }
    TestEqual(TEXT("Sanity: Cave's MinMolva is the no-threshold sentinel"), Cost->MinMolva, -1.0f);

    Manager->Molva = -0.95f;   // худшая возможная община, всё ещё >= -1.0f

    FInventoryItem Material;
    Material.IngredientID = Cost->MaterialIngredientID;
    Material.Count = Cost->MaterialCount;
    PC->InventoryComponent->AddItem(Material, Cost->MaterialCount);

    PC->SetGardenPlot(0, 0, TEXT("cave"));
    TestEqual(TEXT("Cave builds even at near-worst Molva -- personal dig, not a community favor"),
        Manager->GardenPlots.FindRef(FIntPoint(0, 0)), EGardenNiche::Cave);

    Manager->Destroy();
    PC->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistGardenNicheUnlock_RefusesDuplicateWithoutConsumingMaterial,
    "Herbalist.GardenNicheUnlock.RefusesDuplicateWithoutConsumingMaterial",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistGardenNicheUnlock_RefusesDuplicateWithoutConsumingMaterial::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("Manager spawned"), Manager)) return false;
    AHerbalistPlayerController* PC = SpawnControllerAndBeginPlay(World, Manager);
    if (!TestNotNull(TEXT("Controller spawned"), PC)) { Manager->Destroy(); return false; }

    const FGardenNicheUnlockCost* Cost = FindGardenNicheUnlockCost(EGardenNiche::SunnyBed);
    if (!TestNotNull(TEXT("SunnyBed has an unlock recipe"), Cost)) { Manager->Destroy(); PC->Destroy(); return false; }

    Manager->Molva = Cost->MinMolva + 0.05f;
    Manager->GardenPlots.Add(FIntPoint(0, 0), EGardenNiche::SunnyBed);   // уже построена напрямую

    FInventoryItem Material;
    Material.IngredientID = Cost->MaterialIngredientID;
    Material.Count = Cost->MaterialCount;
    PC->InventoryComponent->AddItem(Material, Cost->MaterialCount);

    PC->SetGardenPlot(0, 0, TEXT("sunny"));

    bool bStillHasFullStack = false;
    for (const FInventoryItem& Item : PC->InventoryComponent->GetItems())
    {
        if (Item.IngredientID == Cost->MaterialIngredientID && Item.Count >= Cost->MaterialCount) bStillHasFullStack = true;
    }
    TestTrue(TEXT("Re-building the same already-built niche is refused, material untouched"), bStillHasFullStack);

    Manager->Destroy();
    PC->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistGardenNicheUnlock_ClearingAPlotIsAlwaysFree,
    "Herbalist.GardenNicheUnlock.ClearingAPlotIsAlwaysFree",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistGardenNicheUnlock_ClearingAPlotIsAlwaysFree::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("Manager spawned"), Manager)) return false;
    AHerbalistPlayerController* PC = SpawnControllerAndBeginPlay(World, Manager);
    if (!TestNotNull(TEXT("Controller spawned"), PC)) { Manager->Destroy(); return false; }

    // Molva худшая, ни грамма материала -- "none" всё равно обязан пройти.
    Manager->Molva = -1.0f;
    Manager->GardenPlots.Add(FIntPoint(0, 0), EGardenNiche::SunnyBed);

    PC->SetGardenPlot(0, 0, TEXT("none"));
    TestEqual(TEXT("Clearing a plot never checks Molva/material"),
        Manager->GardenPlots.FindRef(FIntPoint(0, 0)), EGardenNiche::None);

    Manager->Destroy();
    PC->Destroy();
    return true;
}

#endif // WITH_AUTOMATION_TESTS && WITH_EDITOR
