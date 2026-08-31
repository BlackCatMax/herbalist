// Source/ProjectHerbalistTests/Private/Tests/ResourceActorTest.cpp
//
// Полный аудит проекта (2026-08-31, по прямому запросу пользователя):
// Core/Resources (AHerbalistResourceActor) был единственной подсистемой,
// наравне с Core/Journal, без единого теста и без упоминания в ROADMAP.md
// как известного пробела.
//
// Область этого файла — жизненный цикл АКТОРА (Init/Harvest/IsBeingHarvested/
// снятие с клетки), не формула качества инструмента: та уже полностью
// покрыта GatheringToolTest.cpp напрямую через Simulation::ExecutePipeline
// (ToolQualityMultiplier/bIronAverse/bDelicate). Дублировать её здесь через
// ещё один слой (актор -> OnResourceCollected -> QueueCommand -> Tick ->
// пайплайн) было бы избыточно и потребовало бы решать неподтверждённый
// вопрос -- как реально дренируется очередь QueueCommand в инвентарь без
// AHerbalistPlayerController (которого ни один тест в проекте не поднимает,
// см. комментарий в CommunityTest.cpp). InitSetsAllFieldsCorrectly ниже
// проверяет ровно тот контракт, от которого зависит OnResourceCollected
// (GridWorldManagerCore.cpp): Cmd.Harvest.X = Actor->GetX() для каждого
// поля -- если геттеры отдают то, что положил Init(), связка корректна.

#include "Core/Resources/AHerbalistResourceActor.h"
#include "Core/World/GridWorldManager.h"
#include "Core/Types/HerbalistCoreTypes.h"
#include "Misc/AutomationTest.h"
#include "Editor.h"
#include "Engine/World.h"

#if WITH_AUTOMATION_TESTS && WITH_EDITOR

#include "TestWorldHelpers.h"

