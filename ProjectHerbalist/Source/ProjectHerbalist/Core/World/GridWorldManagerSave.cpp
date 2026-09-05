// Core/World/GridWorldManagerSave.cpp
//
// Сохранения v1 (Core/Save/HerbalistSaveTypes.h) — сбор/применение того, что
// в клетке отличается от детерминированной генерации: State/TargetState/
// HarvestStress/Memory/ManifestedEntityID и фактический ростер заспавненных
// ресурсов. Biome/вода/высота ландшафта не трогаются — InitializeCells уже
// восстановит их сама, тем же RngBaseSeed (см. комментарий в HerbalistSaveTypes.h).

#include "Core/World/GridWorldManager.h"
#include "Core/Save/HerbalistSaveTypes.h"
#include "Core/Resources/AHerbalistResourceActor.h"
#include "Core/Storage/StorageContainer.h"
#include "Core/Storage/AlchemyTableActor.h"
#include "EngineUtils.h"
#include "ProjectHerbalist.h"
#include "HerbalistLogChannels.h"

FSavedCellState AGridWorldManager::CaptureCellState(const FGridCell& Cell)
{
    FSavedCellState Saved;
    Saved.X = Cell.X;
    Saved.Y = Cell.Y;
    Saved.State = Cell.State;
    Saved.TargetState = Cell.TargetState;
    Saved.HarvestStress = Cell.HarvestStress;
    Saved.Memory = Cell.Memory;
    Saved.ManifestedEntityID = Cell.ManifestedEntityID;
    Saved.bEternallyPure = Cell.bEternallyPure;
    Saved.PlantedSpeciesID = Cell.PlantedSpeciesID;
    Saved.bResourcesSeeded = Cell.bResourcesSeeded;

    for (const TWeakObjectPtr<AHerbalistResourceActor>& ResourceActor : Cell.ResourceActors)
    {
        if (ResourceActor.IsValid())
        {
            Saved.ResourceIngredientIDs.Add(ResourceActor->GetIngredientID());
        }
    }

    // Спящие ресурсы неактивного чанка (2026-09-03, стриминг): актора нет,
    // но растение есть -- без этой строки сохранение в момент, когда
    // игрок далеко, стирало бы весь дальний мир начисто.
    Saved.ResourceIngredientIDs.Append(Cell.DormantResourceIDs);

    return Saved;
}

TArray<FSavedCellState> AGridWorldManager::CaptureSaveCells() const
{
    // Только тронутые клетки (DirtyCellIndices), не вся сетка — тот же принцип,
    // что у Skyrim ("changed forms") и Valheim (ZDO только для тронутых зон):
    // нетронутый мир воспроизводится заново из RngBaseSeed, сохранять его незачем.
    TArray<FSavedCellState> Result;
    Result.Reserve(DirtyCellIndices.Num());

    for (int32 Index : DirtyCellIndices)
    {
        if (!Cells.IsValidIndex(Index)) continue;
        Result.Add(CaptureCellState(Cells[Index]));
    }

    return Result;
}

void AGridWorldManager::ApplyCellStateAndRespawnResources(FGridCell& Cell, const FSavedCellState& Saved)
{
    Cell.State = Saved.State;
    Cell.TargetState = Saved.TargetState;
    Cell.HarvestStress = Saved.HarvestStress;
    Cell.Memory = Saved.Memory;
    Cell.ManifestedEntityID = Saved.ManifestedEntityID;
    Cell.bEternallyPure = Saved.bEternallyPure;
    Cell.PlantedSpeciesID = Saved.PlantedSpeciesID;

    // Аудит 2026-09-05: без этого поля клетка, уже собранная/пересеянная
    // в предыдущей сессии, при первой активации своего чанка получала бы
    // СВЕЖИЙ случайный бросок (UpdateStreamingChunks проверяет именно
    // bResourcesSeeded, чтобы решить "сеять заново" или "поднять
    // сохранённый DormantResourceIDs-ростер") вместо только что
    // применённого выше ростера — восстановленное состояние стиралось бы
    // на первом же приближении игрока к этой клетке.
    Cell.bResourcesSeeded = Saved.bResourcesSeeded;

    // Заменяем ростер ресурсов на сохранённый, а не оставляем тот, что
    // InitializeCells уже успела заспавнить броском кубика при BeginPlay —
    // собранное игроком не должно молча вернуться после загрузки.
    // WasSpawnedByGrid() (аудит 2026-09-05, тот же класс защиты, что уже
    // UpdateStreamingChunks) -- чужие акторы (PCG-граф) сетке не
    // принадлежат, их стримит сам World Partition; раньше эта проверка
    // здесь отсутствовала, и загрузка могла уничтожить актор, который ей
    // не принадлежит.
    for (const TWeakObjectPtr<AHerbalistResourceActor>& ResourceActor : Cell.ResourceActors)
    {
        if (ResourceActor.IsValid() && ResourceActor->WasSpawnedByGrid())
        {
            ResourceActor->Destroy();
        }
    }
    Cell.ResourceActors.Empty();

    // Аудит 2026-09-05: без явной очистки здесь ростер спящих ресурсов
    // этой ЖЕ ЖИВОЙ сессии (загрузка "не путешествует по уровням",
    // см. UHerbalistSaveSubsystem::LoadGame — WorldManager мог уже
    // потикать стриминг ДО вызова LoadGame) остаётся рядом с только что
    // заспавненными ниже актуальными акторами. Когда чанк снова уйдёт в
    // простой, UpdateStreamingChunks ДОБАВИТ их ID в этот же массив, не
    // заменит — итог: растительность дублируется на каждый цикл
    // сейв/стриминг.
    Cell.DormantResourceIDs.Empty();

    for (FName IngredientID : Saved.ResourceIngredientIDs)
    {
        SpawnResourceActor(IngredientID, Cell.X, Cell.Y);
    }
}

