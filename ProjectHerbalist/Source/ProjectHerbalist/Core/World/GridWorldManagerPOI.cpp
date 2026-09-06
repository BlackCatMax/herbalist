// Core/World/GridWorldManagerPOI.cpp
//
// Точки интереса (DESIGN_Brewing_Situations_And_Lore.md §4, 2026-09-06,
// прямой запрос пользователя "проработаем POI — кроме курганов должны
// быть ещё точки, фольклорные места, постройки"). См. POITypes.h за общим
// доводом каркаса и GridWorldManager.h за доводом у полей/геттеров каждого
// вида точки.

#include "Core/World/GridWorldManager.h"
#include "Core/Config/HerbalistSettings.h"
#include "ProjectHerbalist.h"
#include "HerbalistLogChannels.h"

namespace
{
    // Один и тот же детерминированный линейный проход, что уже
    // SeedKurganSites -- первая небезводная клетка, начиная со случайного
    // (тем же WorldRNG) сдвига, ещё не занятая другим POI. Не retry-цикл со
    // случайными координатами (гарантированно завершается за один проход
    // по сетке даже на маленькой тестовой сетке).
    FIntPoint SeedSinglePOISite(TArray<FGridCell>& Cells, FRandomStream& WorldRNG, const TSet<FIntPoint>& Occupied)
    {
        const int32 TotalCells = Cells.Num();
        if (TotalCells == 0) return FIntPoint(-1, -1);

        const int32 StartIndex = WorldRNG.RandRange(0, TotalCells - 1);
        for (int32 Offset = 0; Offset < TotalCells; ++Offset)
        {
            const FGridCell& Cell = Cells[(StartIndex + Offset) % TotalCells];
            if (Cell.bIsWater) continue;

            const FIntPoint Coord(Cell.X, Cell.Y);
            if (Occupied.Contains(Coord)) continue;
            return Coord;
        }
        return FIntPoint(-1, -1);
    }
}

void AGridWorldManager::SeedPointsOfInterest()
{
    // Курганы сеются первыми и своим собственным, уже протестированным
    // кодом -- их поведение (ровно 2 клетки, свой проход) не меняется,
    // только вызов переносится под эту общую точку входа.
    SeedKurganSites();

    TSet<FIntPoint> Occupied;
    Occupied.Reserve(KurganSites.Num() + 4);
    for (const auto& Pair : KurganSites)
    {
        Occupied.Add(Pair.Key);
    }

    TotemSite = SeedSinglePOISite(Cells, WorldRNG, Occupied);
    if (TotemSite != FIntPoint(-1, -1)) Occupied.Add(TotemSite);

    SvetloyarSite = SeedSinglePOISite(Cells, WorldRNG, Occupied);
    if (SvetloyarSite != FIntPoint(-1, -1)) Occupied.Add(SvetloyarSite);

    GoryuchKamenSite = SeedSinglePOISite(Cells, WorldRNG, Occupied);
    if (GoryuchKamenSite != FIntPoint(-1, -1)) Occupied.Add(GoryuchKamenSite);

    SoloveySite = SeedSinglePOISite(Cells, WorldRNG, Occupied);
    if (SoloveySite != FIntPoint(-1, -1)) Occupied.Add(SoloveySite);

    UE_LOG(LogHerbalistWorld, Log, TEXT("[POI] Seeded: Totem(%d,%d) Svetloyar(%d,%d) GoryuchKamen(%d,%d) Solovey(%d,%d)"),
        TotemSite.X, TotemSite.Y, SvetloyarSite.X, SvetloyarSite.Y,
        GoryuchKamenSite.X, GoryuchKamenSite.Y, SoloveySite.X, SoloveySite.Y);
}

FString AGridWorldManager::GetTotemRevealText() const
{
    if (TotemSite == FIntPoint(-1, -1)) return TEXT("Тотем не найден на этой карте.");

    const FGridCell* Cell = GetCellConst(TotemSite.X, TotemSite.Y);
    if (!Cell) return TEXT("Тотем не найден на этой карте.");

    // Нижний ярус (подземное божество / Навь, §4.2) — всегда читается,
    // прямое попадание в Distortion места. Средний ярус ("состояние
    // игрока") НЕ реализован -- см. довод у GetTotemSite/SetTotemSite в
    // GridWorldManager.h, честный пробел, не молчаливое упрощение.
    const float Distortion = Cell->State.Meta.Distortion;
    FString Result = FString::Printf(TEXT("Нижний ярус (подземное божество, Навь): Distortion места %.2f -- %s."),
        Distortion, Distortion >= 0.5f ? TEXT("лик искажён, черты плывут") : TEXT("лик читается ясно"));

    const UHerbalistSettings* Settings = GetHerbalistSettings();
    const float PurityThreshold = Settings ? Settings->TotemUpperTierPurityThreshold : 0.6f;
    if (Cell->State.Meta.Purity >= PurityThreshold)
    {
        Result += TEXT(" Верхний ярус (небо и боги) виден -- высокая Purity места открывает его.");
    }

    return Result;
}

bool AGridWorldManager::IsSvetloyarVisible() const
{
    if (SvetloyarSite == FIntPoint(-1, -1)) return false;

    const UHerbalistSettings* Settings = GetHerbalistSettings();
    const float Threshold = Settings ? Settings->SvetloyarVisibilityClarityThreshold : 0.7f;
    return GlobalPerceptionClarity >= Threshold;
}

bool AGridWorldManager::ActivateSolovey()
{
    if (SoloveySite == FIntPoint(-1, -1)) return false;
    if (bSoloveyTriggered) return false;

    // Оберег ДО контакта (одолень-трава, §4.4) -- та же
    // IsWardConcealmentActive(Cell), что уже даёт "скрытие" от бестиария;
    // здесь просто другая угроза читает тот же флаг/радиус. Проход под
    // прикрытием засчитывается без порчи -- точка отмечается пройденной,
    // повторно не сработает.
    if (IsWardConcealmentActive(SoloveySite))
    {
        bSoloveyTriggered = true;
        UE_LOG(LogHerbalistWorld, Log, TEXT("[Solovey] Passed at (%d,%d) under одолень-трава concealment -- no corruption"),
            SoloveySite.X, SoloveySite.Y);
        return true;
    }

    const UHerbalistSettings* Settings = GetHerbalistSettings();
    const int32 Radius = Settings ? Settings->SoloveyCorruptionRadius : 3;
    const float Burst = Settings ? Settings->SoloveyCorruptionBurst : 0.3f;

    for (FGridCell& Cell : Cells)
    {
        const int32 Dist = FMath::Max(FMath::Abs(Cell.X - SoloveySite.X), FMath::Abs(Cell.Y - SoloveySite.Y));
        if (Dist > Radius) continue;

        Cell.State.Meta.Purity = FMath::Clamp(Cell.State.Meta.Purity - Burst, 0.0f, 1.0f);
        Cell.State.Meta.Stability = FMath::Clamp(Cell.State.Meta.Stability - Burst, 0.0f, 1.0f);
    }

    bSoloveyTriggered = true;
    UE_LOG(LogHerbalistWorld, Log, TEXT("[Solovey] Triggered at (%d,%d): AoE corruption radius %d, burst %.2f"),
        SoloveySite.X, SoloveySite.Y, Radius, Burst);
    return true;
}
