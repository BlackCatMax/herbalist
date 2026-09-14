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
#include "Core/Types/BiomeTypes.h"
#include "Core/Resources/AHerbalistResourceActor.h"
#include "Core/Storage/StorageContainer.h"
#include "Core/Storage/AlchemyTableActor.h"
#include "Core/Storage/DryingRackActor.h"
#include "Misc/AutomationTest.h"
#include "Editor.h"
#include "Engine/World.h"
#include "EngineUtils.h"

#if WITH_AUTOMATION_TESTS && WITH_EDITOR

#include "TestWorldHelpers.h"

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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistSave_ApplyRevertsCellsTouchedAfterSaveToBaseline,
    "Herbalist.Save.ApplyRevertsCellsTouchedAfterSaveToBaseline",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistSave_ApplyRevertsCellsTouchedAfterSaveToBaseline::RunTest(const FString& Parameters)
{
    // Аудит 2026-09-05, находка 7 ("загрузка не откатывает клетки, тронутые
    // ПОСЛЕ момента сейва") -- решение пользователя: полноценный baseline на
    // клетку (снимок страницы, снятый в InitializeCells), не тихое
    // игнорирование. Сценарий: клетка ЧИСТАЯ на момент сейва (в самом сейве
    // её нет) -> после сейва, без нового сохранения, её трогают -> загрузка
    // старого сейва обязана откатить её к тому, чем она была ДО того, как
    // её вообще коснулись -- не оставить пост-сейвовую правку как есть.
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;

    // Клетка (9,9) ещё ничем не тронута -- её текущий Corruption равен её
    // же будущему baseline'у (снятому InitializeCells до этого теста).
    const FGridCell* Untouched99 = Manager->GetCellConst(9, 9);
    if (!TestNotNull(TEXT("Cell (9,9) exists"), Untouched99)) { Manager->Destroy(); return false; }
    const float BaselineCorruption99 = Untouched99->State.Meta.Corruption;

    // t0: трогаем ДРУГУЮ клетку (2,2) -- она попадёт в сейв, (9,9) в нём не
    // будет вовсе (осталась чистой).
    FStateDelta DeltaAtSave;
    FGridCell CellAtSave;
    if (const FGridCell* Original = Manager->GetCellConst(2, 2)) { CellAtSave = *Original; }
    CellAtSave.State.Meta.Corruption = 0.4f;
    DeltaAtSave.WorldChanges.Add(FIntPoint(2, 2), CellAtSave);
    Manager->ApplyStateDelta(DeltaAtSave);

    const TArray<FSavedCellState> Snapshot = Manager->CaptureSaveCells();
    TestEqual(TEXT("Only (2,2) captured -- (9,9) still clean at save time"), Snapshot.Num(), 1);

    // t1: игрок играет ДАЛЬШЕ без нового сохранения -- трогает именно (9,9),
    // которой не было в сейве.
    FGridCell CellAfterSave99;
    if (const FGridCell* Live99 = Manager->GetCellConst(9, 9)) { CellAfterSave99 = *Live99; }
    CellAfterSave99.State.Meta.Corruption = 0.99f;
    FStateDelta DeltaAfterSave;
    DeltaAfterSave.WorldChanges.Add(FIntPoint(9, 9), CellAfterSave99);
    Manager->ApplyStateDelta(DeltaAfterSave);

    if (const FGridCell* Live99 = Manager->GetCellConst(9, 9))
    {
        TestEqual(TEXT("Live (9,9) reflects post-save play before loading"), Live99->State.Meta.Corruption, 0.99f);
    }

    // "LoadGame" -- откатываем к сейву, где (9,9) ещё не встречается.
    Manager->ApplySaveCells(Snapshot);

    if (const FGridCell* Restored99 = Manager->GetCellConst(9, 9))
    {
        TestEqual(TEXT("(9,9), тронутая ПОСЛЕ сейва, откатилась к своему baseline'у, а не осталась на 0.99"),
            Restored99->State.Meta.Corruption, BaselineCorruption99);
    }
    if (const FGridCell* Restored22 = Manager->GetCellConst(2, 2))
    {
        TestEqual(TEXT("(2,2), бывшая В сейве, по-прежнему восстановлена из него"),
            Restored22->State.Meta.Corruption, 0.4f);
    }

    // DirtyCellIndices после загрузки должен равняться ровно набору сейва --
    // (9,9) больше не "грязная" (откатилась до неотличимости от нетронутой).
    const TArray<FSavedCellState> AfterLoad = Manager->CaptureSaveCells();
    TestEqual(TEXT("После загрузки грязна снова только (2,2) -- (9,9) больше не отслеживается"),
        AfterLoad.Num(), 1);
    if (AfterLoad.Num() == 1)
    {
        TestEqual(TEXT("Единственная грязная клетка после загрузки -- (2,2)"), AfterLoad[0].X, 2);
        TestEqual(TEXT("Единственная грязная клетка после загрузки -- (2,2)"), AfterLoad[0].Y, 2);
    }

    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistSave_BiomeInfluencesWithZeroFieldsStaySparse,
    "Herbalist.Save.BiomeInfluencesWithZeroFieldsStaySparse",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistSave_BiomeInfluencesWithZeroFieldsStaySparse::RunTest(const FString& Parameters)
{
    // Аудит 2026-08-24 (AUDIT_AND_REFACTORING_PLAN.md §7.1): ApplyBiomeInfluences
    // проверял "есть ли запись в MorokFields/ZaryanaFields", а не "ненулевое ли
    // значение" -- а UBiomeGraphSubsystem::ApplyFieldsToGrid всегда строит запись
    // для КАЖДОГО узла графа, независимо от величины поля. Значит в реальной игре
    // (Tick -> StepSimulation -> ApplyFieldsToGrid, не редкое событие) каждая
    // клетка сетки помечалась грязной с первого шага, даже когда Морок/Заряна
    // ещё не успели ничего сдвинуть -- обесценивая липкий DirtyCellIndices,
    // вокруг которого построена вся система сохранений.
    //
    // Этот тест воспроизводит ровно то, что строит ApplyFieldsToGrid, когда поля
    // ещё на нулевой отметке (типичное начало сессии): запись для каждого биома,
    // значение 0.0.
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;

    TestEqual(TEXT("Untouched world has zero dirty cells"), Manager->CaptureSaveCells().Num(), 0);

    TMap<FName, float> MorokFields, ZaryanaFields;
    for (EBiomeType Biome : FBiomeDefaults::GetAllBiomeTypes())
    {
        const FName BiomeID = FBiomeDefaults::BiomeTypeToName(Biome);
        MorokFields.Add(BiomeID, 0.0f);
        ZaryanaFields.Add(BiomeID, 0.0f);
    }
    Manager->ApplyBiomeInfluences(MorokFields, ZaryanaFields, 1.0f, 0.2f);

    TestEqual(TEXT("Zero-valued fields for every biome must not dirty the whole grid"),
        Manager->CaptureSaveCells().Num(), 0);

    Manager->Destroy();
    return true;
}

// ---------------------------------------------------------------------------
// Аудит 2026-09-05, кластер "Сохранения" -- четыре находки ниже.
// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistSave_ApplyDoesNotDestroyNonGridSpawnedResourceActor,
    "Herbalist.Save.ApplyDoesNotDestroyNonGridSpawnedResourceActor",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistSave_ApplyDoesNotDestroyNonGridSpawnedResourceActor::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;

    FGridCell* Cell = Manager->GetCell(6, 6);
    if (!TestNotNull(TEXT("Cell exists"), Cell)) { Manager->Destroy(); return false; }

    // Актор, поставленный PCG-графом напрямую (без Init()) -- bSpawnedByGrid
    // остаётся false, тот же класс актора, что и настоящие PCG-расстановки
    // (см. довод у AHerbalistResourceActor::WasSpawnedByGrid()).
    AHerbalistResourceActor* PCGActor = World->SpawnActor<AHerbalistResourceActor>();
    if (!TestNotNull(TEXT("PCG-style actor spawned"), PCGActor)) { Manager->Destroy(); return false; }
    PCGActor->SetIngredientID(FName(TEXT("Ромашка")));
    PCGActor->SetGridPosition(6, 6);
    TestFalse(TEXT("Actor not spawned by grid (no Init() call)"), PCGActor->WasSpawnedByGrid());
    Cell->ResourceActors.Add(PCGActor);

    FSavedCellState Saved;
    Saved.X = 6; Saved.Y = 6;
    // ResourceIngredientIDs пуст -- имитируем "клетка собрана дочиста" на
    // момент сейва.
    Manager->ApplySaveCells({ Saved });

    // Аудит 2026-09-05: раньше ApplySaveCells уничтожал ВСЁ в
    // Cell->ResourceActors без проверки владения -- чужой (PCG) актор гибнул
    // бы вместе со своими.
    TestTrue(TEXT("PCG-owned actor survives ApplySaveCells (симуляция им не владеет)"), IsValid(PCGActor));

    if (IsValid(PCGActor)) PCGActor->Destroy();
    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistSave_ApplyClearsStaleDormantResourceIDs,
    "Herbalist.Save.ApplyClearsStaleDormantResourceIDs",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistSave_ApplyClearsStaleDormantResourceIDs::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;

    FGridCell* Cell = Manager->GetCell(7, 7);
    if (!TestNotNull(TEXT("Cell exists"), Cell)) { Manager->Destroy(); return false; }

    // Имитируем "живая сессия уже потикала стриминг ДО LoadGame" (загрузка
    // не путешествует по уровням, см. UHerbalistSaveSubsystem::LoadGame) --
    // в клетке остался устаревший DormantResourceIDs с ПРЕЖНЕЙ активности
    // этой же сессии.
    Cell->DormantResourceIDs.Add(FName(TEXT("Ромашка")));

    FSavedCellState Saved;
    Saved.X = 7; Saved.Y = 7;
    Saved.ResourceIngredientIDs = { FName(TEXT("Зверобой")) };   // сейв говорит: только Зверобой

    Manager->ApplySaveCells({ Saved });

    // Аудит 2026-09-05: без явной очистки Ромашка осталась бы рядом -- при
    // следующем уходе клетки в простой актуальный Зверобой добавился бы
    // ВТОРЫМ элементом в тот же массив, дав дубликат при возврате игрока.
    TestEqual(TEXT("Устаревший DormantResourceIDs с прошлой активности очищен, не слит с новым"),
        Cell->DormantResourceIDs.Num(), 0);

    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistSave_ResourcesSeededSurvivesCaptureAndApply,
    "Herbalist.Save.ResourcesSeededSurvivesCaptureAndApply",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistSave_ResourcesSeededSurvivesCaptureAndApply::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;

    FGridCell* Cell = Manager->GetCell(8, 8);
    if (!TestNotNull(TEXT("Cell exists"), Cell)) { Manager->Destroy(); return false; }
    Cell->bResourcesSeeded = true;   // уже посещалась/собрана в "прошлой сессии"

    // Помечаем клетку грязной тем же каналом, что и реальный пайплайн, чтобы
    // она попала в CaptureSaveCells (та же схема, что и другие тесты файла).
    FStateDelta Delta;
    FGridCell Touched = *Cell;
    Touched.State.Meta.Corruption = 0.5f;
    Delta.WorldChanges.Add(FIntPoint(8, 8), Touched);
    Manager->ApplyStateDelta(Delta);

    const TArray<FSavedCellState> Captured = Manager->CaptureSaveCells();
    const FSavedCellState* SavedForCell = Captured.FindByPredicate([](const FSavedCellState& S) { return S.X == 8 && S.Y == 8; });
    if (!TestNotNull(TEXT("Cell (8,8) captured"), SavedForCell)) { Manager->Destroy(); return false; }
    TestTrue(TEXT("bResourcesSeeded=true captured"), SavedForCell->bResourcesSeeded);

    // "Свежая клетка новой сессии" -- bResourcesSeeded по умолчанию false.
    Manager->GetCell(8, 8)->bResourcesSeeded = false;

    Manager->ApplySaveCells(Captured);

    // Аудит 2026-09-05: без восстановления этого поля клетка при первой же
    // активации своего чанка получила бы СВЕЖИЙ случайный бросок вместо
    // только что применённого выше ростера ресурсов.
    TestTrue(TEXT("ApplySaveCells восстанавливает bResourcesSeeded, предотвращая новый случайный засев при активации чанка"),
        Manager->GetCellConst(8, 8)->bResourcesSeeded);

    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistSave_HomeStorageContentsSurviveCaptureAndRestore,
    "Herbalist.Save.HomeStorageContentsSurviveCaptureAndRestore",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistSave_HomeStorageContentsSurviveCaptureAndRestore::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    // Чистая база -- тот же приём изоляции, что уже HomeStorageTest.cpp
    // (Destroy() не гарантирует немедленное удаление из TActorIterator в том
    // же кадре, см. TestWorldHelpers.h).
    for (TActorIterator<AStorageContainer> It(World); It; ++It)
    {
        if (AStorageContainer* Stale = *It) { Stale->Destroy(); }
    }
    for (TActorIterator<AAlchemyTableActor> It(World); It; ++It)
    {
        if (AAlchemyTableActor* Stale = *It) { Stale->Destroy(); }
    }

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;

    const FVector TablePos = Manager->GetCellWorldPosition(4, 4);
    AAlchemyTableActor* Table = World->SpawnActor<AAlchemyTableActor>(AAlchemyTableActor::StaticClass(), TablePos, FRotator::ZeroRotator);
    if (!TestNotNull(TEXT("Alchemy table spawned"), Table)) { Manager->Destroy(); return false; }
    Table->DispatchBeginPlay();

    AStorageContainer* Cellar = Manager->SpawnHomeStorageContainer(Table->GetGridCoords(), EStorageContainerType::Cellar);
    if (!TestNotNull(TEXT("Cellar spawned"), Cellar) || !TestNotNull(TEXT("Cellar has inventory"), Cellar->InventoryComponent))
    {
        Table->Destroy(); Manager->Destroy(); return false;
    }
    FInventoryItem Herb;
    Herb.IngredientID = FName(TEXT("Ромашка"));
    Herb.Count = 5;
    Cellar->InventoryComponent->AddItem(Herb, 5);

    const TArray<FSavedHomeStorage> Captured = Manager->CaptureHomeStorages();
    TestEqual(TEXT("Одно домашнее хранилище захвачено"), Captured.Num(), 1);
    if (Captured.Num() == 1)
    {
        TestEqual(TEXT("Захваченный тип -- Погреб"), Captured[0].ContainerType, EStorageContainerType::Cellar);
        TestEqual(TEXT("Захвачен один стек предметов"), Captured[0].Items.Num(), 1);
        if (Captured[0].Items.Num() == 1)
        {
            TestEqual(TEXT("Захваченное количество"), Captured[0].Items[0].Count, 5);
        }
    }

    // "Новая сессия" -- уничтожаем существующий Погреб, как было бы после
    // рестарта игры (AStorageContainer нигде не отслеживается постоянным
    // списком, единственный источник истины после рестарта -- сам сейв).
    Cellar->Destroy();

    Manager->RestoreHomeStorages(Captured);

    auto CountCellarsAndVerify = [&](const TCHAR* Context) -> int32
    {
        int32 Count = 0;
        for (TActorIterator<AStorageContainer> It(World); It; ++It)
        {
            AStorageContainer* Container = *It;
            if (!Container || !Container->InventoryComponent) continue;
            if (Container->InventoryComponent->ContainerType != EStorageContainerType::Cellar) continue;
            ++Count;
            TestEqual(FString::Printf(TEXT("%s: восстановленное хранилище содержит тот же один стек"), Context),
                Container->InventoryComponent->GetItems().Num(), 1);
            if (Container->InventoryComponent->GetItems().Num() == 1)
            {
                TestEqual(FString::Printf(TEXT("%s: восстановленное количество совпадает"), Context),
                    Container->InventoryComponent->GetItems()[0].Count, 5);
            }
        }
        return Count;
    };

    TestEqual(TEXT("Восстановлен ровно один Погреб"), CountCellarsAndVerify(TEXT("Первое восстановление")), 1);

    // Повторный LoadGame в той же живой сессии (тот же путь, что уже
    // ApplySaveCells поддерживает для клеток) не должен плодить дубликаты --
    // RestoreHomeStorages обязана уничтожить уже существующие хранилища
    // перед пересозданием.
    Manager->RestoreHomeStorages(Captured);
    TestEqual(TEXT("Повторное восстановление в той же сессии не плодит второй Погреб"), CountCellarsAndVerify(TEXT("Повторное восстановление")), 1);

    Table->Destroy();
    Manager->Destroy();
    return true;
}

