// PcgResourceSlotsSetupCommandlet.h
//
// Граф слотов ресурсов (2026-09-19, этап 5б docs/research/
// DESIGN_Living_Vegetation_Research.md §4.1, решение пользователя: слоты
// запекаются в редакторе; водные растения -- на плоскости воды внутри сплайна
// биома воды и на полосе кромки у берега; клетка без слотов -- прежний
// разброс C++). Два шага, оба идемпотентны:
//   1. Собирает /Game/PCG/PCG_ResourceSlots (нет графа -- создаёт; есть --
//      не трогает: дальше его правит художник).
//   2. Добавляет в BP_WaterVolume PCG-компонент с этим графом, генерация по
//      запросу (GenerateOnDemand): в игре граф не работает, слоты читает
//      менеджер сетки из ассета RS_<карта>.
//
// Граф считается от сплайна своего актора (Get Spline Data, Self):
//   Вода  -- Create Surface From Spline -> Surface Sampler, WaterPointsPerM2
//            на м² мира. Плоскость воды SM_PoolRound плоская и лежит в
//            плоскости сплайна (L_TestDev: Z обоих -37.9), поэтому точки
//            внутренности сплайна уже на ней -- выборка по мешу без коллизий
//            не нужна.
//   Кромка -- точки по сплайну через ShoreStepLocal, случайный сдвиг до
//            ShoreBandCm, минус внутренность сплайна (Difference), проекция
//            на ландшафт.
//   Суша  -- то же с шагом LandStepLocal и сдвигом до LandRingCm: кольцо земных
//            слотов вокруг воды. Без него клетка у берега со слотами только
//            воды и кромки отдавала бы водным видам все свои ресурсы: вид
//            ресурса следует виду его слота.
// Каждая ветка пишет атрибут SlotKind и сходится в Write Herbalist Resource
// Slots.
//
// Запуск: UnrealEditor-Cmd.exe <uproject> -run=PcgResourceSlotsSetup
// Запекание слотов (без -nullrhi):
//   MSYS_NO_PATHCONV=1 UnrealEditor-Cmd.exe <uproject> -run=WorldPartitionBuilderCommandlet
//   /Game/Maps/L_TestDev -Builder=PCGWorldPartitionBuilder -IncludeGraphNames=PCG_ResourceSlots
//   -GenerateComponentEditingModeNormal -AllowCommandletRendering
#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "PcgResourceSlotsSetupCommandlet.generated.h"

class UBlueprint;
class UPCGGraph;

UCLASS()
class UPcgResourceSlotsSetupCommandlet : public UCommandlet
{
    GENERATED_BODY()

public:
    virtual int32 Main(const FString& Params) override;

    // Начальные значения графа, см. комментарий файла; дальше их подбирает
    // художник в графе. Клетка L_TestDev -- 9 м (разметка мира): кольцо суши в
    // одну клетку накрывает каждую клетку, которой касается кромка. Шаг по
    // сплайну Spline Sampler считает в ЛОКАЛЬНЫХ единицах сплайна: у
    // BP_WaterVolume на L_TestDev масштаб ~3.4 (длина 13.3 м локально, ~46 м
    // в мире), поэтому 40 и 6 -- это ~1.4 м и ~0.2 м по берегу. Плотности
    // выходят близкими, ~0.3 слота на м² у воды, кромки и кольца суши (на
    // L_TestDev вышло ~0.25), -- вид ресурса следует виду слота, и клетка
    // делится между водными и земными видами примерно по площади.
    static constexpr float WaterPointsPerM2 = 0.3f;
    static constexpr float ShoreStepLocal = 40.0f;
    static constexpr float ShoreBandCm = 150.0f;
    static constexpr float LandStepLocal = 6.0f;
    static constexpr float LandRingCm = 900.0f;

    // Собирает граф слотов в пустом графе (тест -- на временном). 1 --
    // собран, 0 -- граф не пустой (собран или правлен -- не трогаем), -1 --
    // не собрался.
    static int32 BuildSlotsGraph(UPCGGraph* Graph);

    // PCG-компонент с графом в шаблоне Blueprint. 1 -- добавлен или
    // исправлен, 0 -- уже был, -1 -- ошибка.
    static int32 EnsureSlotsComponent(UBlueprint* Blueprint, UPCGGraph* Graph);
};
