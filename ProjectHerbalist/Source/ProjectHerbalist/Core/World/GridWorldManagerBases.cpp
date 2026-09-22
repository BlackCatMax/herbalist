// Core/World/GridWorldManagerBases.cpp
//
// Базы/лагеря (02_GDD/21_Journey_And_Artifacts.md §21.2, 2026-09-01) — герой
// не таскает Заряну по карте, а обживает несколько точек в разных регионах.
// v1: тот же приём, что уже SetGardenPlot/RegisterShrine — механизм
// (регистрация клетки + место варки) полностью работает, физическая
// постройка-стол/декор на каждой базе — контент/редактор, отдельная задача.
//
// Лорные «крючки» баз по биомам (§21.2: карточка компендиума, объясняющая,
// почему в этом биоме есть жильё, куда можно вернуться, -- для
// Широколиственного леса нашлись Злыдни, «заброшенные постройки») сняты
// решением пользователя 2026-09-19: механики за ними нет, вернуться к ним
// -- позже, вместе с обставлением баз.

#include "Core/World/GridWorldManager.h"
#include "Core/World/HomesteadMarkerActor.h"
#include "Core/Storage/StorageContainer.h"
#include "Core/Inventory/HerbalistInventoryComponent.h"
#include "ProjectHerbalist.h"
#include "HerbalistLogChannels.h"
#include "Engine/World.h"
#include "EngineUtils.h"

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

    // Маркер-актор "Обставления" (ROADMAP.md, 2026-09-06) -- база не может
    // быть зарегистрирована дважды (проверка выше), поэтому здесь только
    // спавн, без find-or-update ветки, что уже есть у RegisterGardenPlot.
    if (UWorld* World = GetWorld())
    {
        if (AHomesteadMarkerActor* Marker = World->SpawnActor<AHomesteadMarkerActor>(
            AHomesteadMarkerActor::StaticClass(), GetCellWorldPosition(Cell.X, Cell.Y), FRotator::ZeroRotator))
        {
            Marker->Init(Cell, EHomesteadMarkerKind::Base);
        }
    }
}

void AGridWorldManager::SyncHomesteadMarkers()
{
    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }

    TSet<FIntPoint> BaseCells;
    for (const FHerbalistBase& Base : Bases)
    {
        BaseCells.Add(Base.Cell);
    }

    // Что уже стоит -- сверить с состоянием; дубликаты на одной клетке тоже
    // убрать.
    TSet<FIntPoint> MarkedPlots;
    TSet<FIntPoint> MarkedBases;
    for (TActorIterator<AHomesteadMarkerActor> It(World); It; ++It)
    {
        AHomesteadMarkerActor* Marker = *It;
        const FIntPoint Cell = Marker->GetGridCell();
        if (Marker->GetKind() == EHomesteadMarkerKind::Base)
        {
            if (!BaseCells.Contains(Cell) || MarkedBases.Contains(Cell))
            {
                Marker->Destroy();
                continue;
            }
            MarkedBases.Add(Cell);
        }
        else
        {
            const EGardenNiche* Niche = GardenPlots.Find(Cell);
            if (!Niche || *Niche == EGardenNiche::None || MarkedPlots.Contains(Cell))
            {
                Marker->Destroy();
                continue;
            }
            Marker->SetNiche(*Niche);
            MarkedPlots.Add(Cell);
        }
    }

    for (const TPair<FIntPoint, EGardenNiche>& Plot : GardenPlots)
    {
        if (Plot.Value == EGardenNiche::None || MarkedPlots.Contains(Plot.Key))
        {
            continue;
        }
        // Сейв после смены разметки мира может ссылаться на клетку вне сетки
        // -- маркер в пустоте не ставить.
        if (!IsCellInGrid(Plot.Key.X, Plot.Key.Y))
        {
            UE_LOG(LogHerbalistWorld, Warning, TEXT("[Garden] Plot at (%d,%d) is outside the grid -- no marker"), Plot.Key.X, Plot.Key.Y);
            continue;
        }
        if (AHomesteadMarkerActor* Marker = World->SpawnActor<AHomesteadMarkerActor>(
            AHomesteadMarkerActor::StaticClass(), GetCellWorldPosition(Plot.Key.X, Plot.Key.Y), FRotator::ZeroRotator))
        {
            Marker->Init(Plot.Key, EHomesteadMarkerKind::GardenPristroyka, Plot.Value);
        }
    }
    for (const FIntPoint& Cell : BaseCells)
    {
        if (MarkedBases.Contains(Cell))
        {
            continue;
        }
        if (!IsCellInGrid(Cell.X, Cell.Y))
        {
            UE_LOG(LogHerbalistWorld, Warning, TEXT("[Base] (%d,%d) is outside the grid -- no marker"), Cell.X, Cell.Y);
            continue;
        }
        if (AHomesteadMarkerActor* Marker = World->SpawnActor<AHomesteadMarkerActor>(
            AHomesteadMarkerActor::StaticClass(), GetCellWorldPosition(Cell.X, Cell.Y), FRotator::ZeroRotator))
        {
            Marker->Init(Cell, EHomesteadMarkerKind::Base);
        }
    }
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
    // Погреб, шкаф и кувшин -- не в одной точке (ревью 2026-09-14): у Blueprint
    // есть меш, и совпавшие меши делали выбор хранилища трассой IA_Interact
    // случайным. Разнос вдоль Y на четверть клетки -- все три в пределах клетки.
    const float TypeOffset = ContainerType == EStorageContainerType::Cabinet ? 1.0f
        : (ContainerType == EStorageContainerType::Jar ? -1.0f : 0.0f);
    SpawnPos.Y += CellSize * 0.25f * TypeOffset;
    SpawnPos.Z = GetCellHeight(AnchorCell.X, AnchorCell.Y) + 5.0f;

    // Класс с окном переноса (2026-09-14): TransferWidgetClass задаётся только
    // в Blueprint, и голый AStorageContainer не открывался -- "Missing components".
    UClass* ContainerClass = HomeStorageContainerClass.LoadSynchronous();
    if (!ContainerClass)
    {
        UE_LOG(LogHerbalistWorld, Warning, TEXT("[HomeStorage] HomeStorageContainerClass не загрузился -- голый AStorageContainer, окно не откроется"));
        ContainerClass = AStorageContainer::StaticClass();
    }

    // Отложенный спавн: флаг должен стоять до BeginPlay, иначе хранилище
    // забирало бы содержимое выгруженного контейнера карты с тем же именем.
    UWorld* World = GetWorld();
    const FTransform SpawnTransform(FRotator::ZeroRotator, SpawnPos);
    AStorageContainer* NewContainer = World ? World->SpawnActorDeferred<AStorageContainer>(ContainerClass, SpawnTransform) : nullptr;
    if (!NewContainer)
    {
        UE_LOG(LogHerbalistWorld, Warning, TEXT("[HomeStorage] SpawnActor failed near (%d,%d)"), AnchorCell.X, AnchorCell.Y);
        return nullptr;
    }
    NewContainer->bIsHomeStorage = true;
    NewContainer->FinishSpawning(SpawnTransform);

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
