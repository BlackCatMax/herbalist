// Source/ProjectHerbalistTests/Private/Tests/GatheringToolOwnershipTest.cpp
//
// Инструменты сбора как физические предметы инвентаря (DESIGN_Community_
// And_Homestead.md §2.3, "полировка" 2026-09-06) — раньше SetGatheringTool
// переключал ЛЮБОЙ инструмент без всякой проверки владения (GatheringToolTest.cpp
// проверяет только сам МНОЖИТЕЛЬ качества, минуя резолв). Этот файл проверяет
// новый резолв по инвентарю: Железный серп выдаётся стартовым инвентарём
// (AHerbalistPlayerController::BeginPlay), Медный серп/Костяной нож нужно
// реально иметь при себе. Тот же DispatchBeginPlay-паттерн, что уже
// ArtifactInventoryTest.cpp применяет к AHerbalistPlayerController.

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

// CountItemsWithID -- вынесено в TestWorldHelpers.h (ODR-дубликат с
// ArtifactInventoryTest.cpp).

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistGatheringToolOwnership_StartingInventoryGrantsIronBlade,
    "Herbalist.GatheringToolOwnership.StartingInventoryGrantsIronBlade",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistGatheringToolOwnership_StartingInventoryGrantsIronBlade::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("Manager spawned"), Manager)) return false;
    AHerbalistPlayerController* PC = SpawnControllerAndBeginPlay(World, Manager);
    if (!TestNotNull(TEXT("Controller spawned"), PC)) { Manager->Destroy(); return false; }

    TestEqual(TEXT("BeginPlay grants exactly one Железный серп"),
        CountItemsWithID(PC->InventoryComponent, FName(TEXT("Железный серп"))), 1);

    PC->SetGatheringTool(TEXT("iron"));
    TestEqual(TEXT("Switching to iron succeeds -- item is owned from the start"),
        PC->CurrentGatheringTool, EGatheringTool::IronBlade);

    Manager->Destroy();
    PC->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistGatheringToolOwnership_MissingToolRefusesAndLeavesCurrentToolUntouched,
    "Herbalist.GatheringToolOwnership.MissingToolRefusesAndLeavesCurrentToolUntouched",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistGatheringToolOwnership_MissingToolRefusesAndLeavesCurrentToolUntouched::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("Manager spawned"), Manager)) return false;
    AHerbalistPlayerController* PC = SpawnControllerAndBeginPlay(World, Manager);
    if (!TestNotNull(TEXT("Controller spawned"), PC)) { Manager->Destroy(); return false; }

    // Ни Медного серпа, ни Костяного ножа в инвентаре нет -- только стартовый
    // Железный серп/Корзина.
    PC->SetGatheringTool(TEXT("copper"));
    TestEqual(TEXT("Switching to copper without owning one is refused"),
        PC->CurrentGatheringTool, EGatheringTool::BareHands);

    PC->SetGatheringTool(TEXT("bone"));
    TestEqual(TEXT("Switching to bone without owning one is refused"),
        PC->CurrentGatheringTool, EGatheringTool::BareHands);

    Manager->Destroy();
    PC->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistGatheringToolOwnership_OwnedCopperBladeSwitchesSuccessfully,
    "Herbalist.GatheringToolOwnership.OwnedCopperBladeSwitchesSuccessfully",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistGatheringToolOwnership_OwnedCopperBladeSwitchesSuccessfully::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("Manager spawned"), Manager)) return false;
    AHerbalistPlayerController* PC = SpawnControllerAndBeginPlay(World, Manager);
    if (!TestNotNull(TEXT("Controller spawned"), PC)) { Manager->Destroy(); return false; }

    FInventoryItem CopperBlade;
    CopperBlade.IngredientID = FName(TEXT("Медный серп"));
    CopperBlade.Count = 1;
    PC->InventoryComponent->AddItem(CopperBlade);

    PC->SetGatheringTool(TEXT("copper"));
    TestEqual(TEXT("Switching to copper succeeds once owned"),
        PC->CurrentGatheringTool, EGatheringTool::CopperBlade);

    // Не расходуется переключением -- тот же принцип, что уже у активации
    // оберегов/экипировки контейнеров.
    TestEqual(TEXT("Switching does not consume the item"),
        CountItemsWithID(PC->InventoryComponent, FName(TEXT("Медный серп"))), 1);

    Manager->Destroy();
    PC->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistGatheringToolOwnership_BareHandsAlwaysAvailable,
    "Herbalist.GatheringToolOwnership.BareHandsAlwaysAvailable",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistGatheringToolOwnership_BareHandsAlwaysAvailable::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("Manager spawned"), Manager)) return false;
    AHerbalistPlayerController* PC = SpawnControllerAndBeginPlay(World, Manager);
    if (!TestNotNull(TEXT("Controller spawned"), PC)) { Manager->Destroy(); return false; }

    PC->SetGatheringTool(TEXT("iron"));
    TestEqual(TEXT("Sanity: switched away from BareHands first"), PC->CurrentGatheringTool, EGatheringTool::IronBlade);

    PC->SetGatheringTool(TEXT("hands"));
    TestEqual(TEXT("Switching to bare hands never needs an item"), PC->CurrentGatheringTool, EGatheringTool::BareHands);

    Manager->Destroy();
    PC->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistGatheringToolOwnership_DataTableRowsCarryToolFlags,
    "Herbalist.GatheringToolOwnership.DataTableRowsCarryToolFlags",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistGatheringToolOwnership_DataTableRowsCarryToolFlags::RunTest(const FString& Parameters)
{
    // Регрессия на GatheringToolAppendCommandlet -- сами ряды DT_IngredientClass
    // реально несут bIsGatheringTool/GatheringToolType/bIsSilverWard, не
    // просто существуют с DisplayName. Тот же приём инъекции GameInstance,
    // что уже ArtifactInventoryTest.cpp::DataTableRowsResolveDisplayNames.
    UGameInstance* OwnerGameInstance = NewObject<UGameInstance>(GEngine);
    UIngredientRegistrySubsystem* Registry = NewObject<UIngredientRegistrySubsystem>(OwnerGameInstance);
    if (!TestNotNull(TEXT("Ingredient registry subsystem constructed"), Registry)) return false;

    UDataTable* Table = LoadObject<UDataTable>(nullptr, TEXT("/Game/Herbalist/Data/DT_IngredientClass"));
    if (!TestNotNull(TEXT("DT_IngredientClass loads"), Table)) return false;
    Registry->LoadFromDataTable(Table);

    const FIngredientTableRow* Iron = Registry->GetRow(FName(TEXT("Железный серп")));
    if (TestNotNull(TEXT("Железный серп has a registry row"), Iron))
    {
        TestTrue(TEXT("Железный серп is a gathering tool"), Iron->bIsGatheringTool);
        TestEqual(TEXT("Железный серп resolves to IronBlade"), Iron->GatheringToolType, EGatheringTool::IronBlade);
        TestFalse(TEXT("Железный серп is not a silver ward"), Iron->bIsSilverWard);
    }

    const FIngredientTableRow* Copper = Registry->GetRow(FName(TEXT("Медный серп")));
    if (TestNotNull(TEXT("Медный серп has a registry row"), Copper))
    {
        TestEqual(TEXT("Медный серп resolves to CopperBlade"), Copper->GatheringToolType, EGatheringTool::CopperBlade);
    }

    const FIngredientTableRow* Bone = Registry->GetRow(FName(TEXT("Костяной нож")));
    if (TestNotNull(TEXT("Костяной нож has a registry row"), Bone))
    {
        TestEqual(TEXT("Костяной нож resolves to BoneKnife"), Bone->GatheringToolType, EGatheringTool::BoneKnife);
        TestFalse(TEXT("Костяной нож never spawns via harvest (empty AllowedBiomes)"), Bone->AllowedBiomes.Num() > 0);
    }

    const FIngredientTableRow* Silver = Registry->GetRow(FName(TEXT("Серебряный оберег")));
    if (TestNotNull(TEXT("Серебряный оберег has a registry row"), Silver))
    {
        TestTrue(TEXT("Серебряный оберег is flagged as a silver ward"), Silver->bIsSilverWard);
        TestFalse(TEXT("Серебряный оберег is not a gathering tool (not a blade at all)"), Silver->bIsGatheringTool);
    }

    return true;
}

#endif // WITH_AUTOMATION_TESTS && WITH_EDITOR
