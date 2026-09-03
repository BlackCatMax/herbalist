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
#include "ProjectHerbalist.h"
#include "HerbalistLogChannels.h"

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
        const FGridCell& Cell = Cells[Index];

        FSavedCellState Saved;
        Saved.X = Cell.X;
        Saved.Y = Cell.Y;
        Saved.State = Cell.State;
        Saved.TargetState = Cell.TargetState;
        Saved.HarvestStress = Cell.HarvestStress;
        Saved.Memory = Cell.Memory;
        Saved.ManifestedEntityID = Cell.ManifestedEntityID;
        Saved.bEternallyPure = Cell.bEternallyPure;

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

        Result.Add(MoveTemp(Saved));
    }

    return Result;
}

void AGridWorldManager::ApplySaveCells(const TArray<FSavedCellState>& InCells)
{
    for (const FSavedCellState& Saved : InCells)
    {
        FGridCell* Cell = GetCell(Saved.X, Saved.Y);
        if (!Cell)
        {
            UE_LOG(LogHerbalistSave, Warning, TEXT("ApplySaveCells: no cell at (%d,%d), skipped (grid size mismatch?)"), Saved.X, Saved.Y);
            continue;
        }

        Cell->State = Saved.State;
        Cell->TargetState = Saved.TargetState;
        Cell->HarvestStress = Saved.HarvestStress;
        Cell->Memory = Saved.Memory;
        Cell->ManifestedEntityID = Saved.ManifestedEntityID;
        Cell->bEternallyPure = Saved.bEternallyPure;

        // Re-mark: DirtyCellIndices живёт только в памяти этой сессии и не
        // сохраняется само по себе. Без этого следующий SaveGame сразу после
        // LoadGame потерял бы всё только что восстановленное, кроме клеток,
        // тронутых заново после загрузки.
        MarkCellDirty(Saved.X, Saved.Y);

        // Заменяем ростер ресурсов на сохранённый, а не оставляем тот, что
        // InitializeCells уже успела заспавнить броском кубика при BeginPlay —
        // собранное игроком не должно молча вернуться после загрузки.
        for (const TWeakObjectPtr<AHerbalistResourceActor>& ResourceActor : Cell->ResourceActors)
        {
            if (ResourceActor.IsValid())
            {
                ResourceActor->Destroy();
            }
        }
        Cell->ResourceActors.Empty();

        for (FName IngredientID : Saved.ResourceIngredientIDs)
        {
            SpawnResourceActor(IngredientID, Saved.X, Saved.Y);
        }
    }
}