static int32 CountHomeStoragesForSaveTest(UWorld* World, EStorageContainerType Type)
{
    int32 Count = 0;
    for (TActorIterator<AStorageContainer> It(World); It; ++It)
    {
        AStorageContainer* Container = *It;
        if (Container && Container->bIsHomeStorage && Container->InventoryComponent && Container->InventoryComponent->ContainerType == Type)
        {
            ++Count;
        }
    }
    return Count;
}

static void DestroyStorageAndTablesForSaveTest(UWorld* World)
{
    for (TActorIterator<AStorageContainer> It(World); It; ++It)
    {
        if (AStorageContainer* Stale = *It) { Stale->Destroy(); }
    }
    for (TActorIterator<AAlchemyTableActor> It(World); It; ++It)
    {
        if (AAlchemyTableActor* Stale = *It) { Stale->Destroy(); }
    }
}

// Сундуки и станции карты (2026-09-14): раньше попадали в HomeStorages, и
// загрузка уничтожала их, пересоздавая у стола голыми хранилищами. Теперь
// актор остаётся тем же, из сейва -- только содержимое по имени актора.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistSave_PlacedContainersKeepActorAndContents,
    "Herbalist.Save.PlacedContainersKeepActorAndContents",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistSave_PlacedContainersKeepActorAndContents::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;
    DestroyStorageAndTablesForSaveTest(World);

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;

    AAlchemyTableActor* Table = World->SpawnActor<AAlchemyTableActor>(AAlchemyTableActor::StaticClass(), Manager->GetCellWorldPosition(4, 4), FRotator::ZeroRotator);
    if (!TestNotNull(TEXT("Alchemy table spawned"), Table)) { Manager->Destroy(); return false; }
    Table->DispatchBeginPlay();

    ADryingRackActor* Rack = World->SpawnActor<ADryingRackActor>(ADryingRackActor::StaticClass(), Manager->GetCellWorldPosition(6, 6), FRotator::ZeroRotator);
    AStorageContainer* Cellar = Manager->SpawnHomeStorageContainer(Table->GetGridCoords(), EStorageContainerType::Cellar);
    if (!TestNotNull(TEXT("Сушилка на карте"), Rack) || !TestNotNull(TEXT("Погреб построен"), Cellar)
        || !TestNotNull(TEXT("У сушилки инвентарь"), Rack->InventoryComponent) || !TestNotNull(TEXT("У погреба инвентарь"), Cellar->InventoryComponent))
    {
        DestroyStorageAndTablesForSaveTest(World); Manager->Destroy(); return false;
    }
    TestFalse(TEXT("Сушилка карты -- не домашнее хранилище"), Rack->bIsHomeStorage);
    TestTrue(TEXT("Построенный погреб -- домашнее хранилище"), Cellar->bIsHomeStorage);

    FInventoryItem Herb;
    Herb.IngredientID = FName(TEXT("Ромашка"));
    Herb.Count = 3;
    Rack->InventoryComponent->AddItem(Herb, 3);
    Cellar->InventoryComponent->AddItem(Herb, 3);

    const TArray<FSavedHomeStorage> Home = Manager->CaptureHomeStorages();
    TestEqual(TEXT("В домашних -- только погреб"), Home.Num(), 1);

    const TArray<FSavedPlacedContainer> Placed = Manager->CapturePlacedContainers();
    const FSavedPlacedContainer* RackEntry = Placed.FindByPredicate([Rack](const FSavedPlacedContainer& Entry) { return Entry.ActorName == Rack->GetFName(); });
    if (TestNotNull(TEXT("Сушилка сохранена по имени актора"), RackEntry))
    {
        TestEqual(TEXT("Сохранено содержимое сушилки"), RackEntry->Items.Num(), 1);
    }
    TestNull(TEXT("Погреба среди контейнеров карты нет"),
        Placed.FindByPredicate([Cellar](const FSavedPlacedContainer& Entry) { return Entry.ActorName == Cellar->GetFName(); }));

    // Содержимое сушилки поменялось после сейва -- загрузка возвращает сохранённое.
    Rack->InventoryComponent->Clear();
    Manager->RestoreHomeStorages(Home);
    Manager->RestorePlacedContainers(Placed);

    TestFalse(TEXT("Сушилка -- тот же актор, загрузка её не уничтожила"), Rack->IsActorBeingDestroyed());
    TestEqual(TEXT("Содержимое сушилки восстановлено"), Rack->InventoryComponent->GetNumSlots(), 1);
    TestEqual(TEXT("Погреб один"), CountHomeStoragesForSaveTest(World, EStorageContainerType::Cellar), 1);

    DestroyStorageAndTablesForSaveTest(World);
    Manager->Destroy();
    return true;
}

