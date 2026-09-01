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
#include "ProjectHerbalist.h"
#include "HerbalistLogChannels.h"

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
        // Дубинка (Дубыня, 21_Journey_And_Artifacts.md §21.3-21.4) — "одним
        // ударом расчищает место под стоянку в любом биоме". Единственный
        // реально прошитый эффект артефактов этого прохода — прямая
        // проверка здесь, RegisterBase остаётся единственной точкой входа
        // для основания базы (FoundBase на контроллере просто её вызывает,
        // не дублирует логику).
        const bool bHasClub = AcquiredArtifacts.ContainsByPredicate(
            [](const FAcquiredArtifact& A) { return A.ArtifactID == FName(TEXT("Дубинка")); });
        if (!bHasClub)
        {
            UE_LOG(LogHerbalistWorld, Warning, TEXT("[Base] (%d,%d) is water — not registered"), Cell.X, Cell.Y);
            return;
        }
        UE_LOG(LogHerbalistWorld, Log, TEXT("[Base] (%d,%d) is water, but Дубинка clears it for a camp"), Cell.X, Cell.Y);
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
