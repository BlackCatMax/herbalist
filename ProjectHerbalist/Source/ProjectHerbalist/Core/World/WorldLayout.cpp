// Core/World/WorldLayout.cpp
//
// Решатель разметки мира. Правила и их доводы -- DESIGN_World_Layout.md §4;
// здесь -- только их запись и тонкости чисел с плавающей точкой.

#include "Core/World/WorldLayout.h"

namespace
{
    // Допуск на сравнение длин в сантиметрах. Размеры ландшафта и ячеек --
    // целые сантиметры, а в double они приходят через масштаб актора и
    // деления; сотая сантиметра заведомо меньше любой осмысленной разницы.
    constexpr double LayoutToleranceCm = 0.01;

    // Допуск на округление отношений (страницы, чанки): граница, попавшая
    // ровно на кратное, не должна уйти в соседнюю страницу из-за 1e-12.
    constexpr double RatioEpsilon = 1e-6;

    bool IsWholeMultiple(double Value, double Step)
    {
        if (Step <= 0.0)
        {
            return false;
        }
        const double Ratio = Value / Step;
        return FMath::Abs(Ratio - FMath::RoundToDouble(Ratio)) * Step <= LayoutToleranceCm;
    }
}

const TCHAR* FWorldLayoutSolver::OriginToString(EWorldLayoutValueOrigin Origin)
{
    switch (Origin)
    {
    case EWorldLayoutValueOrigin::Auto:
        return TEXT("авто");
    case EWorldLayoutValueOrigin::Manual:
        return TEXT("вручную");
    default:
        return TEXT("запасное");
    }
}

int32 FWorldLayoutSolver::ChooseCellQuads(int32 ComponentSizeQuads, double QuadSizeCm, double DesiredCellCm)
{
    if (ComponentSizeQuads <= 0 || QuadSizeCm <= 0.0)
    {
        return 1;
    }

    int32 BestQuads = 1;
    double BestDistance = TNumericLimits<double>::Max();
    // Делители идут по возрастанию, поэтому при равном расстоянии достаточно
    // взять текущий -- он больше предыдущего.
    for (int32 Quads = 1; Quads <= ComponentSizeQuads; ++Quads)
    {
        if (ComponentSizeQuads % Quads != 0)
        {
            continue;
        }
        const double Distance = FMath::Abs(Quads * QuadSizeCm - DesiredCellCm);
        if (Distance < BestDistance - LayoutToleranceCm || FMath::IsNearlyEqual(Distance, BestDistance, LayoutToleranceCm))
        {
            BestQuads = Quads;
            BestDistance = Distance;
        }
    }
    return BestQuads;
}

double FWorldLayoutSolver::EffectiveRadiusMeters(int32 ChunkSizeInCells, double CellSizeCm, double SimulationRadiusMeters)
{
    if (SimulationRadiusMeters < 0.0)
    {
        return -1.0;
    }
    const double ChunkMeters = FMath::Max(ChunkSizeInCells, 1) * CellSizeCm / 100.0;
    if (ChunkMeters <= 0.0)
    {
        return 0.0;
    }
    // Та же формула, что у AGridWorldManager::GetActiveRadiusInChunks:
    // граница активной области лежит в целом числе чанков от чанка игрока.
    return FMath::FloorToDouble(SimulationRadiusMeters / ChunkMeters + RatioEpsilon) * ChunkMeters;
}

int32 FWorldLayoutSolver::ChooseChunkCells(int32 PageSizeInCells, double CellSizeCm,
    double SimulationRadiusMeters, double LongestLocalMechanicMeters)
{
    const int32 Page = FMath::Max(PageSizeInCells, 1);
    if (SimulationRadiusMeters < 0.0)
    {
        return Page;
    }
    for (int32 Chunk = Page; Chunk >= 1; --Chunk)
    {
        if (Page % Chunk != 0)
        {
            continue;
        }
        if (EffectiveRadiusMeters(Chunk, CellSizeCm, SimulationRadiusMeters) + RatioEpsilon >= LongestLocalMechanicMeters)
        {
            return Chunk;
        }
    }
    return 1;
}