// Контейнер карты, выгруженный World Partition (2026-09-14): выгрузка
// уничтожает актор, и без менеджера сундук возвращался пустым, а сейв вдали
// от него терял его содержимое.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistSave_UnloadedPlacedContainerKeepsContents,
    "Herbalist.Save.UnloadedPlacedContainerKeepsContents",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistSave_UnloadedPlacedContainerKeepsContents::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;
    DestroyStorageAndTablesForSaveTest(World);

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;

    FInventoryItem Herb;
    Herb.IngredientID = FName(TEXT("Ромашка"));
    Herb.Count = 2;

    // 1) Сейв знает контейнер, которого сейчас нет: содержимое ждёт у менеджера
    // и уходит в следующий сейв.
    FSavedPlacedContainer Far;
    Far.ActorName = FName(TEXT("SaveTestUnloadedChest"));
    Far.Items = { Herb };
    Manager->RestorePlacedContainers({ Far });
    const TArray<FSavedPlacedContainer> Recaptured = Manager->CapturePlacedContainers();
    const FSavedPlacedContainer* FarEntry = Recaptured.FindByPredicate([](const FSavedPlacedContainer& Entry) { return Entry.ActorName == FName(TEXT("SaveTestUnloadedChest")); });
    if (TestNotNull(TEXT("Невыгруженный контейнер остаётся в следующем сейве"), FarEntry))
    {
        TestEqual(TEXT("С тем же содержимым"), FarEntry->Items.Num(), 1);
    }

    // 2) Выгрузка отдаёт содержимое менеджеру. Сам EndPlay(RemovedFromWorld) в
    // редакторском мире не вызвать: акторы не инициализированы, RouteEndPlay
    // до него не доходит -- проверяется то, что EndPlay зовёт.
    AStorageContainer* Chest = World->SpawnActor<AStorageContainer>(AStorageContainer::StaticClass(), Manager->GetCellWorldPosition(3, 3), FRotator::ZeroRotator);
    if (!TestNotNull(TEXT("Сундук"), Chest)) { Manager->Destroy(); return false; }
    Chest->InventoryComponent->AddItem(Herb, 2);
    Chest->StashContentsForUnload();
    TArray<FInventoryItem> Claimed;
    TestTrue(TEXT("Выгруженный сундук оставил содержимое менеджеру"), Manager->ClaimPlacedContainerContents(Chest->GetFName(), Claimed));
    TestEqual(TEXT("То самое содержимое"), Claimed.Num(), 1);
    TestFalse(TEXT("Забранное второй раз не отдаётся"), Manager->ClaimPlacedContainerContents(Chest->GetFName(), Claimed));

    // 3) Загрузка: BeginPlay забирает своё.
    AStorageContainer* Reloaded = World->SpawnActor<AStorageContainer>(AStorageContainer::StaticClass(), Manager->GetCellWorldPosition(3, 4), FRotator::ZeroRotator);
    if (!TestNotNull(TEXT("Сундук после загрузки"), Reloaded)) { Chest->Destroy(); Manager->Destroy(); return false; }
    Manager->StashPlacedContainerContents(Reloaded->GetFName(), { Herb });
    Reloaded->DispatchBeginPlay();
    TestEqual(TEXT("Загруженный сундук забрал содержимое"), Reloaded->InventoryComponent->GetNumSlots(), 1);

    Chest->Destroy();
    Reloaded->Destroy();
    Manager->Destroy();
    return true;
}

