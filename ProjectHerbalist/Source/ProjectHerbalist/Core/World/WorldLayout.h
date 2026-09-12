// Core/World/WorldLayout.h
//
// Разметка мира (2026-09-12, DESIGN_World_Layout.md): сетка Herbalist
// увязана с ландшафтом и World Partition. Здесь -- только данные разметки и
// чистый решатель без мира: из исходных величин (квад и компонент
// ландшафта, ячейка и дальность стриминга, границы, ручные значения) он
// выводит клетку, страницу, чанк, диапазон координат, окно карты состояния
// и отпечаток. Сбор исходных величин из редактора и применение к менеджеру
// -- в AGridWorldManager.
//
// Почему решатель отдельно от мира: каждое правило разметки проверяется
// тестом на числах, а не только глазами на одной карте. Ошибка, которую эта
// система закрывает, была именно такой -- размер клетки брался «на глаз» из
// тестового мира (см. CHANGELOG.md, 2026-09-12, поправка масштаба).

#pragma once

#include "CoreMinimal.h"
#include "WorldLayout.generated.h"

// Откуда взято итоговое значение параметра разметки -- уходит в лог и в
// панель деталей, чтобы было видно, что выведено, а что задано руками.
UENUM(BlueprintType)
enum class EWorldLayoutValueOrigin : uint8
{
    // Выведено из ландшафта и World Partition.
    Auto,
    // Задано вручную в переопределениях.
    Manual,
    // Исходных данных для правила нет -- взято запасное значение.
    Fallback
};

// Исходные величины, запечённые из движка в редакторе. В собранной игре
// границ мира нет (GetCompleteBounds и GetRuntimeWorldBounds -- только
// WITH_EDITOR), поэтому они хранятся на акторе менеджера.
USTRUCT(BlueprintType)
struct PROJECTHERBALIST_API FHerbalistWorldLayoutSource
{
    GENERATED_BODY()

    // Есть ли ландшафт вообще. Без него клетка и границы остаются ручными.
    UPROPERTY(VisibleAnywhere, Category = "Landscape")
    bool bHasLandscape = false;

    // Сторона квада ландшафта в сантиметрах (модуль масштаба актора по X).
    UPROPERTY(VisibleAnywhere, Category = "Landscape")
    double QuadSizeCm = 0.0;

    UPROPERTY(VisibleAnywhere, Category = "Landscape")
    int32 ComponentSizeQuads = 0;

    // Мировая XY вершины (0, 0) ландшафта -- по ней проверяется, ложатся ли
    // клетки на вершины.
    UPROPERTY(VisibleAnywhere, Category = "Landscape")
    FVector2D LandscapeOrigin = FVector2D::ZeroVector;

    UPROPERTY(VisibleAnywhere, Category = "Landscape")
    FVector2D LandscapeMin = FVector2D::ZeroVector;

    UPROPERTY(VisibleAnywhere, Category = "Landscape")
    FVector2D LandscapeMax = FVector2D::ZeroVector;

    // Ландшафтов несколько, и у них разные квад или компонент. Берётся первый
    // (решение пользователя), это -- повод для предупреждения.
    UPROPERTY(VisibleAnywhere, Category = "Landscape")
    bool bLandscapesDisagree = false;

    // Есть ли сетка стриминга World Partition, в которой лежит ландшафт.
    UPROPERTY(VisibleAnywhere, Category = "World Partition")
    bool bHasStreamingGrid = false;

    UPROPERTY(VisibleAnywhere, Category = "World Partition")
    FName StreamingGridName;

    UPROPERTY(VisibleAnywhere, Category = "World Partition")
    double StreamingCellSizeCm = 0.0;

    UPROPERTY(VisibleAnywhere, Category = "World Partition")
    double StreamingLoadingRangeCm = 0.0;

    // Начало сетки разбиения: от него отсчитываются координаты клеток
    // (решение пользователя 13).
    UPROPERTY(VisibleAnywhere, Category = "World Partition")
    FVector2D StreamingGridOrigin = FVector2D::ZeroVector;
};