uint32 FWorldLayoutSolver::ComputeFingerprint(double CellSizeCm, const FVector2D& Anchor, int32 PageSizeInCells)
{
    // Через целые сотые сантиметра, а не биты double: одна и та же разметка,
    // посчитанная разной арифметикой, обязана дать один отпечаток -- иначе
    // сейв объявлялся бы несовместимым на пустом месте.
    uint32 Hash = GetTypeHash(FMath::RoundToInt64(CellSizeCm * 100.0));
    Hash = HashCombine(Hash, GetTypeHash(FMath::RoundToInt64(Anchor.X * 100.0)));
    Hash = HashCombine(Hash, GetTypeHash(FMath::RoundToInt64(Anchor.Y * 100.0)));
    Hash = HashCombine(Hash, GetTypeHash(PageSizeInCells));
    return Hash;
}

bool FWorldLayoutSolver::IsSameSource(const FHerbalistWorldLayoutSource& A, const FHerbalistWorldLayoutSource& B)
{
    // Полсантиметра -- заведомо меньше квада и больше любого шума double в
    // трансформах ландшафта.
    constexpr double ToleranceCm = 0.5;
    return A.bHasLandscape == B.bHasLandscape
        && FMath::IsNearlyEqual(A.QuadSizeCm, B.QuadSizeCm, ToleranceCm)
        && A.ComponentSizeQuads == B.ComponentSizeQuads
        && A.LandscapeOrigin.Equals(B.LandscapeOrigin, ToleranceCm)
        && A.LandscapeMin.Equals(B.LandscapeMin, ToleranceCm)
        && A.LandscapeMax.Equals(B.LandscapeMax, ToleranceCm)
        && A.bLandscapesDisagree == B.bLandscapesDisagree
        && A.bHasStreamingGrid == B.bHasStreamingGrid
        && A.StreamingGridName == B.StreamingGridName
        && FMath::IsNearlyEqual(A.StreamingCellSizeCm, B.StreamingCellSizeCm, ToleranceCm)
        && FMath::IsNearlyEqual(A.StreamingLoadingRangeCm, B.StreamingLoadingRangeCm, ToleranceCm)
        && A.StreamingGridOrigin.Equals(B.StreamingGridOrigin, ToleranceCm);
}

