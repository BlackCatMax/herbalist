// PcgGrassRuntimeBuilder.h
//
// Трава в рантайме (2026-09-19, решение пользователя «переключай в рантайм»,
// вариант А этапа 5 docs/research/DESIGN_Living_Vegetation_Research.md §4):
// PCG-компоненты с графом PCG_Grass -- у акторов BP_BiomeVolume -- переходят с
// GenerateOnLoad (граф запекался в редакторе, где сетки симуляции нет, и
// сезон узла Sample Herbalist Cell не работал) на GenerateAtRuntime с
// разбиением на ячейки: трава строится вокруг игрока по мере подхода.
//
//   1. Шаблон компонента в BP_BiomeVolume: GenerateAtRuntime, разбиение.
//   2. PCG_Grass: иерархическая генерация, сетка 64 м (радиус генерации по
//      умолчанию -- 128 м): объём ~100 м строится ячейками по мере подхода, а
//      не целиком за 512 м от игрока (сетка карты по умолчанию -- 256 м).
//   3. PCG World Actor карты: кэш ландшафта SerializeOnlyAtCook. С
//      NeverSerialize (было) в PIE и в сборке кэша нет, Get Landscape Data не
//      отдаёт данных, проекция травы на ландшафт выбрасывала бы все точки.
//      В PIE кэш строится по запросу, в сборке -- запекается при cook.
//   4. Экземпляры на карте (внешние акторы World Partition): замер
//      запечённой травы (экземпляров ISM на м²), очистка запечённого --
//      иначе в игре стояла бы и старая трава, и новая, -- тот же режим.
//
// -ReportOnly: только замер, без правок.
//
// Запуск: UnrealEditor-Cmd.exe <uproject> -run=WorldPartitionBuilderCommandlet
//         /Game/Maps/L_TestDev -Builder=PcgGrassRuntimeBuilder [-ReportOnly]
#pragma once

#include "CoreMinimal.h"
#include "WorldPartition/WorldPartitionBuilder.h"
#include "PcgGrassRuntimeBuilder.generated.h"

class UPCGComponent;

UCLASS()
class UPcgGrassRuntimeBuilder : public UWorldPartitionBuilder
{
    GENERATED_UCLASS_BODY()

public:
    virtual bool RequiresCommandletRendering() const override { return false; }
    virtual ELoadingMode GetLoadingMode() const override { return ELoadingMode::Custom; }

    // Режим травы в рантайме на компоненте или шаблоне компонента. true --
    // что-то поменялось. Разбиение -- свойством: у шаблона Blueprint владельца
    // нет, а экземпляры builder сразу сохраняет и выходит, регистрация в
    // подсистеме редактора ему не нужна.
    static bool ApplyRuntimeGeneration(UPCGComponent* Component);

    // Иерархическая генерация графа с сеткой 64 м. true -- поменялось.
    static bool ApplyRuntimeGrid(class UPCGGraph* Graph);

    // Кэш ландшафта у PCG World Actor (через отражение: класс не экспортирован).
    // true -- поменялось.
    static bool ApplyLandscapeCacheForRuntime(AActor* PcgWorldActor);

protected:
    virtual bool RunInternal(UWorld* World, const FCellInfo& InCellInfo, FPackageSourceControlHelper& PackageHelper) override;
};