// Ручные значения. Всё, что не отмечено, выводится автоматически. Верхние
// пределы -- не балансовые, а защитные: ручная клетка в сантиметр на карте в
// километры иначе дала бы миллиарды клеток.
USTRUCT(BlueprintType)
struct PROJECTHERBALIST_API FHerbalistWorldLayoutOverrides
{
    GENERATED_BODY()

    // Желаемая клетка. Итоговая -- ближайшее к ней целое число квадов,
    // делящее компонент ландшафта (решения пользователя 1 и 6).
    UPROPERTY(EditAnywhere, Category = "Layout", meta = (ClampMin = "0.01", ClampMax = "1000.0", Units = "m"))
    float DesiredCellSizeMeters = 10.0f;

    UPROPERTY(EditAnywhere, Category = "Layout", meta = (InlineEditConditionToggle))
    bool bOverrideCellSize = false;

    UPROPERTY(EditAnywhere, Category = "Layout", meta = (EditCondition = "bOverrideCellSize", ClampMin = "1.0", ClampMax = "100000.0", Units = "cm"))
    float CellSizeCm = 1000.0f;

    UPROPERTY(EditAnywhere, Category = "Layout", meta = (InlineEditConditionToggle))
    bool bOverridePageSize = false;

    UPROPERTY(EditAnywhere, Category = "Layout", meta = (EditCondition = "bOverridePageSize", ClampMin = "1", ClampMax = "1024"))
    int32 PageSizeInCells = 14;

    UPROPERTY(EditAnywhere, Category = "Layout", meta = (InlineEditConditionToggle))
    bool bOverrideChunkSize = false;

    UPROPERTY(EditAnywhere, Category = "Layout", meta = (EditCondition = "bOverrideChunkSize", ClampMin = "1", ClampMax = "1024"))
    int32 ChunkSizeInCells = 7;
};

// Итоговая разметка.
USTRUCT(BlueprintType)
struct PROJECTHERBALIST_API FHerbalistWorldLayout
{
    GENERATED_BODY()

    // false -- ландшафта нет или разметка отклонена: клетка, размер и начало
    // сетки остаются ручными полями менеджера, как до разметки.
    UPROPERTY(VisibleAnywhere, Category = "Layout")
    bool bValid = false;

    UPROPERTY(VisibleAnywhere, Category = "Layout", meta = (Units = "cm"))
    double CellSizeCm = 0.0;

    UPROPERTY(VisibleAnywhere, Category = "Layout")
    EWorldLayoutValueOrigin CellSizeOrigin = EWorldLayoutValueOrigin::Fallback;

    UPROPERTY(VisibleAnywhere, Category = "Layout")
    int32 PageSizeInCells = 0;

    UPROPERTY(VisibleAnywhere, Category = "Layout")
    EWorldLayoutValueOrigin PageSizeOrigin = EWorldLayoutValueOrigin::Fallback;

    UPROPERTY(VisibleAnywhere, Category = "Layout")
    int32 ChunkSizeInCells = 0;

    UPROPERTY(VisibleAnywhere, Category = "Layout")
    EWorldLayoutValueOrigin ChunkSizeOrigin = EWorldLayoutValueOrigin::Fallback;

    // Мировая XY, от которой отсчитываются координаты клеток.
    UPROPERTY(VisibleAnywhere, Category = "Layout")
    FVector2D Anchor = FVector2D::ZeroVector;

    // Координата первой клетки сетки и число клеток по осям: все клетки,
    // накрывающие ландшафт, расширенные до целых страниц.
    UPROPERTY(VisibleAnywhere, Category = "Layout")
    FIntPoint MinCell = FIntPoint::ZeroValue;

    UPROPERTY(VisibleAnywhere, Category = "Layout")
    FIntPoint GridSize = FIntPoint::ZeroValue;