FHerbalistWorldLayout FWorldLayoutSolver::Resolve(const FHerbalistWorldLayoutSource& Source,
    const FHerbalistWorldLayoutOverrides& Overrides, float SimulationRadiusMeters,
    float LongestLocalMechanicMeters, TArray<FString>& OutWarnings)
{
    FHerbalistWorldLayout Layout;

    if (!Source.bHasLandscape || Source.QuadSizeCm <= 0.0 || Source.ComponentSizeQuads <= 0)
    {
        OutWarnings.Add(TEXT("Ландшафта нет -- клетка, размер и начало сетки остаются ручными полями менеджера"));
        return Layout;
    }

    if (Source.bLandscapesDisagree)
    {
        OutWarnings.Add(TEXT("Ландшафтов несколько, и у них разные квад или компонент -- разметка взята по первому"));
    }

    const double ComponentCm = Source.ComponentSizeQuads * Source.QuadSizeCm;

    // ---- Клетка ----
    if (Overrides.bOverrideCellSize)
    {
        Layout.CellSizeCm = FMath::Max(static_cast<double>(Overrides.CellSizeCm), 1.0);
        Layout.CellSizeOrigin = EWorldLayoutValueOrigin::Manual;
        const int32 WholeQuads = FMath::RoundToInt32(Layout.CellSizeCm / Source.QuadSizeCm);
        if (!IsWholeMultiple(Layout.CellSizeCm, Source.QuadSizeCm) || WholeQuads <= 0
            || Source.ComponentSizeQuads % WholeQuads != 0)
        {
            OutWarnings.Add(FString::Printf(
                TEXT("Клетка %.0f см задана вручную и не делит компонент ландшафта (%d квадов по %.0f см) -- клетки не ложатся на вершины"),
                Layout.CellSizeCm, Source.ComponentSizeQuads, Source.QuadSizeCm));
        }
    }
    else
    {
        const double DesiredCellCm = FMath::Max(static_cast<double>(Overrides.DesiredCellSizeMeters), 0.01) * 100.0;
        Layout.CellSizeCm = ChooseCellQuads(Source.ComponentSizeQuads, Source.QuadSizeCm, DesiredCellCm) * Source.QuadSizeCm;
        Layout.CellSizeOrigin = EWorldLayoutValueOrigin::Auto;
    }
    const double CellCm = Layout.CellSizeCm;

    // ---- Страница ----
    const bool bHasStreaming = Source.bHasStreamingGrid && Source.StreamingCellSizeCm > 0.0;
    if (Overrides.bOverridePageSize)
    {
        Layout.PageSizeInCells = FMath::Max(Overrides.PageSizeInCells, 1);
        Layout.PageSizeOrigin = EWorldLayoutValueOrigin::Manual;
        if (bHasStreaming && !FMath::IsNearlyEqual(Layout.PageSizeInCells * CellCm, Source.StreamingCellSizeCm, LayoutToleranceCm))
        {
            OutWarnings.Add(FString::Printf(
                TEXT("Страница %d клеток (%.0f см) задана вручную и не совпадает с ячейкой стриминга %.0f см"),
                Layout.PageSizeInCells, Layout.PageSizeInCells * CellCm, Source.StreamingCellSizeCm));
        }
    }
    else if (bHasStreaming)
    {
        Layout.PageSizeInCells = FMath::Max(FMath::RoundToInt32(Source.StreamingCellSizeCm / CellCm), 1);
        Layout.PageSizeOrigin = EWorldLayoutValueOrigin::Auto;
        if (!IsWholeMultiple(Source.StreamingCellSizeCm, CellCm))
        {
            // Корректности это не ломает: страница грузится по земле под ней,
            // совпадение с ячейкой стриминга -- только оптимизация.
            OutWarnings.Add(FString::Printf(
                TEXT("Ячейка стриминга %.0f см не делится на клетку %.0f см -- страница округлена до %d клеток (%.0f см)"),
                Source.StreamingCellSizeCm, CellCm, Layout.PageSizeInCells, Layout.PageSizeInCells * CellCm));
        }
    }
    else
    {
        Layout.PageSizeInCells = FMath::Max(FMath::RoundToInt32(ComponentCm / CellCm), 1);
        Layout.PageSizeOrigin = EWorldLayoutValueOrigin::Fallback;
        OutWarnings.Add(TEXT("Сетки стриминга World Partition нет -- страница равна компоненту ландшафта"));
    }
    const int32 Page = Layout.PageSizeInCells;
    const double PageCm = Page * CellCm;

    // ---- Радиус симуляции (ручной, только проверяется) ----
    double RadiusMeters = SimulationRadiusMeters;
    if (RadiusMeters >= 0.0 && bHasStreaming && Source.StreamingLoadingRangeCm > 0.0
        && RadiusMeters * 100.0 > Source.StreamingLoadingRangeCm + LayoutToleranceCm)
    {
        OutWarnings.Add(FString::Printf(
            TEXT("Радиус симуляции %.0f м больше дальности загрузки %.0f м -- урезан до неё: дальше страниц нет"),
            RadiusMeters, Source.StreamingLoadingRangeCm / 100.0));
        RadiusMeters = Source.StreamingLoadingRangeCm / 100.0;
    }

    // ---- Чанк ----
    if (Overrides.bOverrideChunkSize)
    {
        Layout.ChunkSizeInCells = FMath::Max(Overrides.ChunkSizeInCells, 1);
        Layout.ChunkSizeOrigin = EWorldLayoutValueOrigin::Manual;
        if (Page % Layout.ChunkSizeInCells != 0)
        {
            OutWarnings.Add(FString::Printf(
                TEXT("Чанк %d клеток задан вручную и не делит страницу %d клеток"),
                Layout.ChunkSizeInCells, Page));
        }
    }
    else
    {
        Layout.ChunkSizeInCells = ChooseChunkCells(Page, CellCm, RadiusMeters, LongestLocalMechanicMeters);
        Layout.ChunkSizeOrigin = EWorldLayoutValueOrigin::Auto;
    }
    Layout.EffectiveSimulationRadiusMeters = EffectiveRadiusMeters(Layout.ChunkSizeInCells, CellCm, RadiusMeters);
    if (RadiusMeters >= 0.0 && Layout.EffectiveSimulationRadiusMeters + RatioEpsilon < LongestLocalMechanicMeters)
    {
        OutWarnings.Add(FString::Printf(
            TEXT("Действующий радиус симуляции %.1f м меньше дальности локальных механик %.1f м -- они дотянутся до спящих клеток"),
            Layout.EffectiveSimulationRadiusMeters, LongestLocalMechanicMeters));
    }

    // ---- Начало отсчёта и выравнивание ландшафта ----
    Layout.Anchor = bHasStreaming ? Source.StreamingGridOrigin : Source.LandscapeOrigin;
    const FVector2D VertexOffset = Source.LandscapeOrigin - Layout.Anchor;
    if (!IsWholeMultiple(VertexOffset.X, CellCm) || !IsWholeMultiple(VertexOffset.Y, CellCm))
    {
        OutWarnings.Add(FString::Printf(
            TEXT("Вершины ландшафта сдвинуты относительно начала отсчёта на нецелое число клеток (%.1f, %.1f см) -- клетки не ложатся на вершины"),
            VertexOffset.X, VertexOffset.Y));
    }

    // ---- Диапазон координат: весь ландшафт целыми страницами ----
    auto PageFloor = [PageCm](double Value, double Origin)
    {
        return static_cast<int32>(FMath::FloorToDouble((Value - Origin) / PageCm + RatioEpsilon));
    };
    auto PageCeil = [PageCm](double Value, double Origin)
    {
        return static_cast<int32>(FMath::CeilToDouble((Value - Origin) / PageCm - RatioEpsilon));
    };
    const FIntPoint MinPage(PageFloor(Source.LandscapeMin.X, Layout.Anchor.X), PageFloor(Source.LandscapeMin.Y, Layout.Anchor.Y));
    const FIntPoint EndPage(
        FMath::Max(PageCeil(Source.LandscapeMax.X, Layout.Anchor.X), MinPage.X + 1),
        FMath::Max(PageCeil(Source.LandscapeMax.Y, Layout.Anchor.Y), MinPage.Y + 1));
    Layout.MinCell = MinPage * Page;
    Layout.GridSize = (EndPage - MinPage) * Page;

    const int64 TotalCells = static_cast<int64>(Layout.GridSize.X) * Layout.GridSize.Y;
    if (Layout.GridSize.X <= 0 || Layout.GridSize.Y <= 0 || TotalCells > MaxGridCells)
    {
        OutWarnings.Add(FString::Printf(
            TEXT("Сетка %dx%d = %lld клеток больше предела %lld -- разметка отклонена; увеличьте клетку"),
            Layout.GridSize.X, Layout.GridSize.Y, TotalCells, MaxGridCells));
        return FHerbalistWorldLayout();
    }

    // ---- Окно карты состояния мира ----
    // Дальность плюс страница с каждой стороны: страница, частично задетая
    // дальностью, загружена целиком.
    const double WindowSpanCm = bHasStreaming && Source.StreamingLoadingRangeCm > 0.0
        ? 2.0 * (Source.StreamingLoadingRangeCm + PageCm)
        : FMath::Max(Layout.GridSize.X, Layout.GridSize.Y) * CellCm;
    const uint32 WindowCells = static_cast<uint32>(FMath::Max(FMath::CeilToDouble(WindowSpanCm / CellCm - RatioEpsilon), 1.0));
    Layout.WorldStateWindowCells = static_cast<int32>(FMath::RoundUpToPowerOfTwo(WindowCells));

    Layout.Fingerprint = ComputeFingerprint(CellCm, Layout.Anchor, Page);
    Layout.bValid = true;
    return Layout;
}

