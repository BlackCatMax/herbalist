// Source/ProjectHerbalistTests/Private/Tests/SaveSystemTest.cpp
//
// Проверка Core/Save/ headless-скриптами через -ExecCmds в -game оказалась
// ненадёжной (гонка с загрузкой World Partition, часть команд молча не
// доходит до диспетчера) — см. историю сессии. Здесь то же самое проверяется
// через сам движковый автотест-раннер (уже доказанно надёжный: 8/8 в этом
// проекте), вызывая C++ API AGridWorldManager напрямую, без консоли и exec.
//
// Область теста — именно то, что переписано по замечанию "не резать по
// живому": DirtyCellIndices должен быть по-настоящему разреженным (не вся
// сетка) и переживать capture/apply без потерь. UHerbalistSaveSubsystem
// (сериализация на диск через UGameplayStatics) не тестируется здесь: ему
// нужен UGameInstance, которого нет у голого editor-мира вне PIE — это
// стандартная, многократно проверенная машинерия движка (см. Tom Looman,
// "Unreal Engine C++ Save System"), а не наша логика.

#include "Core/World/GridWorldManager.h"
#include "Core/Save/HerbalistSaveTypes.h"
#include "Core/Simulation/Public/DeltaTypes.h"
#include "Misc/AutomationTest.h"
#include "Editor.h"
#include "Engine/World.h"

#if WITH_AUTOMATION_TESTS && WITH_EDITOR

namespace
{
    // Прямой вызов виртуального BeginPlay() пропускает состояние-машину движка
    // (AActor::ActorHasBegunPlay выставляется в DispatchBeginPlay ДО вызова
    // BeginPlay) и валит ensure "ActorHasBegunPlay == BeginningPlay". Правильный
    // публичный API именно для этого случая — динамически заспавненный актор вне
    // обычного старта уровня/PIE — DispatchBeginPlay(), не голый BeginPlay().
    AGridWorldManager* SpawnAndBeginPlay(UWorld* World)
    {
        if (!World) return nullptr;
        AGridWorldManager* Manager = World->SpawnActor<AGridWorldManager>();
        if (Manager)
        {
            Manager->DispatchBeginPlay();
        }
        return Manager;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistSave_CaptureIsSparse,
    "Herbalist.Save.CaptureIsSparse",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistSave_CaptureIsSparse::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;

    // Ничего не трогали — DirtyCellIndices должен быть пуст, а не вся сетка
    // (20x20 = 400 клеток по умолчанию).
    TestEqual(TEXT("Untouched world has zero dirty cells"), Manager->CaptureSaveCells().Num(), 0);

    // Трогаем ровно одну клетку тем же каналом, что и реальный пайплайн/
    // релаксация — ApplyStateDelta (см. ApplyStateDelta MarkCellDirty).
    FStateDelta Delta;
    FGridCell Touched;
    if (const FGridCell* Original = Manager->GetCellConst(3, 4))
    {
        Touched = *Original;
    }
    Touched.State.Meta.Corruption = 0.9f;
    Delta.WorldChanges.Add(FIntPoint(3, 4), Touched);
    Manager->ApplyStateDelta(Delta);

    const TArray<FSavedCellState> Captured = Manager->CaptureSaveCells();
    TestEqual(TEXT("Exactly one dirty cell after touching one"), Captured.Num(), 1);
    if (Captured.Num() == 1)
    {
        TestEqual(TEXT("Dirty cell is the one touched (X)"), Captured[0].X, 3);
        TestEqual(TEXT("Dirty cell is the one touched (Y)"), Captured[0].Y, 4);
        TestEqual(TEXT("Captured state matches applied state"), Captured[0].State.Meta.Corruption, 0.9f);
    }

    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistSave_ApplyRestoresEarlierSnapshot,
    "Herbalist.Save.ApplyRestoresEarlierSnapshot",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistSave_ApplyRestoresEarlierSnapshot::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;

    // t0: собираем зелье (условно) на клетке (2,2) — Corruption 0.4
    FStateDelta DeltaAtSave;
    FGridCell CellAtSave;
    if (const FGridCell* Original = Manager->GetCellConst(2, 2))
    {
        CellAtSave = *Original;
    }
    CellAtSave.State.Meta.Corruption = 0.4f;
    DeltaAtSave.WorldChanges.Add(FIntPoint(2, 2), CellAtSave);
    Manager->ApplyStateDelta(DeltaAtSave);

    // "SaveGame" — снимок клетки, как она есть прямо сейчас.
    const TArray<FSavedCellState> Snapshot = Manager->CaptureSaveCells();
    TestEqual(TEXT("One dirty cell captured for the save"), Snapshot.Num(), 1);

    // t1: игрок играет дальше без сохранения — то же место портится сильнее.
    FStateDelta DeltaAfterSave;
    FGridCell CellAfterSave = CellAtSave;
    CellAfterSave.State.Meta.Corruption = 0.95f;
    DeltaAfterSave.WorldChanges.Add(FIntPoint(2, 2), CellAfterSave);
    Manager->ApplyStateDelta(DeltaAfterSave);

    if (const FGridCell* Live = Manager->GetCellConst(2, 2))
    {
        TestEqual(TEXT("Live state reflects post-save play before loading"), Live->State.Meta.Corruption, 0.95f);
    }

    // "LoadGame" — откатываем к снимку t0.
    Manager->ApplySaveCells(Snapshot);

    if (const FGridCell* Restored = Manager->GetCellConst(2, 2))
    {
        TestEqual(TEXT("LoadGame restores the state at save time, not post-save play"), Restored->State.Meta.Corruption, 0.4f);
    }

    Manager->Destroy();
    return true;
}

#endif // WITH_AUTOMATION_TESTS && WITH_EDITOR
