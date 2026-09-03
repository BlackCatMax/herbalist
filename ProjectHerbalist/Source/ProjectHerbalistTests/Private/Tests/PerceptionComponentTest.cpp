// Source/ProjectHerbalistTests/Private/Tests/PerceptionComponentTest.cpp
//
// UPerceptionComponent на живом AGridWorldManager (2026-09-03) — разбор
// жалобы пользователя на периодические просадки FPS ("просадка каждую
// секунду") нашёл, что TickComponent безусловно, каждые 0.5с, тратил
// ~500 000 TMap-вставок (Grid->CaptureState() + ComputePerceivedWorld) на
// вычисление AGridWorldManager::GetPerceivedWorld() -- у которого не было
// НИ ОДНОГО читателя во всём проекте (в отличие от GetPerceivedInventory,
// который реально используется InventorySlotWidget/ItemTooltipWidget).
// Мир больше не считается в тике -- только по требованию, внутри самого
// GetPerceivedWorld().
//
// ПОЧЕМУ ЗДЕСЬ НЕТ ТЕСТА НА САМ ЛЕНИВЫЙ РАСЧЁТ. Первая версия была --
// упала: GetPerceivedWorld() идёт через Simulation::FSnapshotService::
// CaptureWorld(), а та ищет мир через GetSimulationWorld(), который
// перебирает только FWorldContext с WorldType Game/PIE. Headless-тест живёт
// в EditorContext -- CaptureWorld() честно возвращает пустой снапшот, и
// GetPerceivedWorld() падает в ветку "мир ещё не инициализирован". Это не
// баг правки -- та же граница, что и у остального пайплайна (см.
// ShrineTest.cpp: "Интеграция с реальным пайплайном... проверена вручную
// построчно, отдельных автотестов нет"). Сама формула шума
// (ComputePerceivedWorld/PerceiveRealState) уже полностью покрыта
// PerceptionServiceTest.cpp на уровне чистой функции -- это как раз то, что
// автотест МОЖЕТ проверить не через PIE. Реальную PIE-проверку лени
// (компонент не топчется впустую в Tick, GetPerceivedWorld всё ещё отдаёт
// свежие данные по вызову) предстоит сделать вручную, как и остальной
// пайплайн.

#include "Core/World/GridWorldManager.h"
#include "Misc/AutomationTest.h"
#include "Editor.h"
#include "Engine/World.h"

#if WITH_AUTOMATION_TESTS && WITH_EDITOR

#include "TestWorldHelpers.h"

// GetCurrentWorldSeed() вынесен из CaptureState() (2026-09-03) специально,
// чтобы UPerceptionComponent мог получить тот же сид без дорогого полного
// захвата клеток. Регрессия: формула не должна была разъехаться при
// вынесении -- CaptureState() теперь зовёт GetCurrentWorldSeed() вместо
// повторения HashCombine на месте.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistPerceptionComponent_WorldSeedMatchesCaptureState,
    "Herbalist.PerceptionComponent.WorldSeedMatchesCaptureState",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistPerceptionComponent_WorldSeedMatchesCaptureState::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;

    Manager->SetCurrentTickID(7);
    const FWorldSnapshot Snapshot = Manager->CaptureState();

    TestEqual(TEXT("GetCurrentWorldSeed() matches the seed CaptureState() puts in the snapshot"),
        Manager->GetCurrentWorldSeed(), Snapshot.WorldSeed);

    Manager->Destroy();
    return true;
}

// GetPerceivedWorld() не должен падать, если мир (через GetSimulationWorld)
// недостижим -- ровно ситуация headless-теста выше, и ровно то состояние,
// в которое компонент попадает до первого реального Game/PIE-тика. Отдаёт
// валидный (пустой) результат, а не крашит и не возвращает мусор.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistPerceptionComponent_GetPerceivedWorldFailsSafeWithoutSimulationWorld,
    "Herbalist.PerceptionComponent.GetPerceivedWorldFailsSafeWithoutSimulationWorld",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistPerceptionComponent_GetPerceivedWorldFailsSafeWithoutSimulationWorld::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;

    // Editor-мир теста не найден GetSimulationWorld() (см. шапку файла) --
    // GetPerceivedWorld() обязан вернуть валидный указатель на пустой
    // результат, а не упасть и не вернуть nullptr.
    const FPerceivedWorld* Perceived = Manager->GetPerceivedWorld();
    if (TestNotNull(TEXT("GetPerceivedWorld() returns a valid pointer even with no reachable simulation world"), Perceived))
    {
        TestEqual(TEXT("No simulation world reachable -> empty perceived world, not garbage"), Perceived->Cells.Num(), 0);
    }

    Manager->Destroy();
    return true;
}

#endif // WITH_AUTOMATION_TESTS && WITH_EDITOR
