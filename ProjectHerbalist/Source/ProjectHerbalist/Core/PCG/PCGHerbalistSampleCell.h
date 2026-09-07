// PCGHerbalistSampleCell.h
//
// «Sample Herbalist Cell» — узел, который берёт ЧУЖИЕ точки (траву, кусты,
// что угодно, порождённое графом) и дописывает каждой состояние клетки, над
// которой она стоит (2026-09-08, продолжение разбора CalystoWorld).
//
// ЗАЧЕМ ОТДЕЛЬНЫЙ УЗЕЛ, ЕСЛИ ЕСТЬ «Get Herbalist Grid». У того узла нет
// входных пинов вовсе (`InputPinProperties()` возвращает пустой массив) —
// это источник, он отдаёт СВОЁ облако по точке на клетку и пометить чужие
// точки не может технически. Отсюда и нужда во втором узле: один отдаёт
// сетку как данные, другой переносит её на уже существующую геометрию.
//
// ЧЕЙ ЭТО ОБРАЗЕЦ. Ровно тот же приём, что в Calysto: там HLSL-генератор
// травы получает вторым входом облако «маяков биомов» и для каждой травинки
// ищет ближайший маяк, после чего пишет ей строковый ключ
// (`Out_SetStringKey('Owner', ...)`), а `PCGMeshSelectorByAttribute` по
// этому ключу решает, какой меш вообще спавнить. У нас та же схема, с двумя
// отличиями в нашу пользу:
//   * сетка регулярная, поэтому вместо перебора всех маяков (у Calysto это
//     цикл по BeaconCount на каждую точку) достаточно прямого пересчёта
//     координаты в индекс клетки — O(1) вместо O(N);
//   * состояние берётся живое, из симуляции, а не расставленное автором.
//
// ПОЧЕМУ КЛЮЧ ВЫБИРАЕТСЯ ПО `bDegrading`, А НЕ ПО ПОРОГУ. Соблазн написать
// «Corruption > 0.8 — значит колючки» ведёт к дребезгу: клетка, висящая у
// порога, переключала бы флору туда-сюда на каждой перегенерации. В проекте
// уже есть ровно то, что нужно, — липкий флаг бистабильности
// `Cell.Memory.bDegrading` с гистерезисом (вход при Corruption > 0.85,
// выход при < 0.65). Он по построению не дребезжит, и «уйти из испорченного
// полюса можно только действием игрока» (02_GDD/12_Biome_Change.md §12.10)
// — то есть смена флоры обратно становится наградой за работу, а не
// случайным шумом. Изобретать второй порог поверх существующего было бы
// дублированием одного решения в двух местах.
//
// ЦЕНА, КОТОРУЮ НАДО ЗНАТЬ. Узел считается на CPU. В графе `PCG_Grass` все
// четыре спавнера стоят с `bExecuteOnGPU`, и вставка CPU-узла в такую
// цепочку заставит PCG гонять данные между CPU и GPU. Для ДИСКРЕТНОЙ смены
// флоры это приемлемо: узел работает только при перегенерации, а она редка
// (флаг липкий). Для НЕПРЕРЫВНОГО отклика — пожелтения травы по мере порчи
// — этот путь не годится вовсе, и он для него и не предназначен: непрерывное
// делает карта состояния мира (GridWorldManagerWorldStateMap.cpp), которая
// обновляется без всякой перегенерации.
//
// ВРЕМЯ ИСПОЛНЕНИЯ. То же ограничение, что у «Get Herbalist Grid»: клетки
// существуют только после InitializeCells (BeginPlay). В редакторе до
// запуска игры узел честно пропустит точки нетронутыми и скажет об этом в
// лог, а не притворится, что мир здоров.
#pragma once

#include "CoreMinimal.h"
#include "PCGSettings.h"
#include "PCGHerbalistSampleCell.generated.h"

/**
 * Дописывает точкам состояние клетки сетки, над которой они стоят, и ключ
 * выбора меша для PCGMeshSelectorByAttribute.
 */
UCLASS(BlueprintType, ClassGroup = (Procedural))
class PROJECTHERBALIST_API UPCGHerbalistSampleCellSettings : public UPCGSettings
{
    GENERATED_BODY()

public:
#if WITH_EDITOR
    virtual FName GetDefaultNodeName() const override { return FName(TEXT("SampleHerbalistCell")); }
    virtual FText GetDefaultNodeTitle() const override { return NSLOCTEXT("PCGHerbalistSampleCell", "NodeTitle", "Sample Herbalist Cell"); }
    virtual FText GetNodeTooltipText() const override
    {
        return NSLOCTEXT("PCGHerbalistSampleCell", "NodeTooltip",
            "Дописывает каждой входящей точке состояние клетки под ней "
            "(Distortion/Corruption/HarvestStress/Biome/bDegrading) и строковый MeshKey "
            "для PCGMeshSelectorByAttribute. Работает в рантайме -- в редакторе до запуска "
            "игры клеток ещё не существует.");
    }
    virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Spatial; }
#endif

    /**
     * Ключ для клеток в порядке. Тот же текст надо вписать в запись
     * PCGMeshSelectorByAttribute у спавнера -- он сопоставляет по строке.
     */
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
    FString HealthyMeshKey = TEXT("Healthy");

    /** Ключ для клеток в испорченном полюсе (Cell.Memory.bDegrading). */
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
    FString DegradingMeshKey = TEXT("Degrading");

    /**
     * Выбрасывать точки, оказавшиеся вне сетки. По умолчанию выключено:
     * тихо удалять растительность за краем игровой сетки -- решение с
     * далеко идущими последствиями для вида мира, и принимать его молча,
     * настройкой по умолчанию, неправильно. Такие точки получают ключ
     * HealthyMeshKey и нулевые оси.
     */
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
    bool bDropPointsOutsideGrid = false;

protected:
    virtual TArray<FPCGPinProperties> InputPinProperties() const override;
    virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
    virtual FPCGElementPtr CreateElement() const override;
};

class FPCGHerbalistSampleCellElement : public IPCGElement
{
protected:
    virtual bool ExecuteInternal(FPCGContext* Context) const override;
};