void AGridWorldManager::ApplySaveCells(const TArray<FSavedCellState>& InCells)
{
    TSet<int32> SavedIndices;
    SavedIndices.Reserve(InCells.Num());

    for (const FSavedCellState& Saved : InCells)
    {
        FGridCell* Cell = GetCell(Saved.X, Saved.Y);
        if (!Cell)
        {
            UE_LOG(LogHerbalistSave, Warning, TEXT("ApplySaveCells: no cell at (%d,%d), skipped (grid size mismatch?)"), Saved.X, Saved.Y);
            continue;
        }

        SavedIndices.Add(GetCellIndex(Saved.X, Saved.Y));
        ApplyCellStateAndRespawnResources(*Cell, Saved);
    }

    // Откат клеток, тронутых ПОСЛЕ момента сейва (аудит 2026-09-05, решение
    // пользователя: полноценный baseline на клетку, не тихое игнорирование).
    // DirtyCellIndices — монотонный набор (только .Add(), никогда не
    // очищается, см. довод у объявления в GridWorldManager.h): если клетка
    // грязная СЕЙЧАС, но отсутствует в самом сейве, значит на МОМЕНТ
    // сохранения она ещё ни разу не была тронута — то есть в точности
    // равнялась CellBaselines[Index], снятому в InitializeCells до единого
    // действия игрока. Это не приближение, а точный факт: единственные пути
    // пометить клетку грязной (ApplyStateDelta/OnResourceCollected/
    // StartResourceRegrowth/проявление сущностей/Заряна/перья Жар-птицы)
    // все явно происходят ПОСЛЕ момента, когда клетка перестаёт совпадать с
    // детерминированной генерацией — активация чанка стримингом сама по
    // себе клетку не пачкает (комментарий у OnResourceCollected: "исходный
    // бросок... безопасно переигрывается заново из RngBaseSeed").
    for (int32 Index : DirtyCellIndices)
    {
        if (SavedIndices.Contains(Index)) continue;
        if (!Cells.IsValidIndex(Index) || !CellBaselines.IsValidIndex(Index)) continue;

        ApplyCellStateAndRespawnResources(Cells[Index], CellBaselines[Index]);
    }

    // DirtyCellIndices живёт только в памяти этой сессии и не сохраняется
    // само по себе — приравниваем его РОВНО к набору сейва (не объединяем с
    // тем, что было до загрузки): те же правила, что были бы у свежей
    // сессии, загрузившей этот же сейв с нуля. Дальнейшая игра после
    // загрузки продолжит помечать клетки как обычно поверх этого набора.
    DirtyCellIndices = MoveTemp(SavedIndices);
}

TArray<FSavedHomeStorage> AGridWorldManager::CaptureHomeStorages() const
{
    // TActorIterator, не отдельный список -- у AStorageContainer нет ни
    // одного постоянного держателя ссылки (ни здесь, ни на контроллере, см.
    // BuildHomeStorage/SpawnHomeStorageContainer) -- тот же путь, что уже
    // использует сам BuildHomeStorage при проверке "такой тип уже есть".
    TArray<FSavedHomeStorage> Result;
    for (TActorIterator<AStorageContainer> It(GetWorld()); It; ++It)
    {
        AStorageContainer* Container = *It;
        if (!Container || !Container->InventoryComponent) continue;

        FSavedHomeStorage Saved;
        Saved.ContainerType = Container->InventoryComponent->ContainerType;
        Saved.Items = Container->InventoryComponent->GetItems();
        Result.Add(MoveTemp(Saved));
    }
    return Result;
}

void AGridWorldManager::RestoreHomeStorages(const TArray<FSavedHomeStorage>& InStorages)
{
    if (InStorages.Num() == 0) return;

    // Клетка-якорь дома -- ровно та же логика поиска, что уже
    // AHerbalistPlayerController::BuildHomeStorage использует при постройке:
    // первый AAlchemyTableActor в мире, не хранимая отдельно позиция (тот же
    // довод, что и у Shrine.Cell -- контент уровня, не рантайм-состояние).
    AAlchemyTableActor* Table = nullptr;
    for (TActorIterator<AAlchemyTableActor> It(GetWorld()); It; ++It)
    {
        Table = *It;
        break;
    }
    if (!Table)
    {
        UE_LOG(LogHerbalistSave, Warning, TEXT("RestoreHomeStorages: no alchemy table (home anchor) in world, %d storages skipped"), InStorages.Num());
        return;
    }
    const FIntPoint AnchorCell = Table->GetGridCoords();

    // Уничтожаем уже существующие домашние хранилища ПЕРЕД восстановлением —
    // тот же принцип, что уже ApplySaveCells делает с ResourceActors выше:
    // загрузка происходит в уже живой сессии (не путешествует по уровням,
    // см. UHerbalistSaveSubsystem::LoadGame), WorldManager мог успеть
    // построить хранилище САМ (BuildHomeStorage) ещё до вызова LoadGame —
    // без этой очистки восстановление плодило бы дубликаты того же типа.
    for (TActorIterator<AStorageContainer> It(GetWorld()); It; ++It)
    {
        if (AStorageContainer* Existing = *It)
        {
            Existing->Destroy();
        }
    }

    for (const FSavedHomeStorage& Saved : InStorages)
    {
        AStorageContainer* Container = SpawnHomeStorageContainer(AnchorCell, Saved.ContainerType);
        if (Container && Container->InventoryComponent)
        {
            Container->InventoryComponent->RestoreItems(Saved.Items);
        }
    }
}