FString FWorldLayoutSolver::Describe(const FHerbalistWorldLayout& Layout)
{
    if (!Layout.bValid)
    {
        return TEXT("разметка не выведена -- клетка, размер и начало сетки ручные");
    }
    const FString Radius = Layout.EffectiveSimulationRadiusMeters < 0.0
        ? FString(TEXT("стриминг сетки выключен"))
        : FString::Printf(TEXT("действующий радиус симуляции %.0f м"), Layout.EffectiveSimulationRadiusMeters);
    return FString::Printf(
        TEXT("клетка %.2f м (%s), страница %d кл. = %.0f м (%s), чанк %d кл. = %.0f м (%s), %s, сетка %dx%d от (%d, %d), начало отсчёта (%.0f, %.0f) см, окно карты %d кл., отпечаток %08X"),
        Layout.CellSizeCm / 100.0, OriginToString(Layout.CellSizeOrigin),
        Layout.PageSizeInCells, Layout.GetPageSizeCm() / 100.0, OriginToString(Layout.PageSizeOrigin),
        Layout.ChunkSizeInCells, Layout.GetChunkSizeCm() / 100.0, OriginToString(Layout.ChunkSizeOrigin),
        *Radius,
        Layout.GridSize.X, Layout.GridSize.Y, Layout.MinCell.X, Layout.MinCell.Y,
        Layout.Anchor.X, Layout.Anchor.Y,
        Layout.WorldStateWindowCells, Layout.Fingerprint);
}
