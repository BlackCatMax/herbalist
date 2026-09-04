// Core/World/GridWorldManagerBases.cpp
//
// Базы/лагеря (02_GDD/21_Journey_And_Artifacts.md §21.2, 2026-09-01) — герой
// не таскает Заряну по карте, а обживает несколько точек в разных регионах.
// v1: тот же приём, что уже SetGardenPlot/RegisterShrine — механизм
// (регистрация клетки + место варки) полностью работает, физическая
// постройка-стол/декор на каждой базе — контент/редактор, отдельная задача.
//
// Крючок в компендиуме за образ "место, куда возвращаются обживаться", на
// биом (найден в разведке к этому шагу — исходный промпт называл Гуменник/
// Овинник/Жердяи для Широколиственного леса, но эти три карточки на деле
// про АКТИВНОЕ, ухоженное хозяйство, не заброшенное, см. CHANGELOG.md):
//
// - Широколиственный лес: Злыдни ("заброшенные постройки", Низший ярус,
//   AmbientEntityTypes.h, триггер HarvestStress > 0.6) — единственный уже
//   подобранный крючок, реально про заброшенное жильё.
// - Тундра, Тайга, Смешанный лес, Лесостепь, Степь, Речная пойма, Болото —
//   TODO: биомный крючок не подобран. Не изобретаю лор здесь — тот же
//   принцип честности, что уже у "📐 Запланировано, не реализовано" карточек
//   компендиума.

#include "Core/World/GridWorldManager.h"
#include "Core/Storage/StorageContainer.h"
#include "Core/Inventory/HerbalistInventoryComponent.h"
#include "ProjectHerbalist.h"
#include "HerbalistLogChannels.h"
#include "Engine/World.h"

void AGridWorldManager::RegisterBase(const FIntPoint& Cell)
{
    const FGridCell* Center = GetCellConst(Cell.X, Cell.Y);
    if (!Center)
    {
        UE_LOG(LogHerbalistWorld, Warning, TEXT("[Base] (%d,%d) is outside the grid — not registered"), Cell.X, Cell.Y);
        return;
    }
    if (Center->bIsWater)
    {
        // Дубинка/Дубыня изъяты из дизайна (21_Journey_And_Artifacts.md
        // §21.3/§21.5, 2026-09-01, коммит "Ending and artifacts") — Смешанный
        // лес теперь Баба-Яга/Шапка-невидимка, у которой нет эффекта,
        // связанного с базами. Вода снова всегда запрещена для базы, тем же
        // способом, что и до Дубинки — не регрессия, отмена конкретной фичи.
        UE_LOG(LogHerbalistWorld, Warning, TEXT("[Base] (%d,%d) is water — not registered"), Cell.X, Cell.Y);
        return;
    }
    for (const FHerbalistBase& Existing : Bases)
    {
        if (Existing.Cell == Cell)
        {
            UE_LOG(LogHerbalistWorld, Log, TEXT("[Base] (%d,%d) already registered"), Cell.X, Cell.Y);
            return;
        }
    }

    FHerbalistBase NewBase;
    NewBase.Cell = Cell;
    NewBase.Biome = Center->Biome;
    Bases.Add(NewBase);
    UE_LOG(LogHerbalistWorld, Log, TEXT("[Base] Founded at (%d,%d), biome=%d"), Cell.X, Cell.Y, (int32)Center->Biome);
}

bool AGridWorldManager::IsValidBrewingLocation(const FIntPoint& Cell) const
{
    for (const FShrine& S : Shrines)
    {
        if (S.Cell == Cell) return true;
    }
    for (const FHerbalistBase& B : Bases)
    {
        if (B.Cell == Cell) return true;
    }
    return false;
}

// Домашние хранилища (DESIGN_Community_And_Homestead.md §2.2, 2026-09-04) --
// см. подробный довод у объявления (GridWorldManager.h): только сам эффект
// (спавн AStorageContainer у клетки-якоря дома), владение/Respect/материал
// уже проверены вызывающей стороной (AHerbalistPlayerController::
// BuildHomeStorage) -- тот же принцип границы, что уже PlantSeedInCell/
// ApplyFertilizerToCell в GridWorldManagerCore.cpp.
AStorageContainer* AGridWorldManager::SpawnHomeStorageContainer(const FIntPoint& AnchorCell, EStorageContainerType ContainerType)
{
    const FGridCell* Cell = GetCellConst(AnchorCell.X, AnchorCell.Y);
    if (!Cell)
    {
        UE_LOG(LogHerbalistWorld, Warning, TEXT("[HomeStorage] (%d,%d) is outside the grid -- not built"), AnchorCell.X, AnchorCell.Y);
        return nullptr;
    }

    // "Рядом с клеткой" котла, не поверх неё (прямая формулировка запроса) --
    // сдвиг на полклетки в сторону +X. Не JitterRadius/FindFreeSpawnPositionInCell
    // (тот путь трейсит на ландшафт и проверяет занятость коллизией, см.
    // SpawnResourceActor) -- намеренно проще: постройка мгновенна и
    // детерминирована по решению игрока, не вероятностная россыпь ресурсов,
    // а тот же класс упрощения делает эту функцию тестируемой без
    // настоящего ландшафта под ногами (см. StorageContainerTest.cpp — тот
    // же голый SpawnActor, без трассировки).
    FVector SpawnPos = GetCellWorldPositionFlat(AnchorCell.X, AnchorCell.Y);
    SpawnPos.X += CellSize * 0.5f;
    SpawnPos.Z = GetCellHeight(AnchorCell.X, AnchorCell.Y) + 5.0f;

    UWorld* World = GetWorld();
    AStorageContainer* NewContainer = World ? World->SpawnActor<AStorageContainer>(AStorageContainer::StaticClass(), SpawnPos, FRotator::ZeroRotator) : nullptr;
    if (!NewContainer)
    {
        UE_LOG(LogHerbalistWorld, Warning, TEXT("[HomeStorage] SpawnActor failed near (%d,%d)"), AnchorCell.X, AnchorCell.Y);
        return nullptr;
    }

    // AStorageContainer's constructor defaults ContainerType to Basket
    // (see StorageContainer.cpp) -- overridden here to what was actually built.
    if (NewContainer->InventoryComponent)
    {
        NewContainer->InventoryComponent->ContainerType = ContainerType;
    }

    UE_LOG(LogHerbalistWorld, Log, TEXT("[HomeStorage] Built container type=%d near (%d,%d)"),
        (int32)ContainerType, AnchorCell.X, AnchorCell.Y);
    return NewContainer;
}
