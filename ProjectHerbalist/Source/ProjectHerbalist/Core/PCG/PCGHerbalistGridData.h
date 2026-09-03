// PCGHerbalistGridData.h
//
// Обратная связь «симуляция -> PCG-граф» (2026-09-03, прямой запрос
// пользователя после разбора вариантов интеграции). До этого связь была
// односторонней: граф мог сыпать ресурсы, но ничего не знал о состоянии
// мира. Этот узел отдаёт графу сетку как облако точек — по точке на клетку,
// с атрибутами состояния — и тем самым позволяет растительности вырастать
// ИЗ симуляции, а не быть покрашенной поверх неё.
//
// Что это даёт на практике: почерневшая от Морока трава не «затекстурена»,
// а не выросла, потому что в клетке высокий Distortion; вытоптанная поляна
// редеет, потому что HarvestStress реально накоплен сборами; вокруг
// ухоженного капища гуще, потому что Restoration высок. Всё это в графе
// делается обычными нодами (Attribute Filter / Density / Match&Set), без
// единой правки C++ на каждое новое правило.
//
// Формат вывода выбран точками с атрибутами, а не текстурой-маской: это
// нативный для PCG способ (attribute-driven spawning), он не требует
// пересборки текстур при изменении сетки и позволяет графу читать сразу
// несколько осей состояния из одного источника.
//
// ВАЖНО про время исполнения: клетки существуют только после
// InitializeCells (BeginPlay). В редакторе, до запуска игры, сетка пуста —
// узел честно вернёт ноль точек и скажет об этом в лог. Осмысленное
// применение — PCG-компонент с генерацией в рантайме (Generate at Runtime),
// тогда граф пересобирается по живому состоянию мира.
#pragma once

#include "CoreMinimal.h"
#include "PCGSettings.h"
#include "PCGHerbalistGridData.generated.h"

/**
 * Отдаёт состояние игровой сетки (AGridWorldManager) в PCG-граф: по точке
 * на клетку, с атрибутами состояния и биома.
 */
UCLASS(BlueprintType, ClassGroup = (Procedural))
class PROJECTHERBALIST_API UPCGHerbalistGridSettings : public UPCGSettings
{
    GENERATED_BODY()

public:
#if WITH_EDITOR
    virtual FName GetDefaultNodeName() const override { return FName(TEXT("GetHerbalistGrid")); }
    virtual FText GetDefaultNodeTitle() const override { return NSLOCTEXT("PCGHerbalistGrid", "NodeTitle", "Get Herbalist Grid"); }
    virtual FText GetNodeTooltipText() const override
    {
        return NSLOCTEXT("PCGHerbalistGrid", "NodeTooltip",
            "Отдаёт состояние игровой сетки травницы точками: по точке на клетку, с атрибутами "
            "Distortion/Corruption/Purity/Stability/HarvestStress/ShrineRestoration/Biome/bIsWater. "
            "Работает в рантайме -- в редакторе до запуска игры клеток ещё не существует.");
    }
    virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Spatial; }
#endif

    /** Пропускать водные клетки (обычно графу растительности они не нужны). */
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
    bool bExcludeWaterCells = false;

    /** Отдавать только клетки, заявленные каким-либо ABiomeRegionVolume. */
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
    bool bOnlyCellsClaimedByBiomeRegions = true;

    /**
     * Отдавать только активные (симулируемые) клетки. Со включённым
     * стримингом это ровно те, что вокруг игрока, — граф не получит
     * данные о дальнем мире, состояние которого всё равно догоняется
     * лениво и на момент запроса неактуально.
     */
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
    bool bOnlyActiveCells = true;

protected:
    virtual TArray<FPCGPinProperties> InputPinProperties() const override { return TArray<FPCGPinProperties>(); }
    virtual TArray<FPCGPinProperties> OutputPinProperties() const override { return Super::DefaultPointOutputPinProperties(); }
    virtual FPCGElementPtr CreateElement() const override;
};

class FPCGHerbalistGridElement : public IPCGElement
{
protected:
    virtual bool ExecuteInternal(FPCGContext* Context) const override;
};
