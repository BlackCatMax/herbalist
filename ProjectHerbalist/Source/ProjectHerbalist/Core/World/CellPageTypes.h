// Core/World/CellPageTypes.h
//
// Страницы клеток (разметка мира, этап 8, DESIGN_World_Layout.md §6): клетки
// хранятся не единым массивом, а страницами -- квадратами PageSizeInCells
// клеток от глобальной клетки (0, 0). Страница -- единица загрузки и выгрузки.

#pragma once

#include "CoreMinimal.h"
#include "Core/Types/HerbalistCoreTypes.h"
#include "Core/Save/HerbalistSaveTypes.h"

class UWaterTypeRegistrySubsystem;

// Сторона блока фолбэка биомов в клетках: клетки вне всех регионов раскрашены
// блоками 5 x 5 (довод -- у BuildCellBase).
inline constexpr int32 HerbalistFallbackBiomeBlockCells = 5;

// Брать ли воду основы клетки из запечённой маски: при старте маска ещё не
// построена, вода раскладывается после основы.
enum class ECellBaseWater : uint8
{
    None,
    FromBakedMask,
};

// Ростер ресурсов нетронутой клетки выгруженной страницы (этап 8в): вид
// засеянного ресурса зависит от условий в момент заселения, и основа его не
// воспроизводит. Полная дельта клетки -- только у тронутых и проявившихся.
struct FHerbalistCellRoster
{
    TArray<FName> IngredientIDs;
    TArray<int32> PlacementSlots;
};

// Всё, что основе клетки нужно помимо координаты и регионов менеджера (этап
// 8в): собирается один раз на страницу, а не на клетку.
struct FHerbalistCellBaseContext
{
    TArray<EBiomeType> AllBiomes;
    const UWaterTypeRegistrySubsystem* WaterSubsystem = nullptr;
    // Ширина строки блоков фолбэка 5 x 5 -- от размера сетки, фиксированного
    // на сессию.
    int32 BlocksX = 0;
};

struct FHerbalistCellPage
{
    // Первая клетка страницы и её размер в клетках. Страница обрезана сеткой:
    // с разметкой сетка кратна странице, без разметки страница одна.
    FIntPoint MinCell = FIntPoint::ZeroValue;
    FIntPoint Size = FIntPoint::ZeroValue;

    bool bLoaded = false;

    // Высоты сняты по загруженному ландшафту для всех клеток. Страница, загруженная
    // раньше земли под ней, досчитывает их при материализации чанка.
    bool bHeightsComplete = false;

    // Построчно от MinCell, индекс -- GetLocalIndex. Высоты и снимки клеток
    // сразу после инициализации (откат при загрузке сейва) -- рядом, тем же
    // индексом.
    TArray<FGridCell> Cells;
    TArray<float> Heights;
    TArray<FSavedCellState> Baselines;

    int32 GetLocalIndex(int32 X, int32 Y) const
    {
        return (Y - MinCell.Y) * Size.X + (X - MinCell.X);
    }
};

// Обход клеток сетки построчно (Y, затем X) для range-for: тот же порядок, что
// у единого массива до страниц. Идёт отрезками строки внутри страницы -- поиск
// страницы один раз на отрезок, а не на клетку, выгруженная страница
// пропускается целиком (ревью этапа 8б). ManagerType -- AGridWorldManager или
// const AGridWorldManager (итератору нужны его страницы, он друг менеджера).
template<typename ManagerType, typename CellType>
class TGridCellRange
{
    using PageType = std::conditional_t<std::is_const_v<ManagerType>, const FHerbalistCellPage, FHerbalistCellPage>;

public:
    class FIterator
    {
    public:
        // bEnd -- итератор конца: текущей клетки нет.
        FIterator(ManagerType* InManager, bool bEnd)
            : Manager(InManager)
        {
            if (bEnd || !Manager)
            {
                return;
            }
            GridMin = Manager->GetGridMinCell();
            GridEnd = GridMin + FIntPoint(FMath::Max(Manager->GridSizeX, 0), FMath::Max(Manager->GridSizeY, 0));
            X = GridMin.X;
            Y = GridMin.Y;
            if (GridEnd.X > GridMin.X && GridEnd.Y > GridMin.Y)
            {
                FindSegment();
            }
        }

        CellType& operator*() const { return *Current; }

        FIterator& operator++()
        {
            ++Current;
            ++X;
            if (X >= SegmentEndX)
            {
                FindSegment();
            }
            return *this;
        }

        bool operator!=(const FIterator& Other) const { return Current != Other.Current; }

    private:
        // Первый отрезок загруженной страницы, начиная с (X, Y).
        void FindSegment()
        {
            while (true)
            {
                if (X >= GridEnd.X)
                {
                    X = GridMin.X;
                    ++Y;
                }
                if (Y >= GridEnd.Y)
                {
                    Current = nullptr;
                    return;
                }
                PageType* Page = Manager->FindCellPage(X, Y);
                if (!Page)
                {
                    X = GridEnd.X;
                    continue;
                }
                const int32 PageEndX = Page->MinCell.X + Page->Size.X;
                if (!Page->bLoaded)
                {
                    X = PageEndX;
                    continue;
                }
                Current = &Page->Cells[Page->GetLocalIndex(X, Y)];
                SegmentEndX = FMath::Min(PageEndX, GridEnd.X);
                return;
            }
        }

        ManagerType* Manager = nullptr;
        FIntPoint GridMin = FIntPoint::ZeroValue;
        FIntPoint GridEnd = FIntPoint::ZeroValue;
        int32 X = 0;
        int32 Y = 0;
        int32 SegmentEndX = 0;
        CellType* Current = nullptr;
    };

    explicit TGridCellRange(ManagerType* InManager)
        : Manager(InManager)
    {
    }

    FIterator begin() const { return FIterator(Manager, false); }
    FIterator end() const { return FIterator(Manager, true); }

private:
    ManagerType* Manager;
};
