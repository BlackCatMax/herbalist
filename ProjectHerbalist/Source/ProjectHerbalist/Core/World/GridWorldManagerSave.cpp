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
            Saved.ResourceSlots.Add(ResourceActor->GetPlacementSlot());
        }
    }

    // Спящие ресурсы неактивного чанка (2026-09-03, стриминг): актора нет,
    // но растение есть -- без этой строки сохранение в момент, когда
    // игрок далеко, стирало бы весь дальний мир начисто.
    Saved.ResourceIngredientIDs.Append(Cell.DormantResourceIDs);
    for (int32 Index = 0; Index < Cell.DormantResourceIDs.Num(); ++Index)
    {
        Saved.ResourceSlots.Add(Cell.DormantResourceSlots.IsValidIndex(Index) ? Cell.DormantResourceSlots[Index] : INDEX_NONE);
    }

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
        if (const FGridCell* Cell = GetCellByGridIndex(Index))
        {
            Result.Add(CaptureCellState(*Cell));
        }
        else if (const FSavedCellState* Delta = UnloadedCellDeltas.Find(Index))
        {
            // Клетка выгруженной страницы (этап 8в) -- её отклонение в дельте.
            Result.Add(*Delta);
        }
    }

    return Result;
}

void AGridWorldManager::CopySavedCellFields(FGridCell& Cell, const FSavedCellState& Saved)
{
    Cell.State = Saved.State;
    Cell.TargetState = Saved.TargetState;
    Cell.HarvestStress = Saved.HarvestStress;
    Cell.Memory = Saved.Memory;
    Cell.ManifestedEntityID = Saved.ManifestedEntityID;
    Cell.bEternallyPure = Saved.bEternallyPure;
    Cell.PlantedSpeciesID = Saved.PlantedSpeciesID;
}

void AGridWorldManager::ApplyCellStateAndRespawnResources(FGridCell& Cell, const FSavedCellState& Saved)
{
    CopySavedCellFields(Cell, Saved);

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
    Cell.DormantResourceSlots.Empty();

    SpawnResourceRoster(Cell, Saved.ResourceIngredientIDs, Saved.ResourceSlots);
}

int32 AGridWorldManager::ApplySaveCells(const TArray<FSavedCellState>& InCells)
{
    TSet<int32> SavedIndices;
    SavedIndices.Reserve(InCells.Num());
    int32 DroppedCount = 0;
    // Чанки выгруженных страниц, чьи клетки сейв заменил или откатил: их
    // последние сводки устарели.
    TSet<FIntPoint> TouchedUnloadedChunks;

    for (const FSavedCellState& Saved : InCells)
    {
        if (!IsCellInGrid(Saved.X, Saved.Y))
        {
            // Одна строка на загрузку, а не на клетку (этап 8): после того как
            // убрали плитку ландшафта, таких клеток тысячи.
            ++DroppedCount;
            continue;
        }

        const int32 GridIndex = GetCellIndex(Saved.X, Saved.Y);
        SavedIndices.Add(GridIndex);
        if (FGridCell* Cell = GetCell(Saved.X, Saved.Y))
        {
            ApplyCellStateAndRespawnResources(*Cell, Saved);
        }
        else
        {
            // Страница выгружена (этап 8в): сохранённое ложится в её дельту и
            // встанет на место при загрузке; ростер и засев -- из сейва.
            UnloadedCellDeltas.Add(GridIndex, Saved);
            UnloadedCellRosters.Remove(GridIndex);
            if (SeededCellMask.IsValidIndex(GridIndex))
            {
                SeededCellMask[GridIndex] = false;
            }
            TouchedUnloadedChunks.Add(GetChunkCoordForCell(Saved.X, Saved.Y));
        }
    }

    // Откат клеток, тронутых ПОСЛЕ момента сейва (аудит 2026-09-05, решение
    // пользователя: полноценный baseline на клетку, не тихое игнорирование).
    // DirtyCellIndices — монотонный набор (только .Add(), никогда не
    // очищается, см. довод у объявления в GridWorldManager.h): если клетка
    // грязная СЕЙЧАС, но отсутствует в самом сейве, значит на МОМЕНТ
    // сохранения она ещё ни разу не была тронута — то есть в точности
    // равнялась снимку своей страницы (Baselines), снятому в InitializeCells до единого
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
        FGridCell* Cell = GetCellByGridIndex(Index);
        if (!Cell)
        {
            // Страница выгружена (этап 8в): откат к основе -- просто без
            // дельты, ростера и засева, основа пересчитается при загрузке.
            UnloadedCellDeltas.Remove(Index);
            UnloadedCellRosters.Remove(Index);
            if (SeededCellMask.IsValidIndex(Index))
            {
                SeededCellMask[Index] = false;
            }
            const FIntPoint GridMin = GetGridMinCell();
            TouchedUnloadedChunks.Add(GetChunkCoordForCell(GridMin.X + Index % GridSizeX, GridMin.Y + Index / GridSizeX));
            continue;
        }
        const FSavedCellState* Baseline = FindCellBaselineByGridIndex(Index);
        if (!Baseline) continue;

        ApplyCellStateAndRespawnResources(*Cell, *Baseline);
    }

    // DirtyCellIndices живёт только в памяти этой сессии и не сохраняется
    // само по себе — приравниваем его РОВНО к набору сейва (не объединяем с
    // тем, что было до загрузки): те же правила, что были бы у свежей
    // сессии, загрузившей этот же сейв с нуля. Дальнейшая игра после
    // загрузки продолжит помечать клетки как обычно поверх этого набора.
    DirtyCellIndices = MoveTemp(SavedIndices);

    // Клетки заменены сейвом и базой -- сводки загруженных чанков считаются
    // заново (этап 7). У выгруженных -- последние, кроме тех, чьи клетки сейв
    // тронул: иначе загрузка сейва пересобирала бы весь выгруженный мир по
    // основе (ревью этапа 8в).
    EnsureChunkSummaryCacheKey();
    for (const FHerbalistCellPage& Page : CellPages)
    {
        if (!Page.bLoaded)
        {
            continue;
        }
        FIntPoint MinChunk;
        FIntPoint MaxChunk;
        GetPageChunkRange(Page, MinChunk, MaxChunk);
        for (int32 ChunkY = MinChunk.Y; ChunkY <= MaxChunk.Y; ++ChunkY)
        {
            for (int32 ChunkX = MinChunk.X; ChunkX <= MaxChunk.X; ++ChunkX)
            {
                StaleChunkSummaries.Add(FIntPoint(ChunkX, ChunkY));
            }
        }
    }
    StaleChunkSummaries.Append(TouchedUnloadedChunks);

    if (DroppedCount > 0)
    {
        UE_LOG(LogHerbalistSave, Warning, TEXT("ApplySaveCells: %d saved cells lie outside the grid and were dropped (landscape tiles removed?)"), DroppedCount);
    }
    return DroppedCount;
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