namespace
{
    FRealState MakeResourceProbeState()
    {
        FRealState State;
        State.Magnitude = 0.7f;
        State.Meta.Purity = 0.8f;
        State.Meta.Corruption = 0.1f;
        return State;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistResourceActor_InitSetsAllFieldsCorrectly,
    "Herbalist.ResourceActor.InitSetsAllFieldsCorrectly",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistResourceActor_InitSetsAllFieldsCorrectly::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AHerbalistResourceActor* Actor = World->SpawnActor<AHerbalistResourceActor>();
    if (!TestNotNull(TEXT("Resource actor spawned"), Actor)) return false;

    const FRealState ProbeState = MakeResourceProbeState();
    Actor->Init(FName(TEXT("Плакун-трава")), FText::FromString(TEXT("Плакун-трава")), nullptr,
        ProbeState, FVector::ZeroVector, nullptr, 4, 7,
        /*Resilience=*/0.6f, /*bIronAverse=*/true, /*bDelicate=*/false);

    TestEqual(TEXT("IngredientID set"), Actor->GetIngredientID(), FName(TEXT("Плакун-трава")));
    TestEqual(TEXT("GridX set"), Actor->GetGridX(), 4);
    TestEqual(TEXT("GridY set"), Actor->GetGridY(), 7);
    TestEqual(TEXT("Resilience set (feeds Cmd.Harvest.Resilience in OnResourceCollected)"), Actor->GetResilience(), 0.6f);
    TestTrue(TEXT("bIronAverse set (feeds Cmd.Harvest.bIronAverse)"), Actor->GetIsIronAverse());
    TestFalse(TEXT("bDelicate set"), Actor->GetIsDelicate());
    TestEqual(TEXT("BaseState.Magnitude set (feeds Cmd.Harvest.BaseState)"), Actor->GetBaseState().Magnitude, ProbeState.Magnitude);

    Actor->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistResourceActor_IsBeingHarvestedStartsFalse,
    "Herbalist.ResourceActor.IsBeingHarvestedStartsFalse",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistResourceActor_IsBeingHarvestedStartsFalse::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AHerbalistResourceActor* Actor = World->SpawnActor<AHerbalistResourceActor>();
    if (!TestNotNull(TEXT("Resource actor spawned"), Actor)) return false;

    TestFalse(TEXT("A freshly spawned actor is not mid-harvest"), Actor->IsBeingHarvested());

    Actor->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistResourceActor_HarvestWithoutWorldManagerIsANoOp,
    "Herbalist.ResourceActor.HarvestWithoutWorldManagerIsANoOp",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistResourceActor_HarvestWithoutWorldManagerIsANoOp::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AHerbalistResourceActor* Actor = World->SpawnActor<AHerbalistResourceActor>();
    if (!TestNotNull(TEXT("Resource actor spawned"), Actor)) return false;

    // WorldManager намеренно не задан (Init с InWorldManager=nullptr) --
    // Harvest() должен честно отказаться, не крашить и не помечать сбор
    // начатым (ранний return в AHerbalistResourceActor::Harvest()).
    Actor->Init(FName(TEXT("Probe")), FText(), nullptr, FRealState(), FVector::ZeroVector,
        nullptr, 5, 5);
    Actor->Harvest();

    TestFalse(TEXT("Harvest without a WorldManager doesn't crash and leaves IsBeingHarvested false"),
        Actor->IsBeingHarvested());

    Actor->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistResourceActor_HarvestWithInvalidCellIsANoOp,
    "Herbalist.ResourceActor.HarvestWithInvalidCellIsANoOp",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistResourceActor_HarvestWithInvalidCellIsANoOp::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("Manager spawned"), Manager)) { return false; }

    AHerbalistResourceActor* Actor = World->SpawnActor<AHerbalistResourceActor>();
    if (!TestNotNull(TEXT("Resource actor spawned"), Actor)) { Manager->Destroy(); return false; }

    // WorldManager реален, но клетка (999,999) заведомо вне сетки --
    // WorldManager->GetCell() вернёт nullptr, Harvest() должен отказаться.
    Actor->Init(FName(TEXT("Probe")), FText(), nullptr, FRealState(), FVector::ZeroVector,
        Manager, 999, 999);
    Actor->Harvest();

    TestFalse(TEXT("Harvest with an out-of-bounds cell doesn't crash and leaves IsBeingHarvested false"),
        Actor->IsBeingHarvested());

    Actor->Destroy();
    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistResourceActor_HarvestMarksInProgressAndRemovesFromCell,
    "Herbalist.ResourceActor.HarvestMarksInProgressAndRemovesFromCell",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistResourceActor_HarvestMarksInProgressAndRemovesFromCell::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("Manager spawned"), Manager)) return false;

    FGridCell* Cell = Manager->GetCell(5, 5);
    if (!TestNotNull(TEXT("Cell (5,5) exists"), Cell)) { Manager->Destroy(); return false; }

    AHerbalistResourceActor* Actor = World->SpawnActor<AHerbalistResourceActor>();
    if (!TestNotNull(TEXT("Resource actor spawned"), Actor)) { Manager->Destroy(); return false; }

    Actor->Init(FName(TEXT("Probe")), FText(), nullptr, FRealState(), FVector::ZeroVector,
        Manager, 5, 5);
    // AHerbalistResourceActor::Init() намеренно не регистрирует себя в
    // Cell->ResourceActors (это делает SpawnResourcesInCell на стороне
    // менеджера) -- воспроизводим то же предусловие вручную, как это было
    // бы в реальном мире на момент сбора.
    Cell->ResourceActors.Add(Actor);

    TestEqual(TEXT("Precondition: actor registered on the cell"), Cell->ResourceActors.Num(), 1);

    Actor->Harvest();

    TestTrue(TEXT("IsBeingHarvested becomes true"), Actor->IsBeingHarvested());
    TestEqual(TEXT("Actor removed from the cell's resource list (OnResourceCollected)"),
        Cell->ResourceActors.Num(), 0);

    // Второй Harvest() пока идёт первый -- должен тихо отказаться (ветка
    // "уже собирается"), не удалять из уже пустого списка повторно и не крашить.
    Actor->Harvest();
    TestTrue(TEXT("A second Harvest() call while in progress is a safe no-op"), Actor->IsBeingHarvested());
    TestEqual(TEXT("Cell's resource list stays empty, not negative/corrupted"), Cell->ResourceActors.Num(), 0);

    Actor->Destroy();
    Manager->Destroy();
    return true;
}

#endif // WITH_AUTOMATION_TESTS && WITH_EDITOR