// Записи сейва старее v7 о контейнерах карты (не cellar/cabinet/jar) дома не
// пересоздаются; сейв без хранилищ убирает построенное после него.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistSave_HomeStorageRestoreRebuildsOnlyHomeTypes,
    "Herbalist.Save.HomeStorageRestoreRebuildsOnlyHomeTypes",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistSave_HomeStorageRestoreRebuildsOnlyHomeTypes::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;
    DestroyStorageAndTablesForSaveTest(World);

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;

    AAlchemyTableActor* Table = World->SpawnActor<AAlchemyTableActor>(AAlchemyTableActor::StaticClass(), Manager->GetCellWorldPosition(4, 4), FRotator::ZeroRotator);
    if (!TestNotNull(TEXT("Alchemy table spawned"), Table)) { Manager->Destroy(); return false; }
    Table->DispatchBeginPlay();

    FSavedHomeStorage LegacyBasket;
    LegacyBasket.ContainerType = EStorageContainerType::Basket;
    FSavedHomeStorage Jar;
    Jar.ContainerType = EStorageContainerType::Jar;
    Manager->RestoreHomeStorages({ LegacyBasket, Jar });

    TestEqual(TEXT("Корзина из старого сейва дома не пересоздана"), CountHomeStoragesForSaveTest(World, EStorageContainerType::Basket), 0);
    TestEqual(TEXT("Кувшин пересоздан"), CountHomeStoragesForSaveTest(World, EStorageContainerType::Jar), 1);

    Manager->RestoreHomeStorages({});
    TestEqual(TEXT("Сейв без хранилищ убирает построенное"), CountHomeStoragesForSaveTest(World, EStorageContainerType::Jar), 0);

    DestroyStorageAndTablesForSaveTest(World);
    Manager->Destroy();
    return true;
}

#endif // WITH_AUTOMATION_TESTS && WITH_EDITOR
