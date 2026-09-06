// Core/World/GridWorldManagerKurgan.cpp
//
// Курганы (DESIGN_Brewing_Situations_And_Lore.md §4.3 "Гнёздово",
// DESIGN_Community_And_Homestead.md §2.3, 2026-09-06) — единственный
// источник Костяного ножа и Серебряного оберега (артефакт-тир инструментов
// сбора, "не рыночный товар, а находка в кургане или дар хозяина места").
// Дар хозяина места НЕ реализован в этом заходе (решение пользователя:
// "полноценные курганы как POI") — оба предмета идут только этим путём.
//
// Владение (есть ли предмет в инвентаре после находки) НЕ проверяется
// здесь — тот же принцип разделения обязанностей, что уже у оберегов
// (GridWorldManagerWards.cpp): AKurganActor::OnInteract (Core/World/
// KurganActor.h, DECISIONS_LOG.md решение №5, 2026-09-06 — физический
// подбор вместо тихого Exec-гранта) резолвит State через
// IngredientRegistrySubsystem и кладёт предмет в инвентарь,
// GridWorldManager хранит только то, какие курганы ещё не разграблены.

#include "Core/World/GridWorldManager.h"
#include "Core/World/KurganActor.h"
#include "ProjectHerbalist.h"
#include "HerbalistLogChannels.h"

void AGridWorldManager::SeedKurganSites()
{
    KurganSites.Empty();

    // Ровно два предмета этого захода несут артефакт-тир (§2.3) — курганы
    // не система лута с произвольным числом сайтов, а точечный источник
    // именно этих двух карточек.
    static const TArray<FName> Loot = { FName(TEXT("Костяной нож")), FName(TEXT("Серебряный оберег")) };

    const int32 TotalCells = Cells.Num();
    if (TotalCells == 0) return;

    // Детерминированный старт-сдвиг тем же WorldRNG, что уже заливка воды/
    // ресурсов выше в InitializeCells — не отдельный, несинхронизированный
    // источник случайности. Перебор от сдвига линейный и гарантированно
    // завершается за один проход по сетке (тот же принцип, что уже
    // SeedTestLandmarks: первая подходящая клетка в порядке обхода, не
    // retry-цикл со случайными координатами, который мог бы не найти
    // валидную клетку на маленькой тестовой сетке).
    const int32 StartIndex = WorldRNG.RandRange(0, TotalCells - 1);
    int32 LootIndex = 0;

    for (int32 Offset = 0; Offset < TotalCells && LootIndex < Loot.Num(); ++Offset)
    {
        const FGridCell& Cell = Cells[(StartIndex + Offset) % TotalCells];
        if (Cell.bIsWater) continue;

        const FIntPoint Coord(Cell.X, Cell.Y);
        KurganSites.Add(Coord, Loot[LootIndex]);
        UE_LOG(LogHerbalistWorld, Log, TEXT("[Kurgan] Seeded '%s' at (%d,%d)"),
            *Loot[LootIndex].ToString(), Coord.X, Coord.Y);

        // Физический подбор (DECISIONS_LOG.md решение №5, 2026-09-06) --
        // AKurganActor спавнится сразу здесь, тем же приёмом, что
        // Тотем/Светлояр/Горюч-камень в SeedPointsOfInterest.
        if (UWorld* World = GetWorld())
        {
            if (AKurganActor* KurganPickup = World->SpawnActor<AKurganActor>(AKurganActor::StaticClass(),
                GetCellWorldPosition(Coord.X, Coord.Y), FRotator::ZeroRotator))
            {
                KurganPickup->Init(this, Coord.X, Coord.Y, Loot[LootIndex]);
            }
        }

        ++LootIndex;
    }
}

bool AGridWorldManager::LootKurgan(const FIntPoint& Cell, FName& OutGrantedIngredientID)
{
    const FName* Found = KurganSites.Find(Cell);
    if (!Found) return false;

    OutGrantedIngredientID = *Found;
    KurganSites.Remove(Cell);
    UE_LOG(LogHerbalistWorld, Log, TEXT("[Kurgan] Looted at (%d,%d): granted '%s'"),
        Cell.X, Cell.Y, *OutGrantedIngredientID.ToString());
    return true;
}