    // Радиус симуляции после проверок (не больше дальности загрузки), кратный
    // чанку. -1 -- стриминг сетки выключен.
    UPROPERTY(VisibleAnywhere, Category = "Layout", meta = (Units = "m"))
    double EffectiveSimulationRadiusMeters = -1.0;

    // Сторона окна карты состояния мира в клетках (степень двойки).
    UPROPERTY(VisibleAnywhere, Category = "Layout")
    int32 WorldStateWindowCells = 0;

    // Отпечаток: клетка, начало отсчёта, страница. Границы ландшафта и
    // дальность загрузки в него не входят (решение пользователя 14).
    UPROPERTY(VisibleAnywhere, Category = "Layout")
    uint32 Fingerprint = 0;

    double GetPageSizeCm() const { return CellSizeCm * PageSizeInCells; }
    double GetChunkSizeCm() const { return CellSizeCm * ChunkSizeInCells; }
};

// Чистый решатель разметки. Все функции статические и не трогают мир.
struct PROJECTHERBALIST_API FWorldLayoutSolver
{
    // Предел числа клеток. Пока сетка -- один массив в памяти (страницы --
    // этап 8), клетка со всеми параллельными массивами весит порядка 0.44 КБ:
    // 4 млн клеток -- около 1.7 ГБ, дальше разметка отклоняется.
    static constexpr int64 MaxGridCells = 4000000;

    // Итоговая разметка. SimulationRadiusMeters -- действующая настройка
    // ActiveSimulationRadiusMeters (-1 -- стриминг сетки выключен);
    // LongestLocalMechanicMeters -- дальность самого дальнобойного локального
    // механизма (сейчас разрежение сущностей, 30 м). Предупреждения -- по
    // одной строке на нарушенную проверку.
    static FHerbalistWorldLayout Resolve(const FHerbalistWorldLayoutSource& Source,
        const FHerbalistWorldLayoutOverrides& Overrides, float SimulationRadiusMeters,
        float LongestLocalMechanicMeters, TArray<FString>& OutWarnings);

    // Число квадов в клетке: делитель ComponentSizeQuads, дающий клетку,
    // ближайшую к желаемой; при равном расстоянии -- больший.
    static int32 ChooseCellQuads(int32 ComponentSizeQuads, double QuadSizeCm, double DesiredCellCm);

    // Наибольший делитель страницы, при котором действующий радиус
    // floor(R / чанк) * чанк не меньше LongestLocalMechanicMeters. Если не
    // подходит ни один -- 1, вызывающая сторона предупреждает. Радиус < 0
    // (стриминг выключен) -- ограничения нет, чанк равен странице.
    static int32 ChooseChunkCells(int32 PageSizeInCells, double CellSizeCm,
        double SimulationRadiusMeters, double LongestLocalMechanicMeters);

    // Действующий радиус активной области в метрах для данного чанка.
    static double EffectiveRadiusMeters(int32 ChunkSizeInCells, double CellSizeCm, double SimulationRadiusMeters);

    // Радиус в клетках для величины, заданной в метрах (решение пользователя
    // 11): round(метры / клетка), не меньше 1 для положительных метров --
    // радиус, который был ненулевым, не должен исчезать на крупной клетке.
    static int32 MetersToCellRadius(double Meters, double CellSizeCm);

    static uint32 ComputeFingerprint(double CellSizeCm, const FVector2D& Anchor, int32 PageSizeInCells);

    // Совпадают ли исходные величины с точностью до допуска. Границы
    // ландшафта из редактора зависят от того, что загружено, -- точное
    // сравнение double объявляло бы разметку изменившейся на пустом месте.
    static bool IsSameSource(const FHerbalistWorldLayoutSource& A, const FHerbalistWorldLayoutSource& B);

    // Строка для лога: значения и их происхождение.
    static FString Describe(const FHerbalistWorldLayout& Layout);

    static const TCHAR* OriginToString(EWorldLayoutValueOrigin Origin);
};
