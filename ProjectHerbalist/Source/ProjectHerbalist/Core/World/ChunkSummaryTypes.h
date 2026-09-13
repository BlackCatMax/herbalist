// Core/World/ChunkSummaryTypes.h
//
// Сводки чанков (разметка мира, этап 7, DESIGN_World_Layout.md §9): всё, что
// раньше считалось обходом всего мира -- на каждом шаге графа биомов, раз в
// несколько секунд для Буяна и фрагментов памяти, в отчёте о порче.

#pragma once

#include "CoreMinimal.h"
#include "Core/Types/HerbalistCoreTypes.h"

// Число биомов: индекс суммы по биому -- EBiomeType. Новый биом после Bog --
// поправить здесь (BuildChunkSummary проверяет индекс).
inline constexpr int32 HerbalistBiomeTypeCount = static_cast<int32>(EBiomeType::Bog) + 1;

// Сумма по биому в сводке чанка. Морок -- отклонение Distortion клетки от
// дефолта её биома или воды, Заряна -- отклонение Stability (доводы -- у
// AGridWorldManager::BuildChunkSummary).
struct FHerbalistBiomeFieldSum
{
    double MorokSum = 0.0;
    double ZaryanaSum = 0.0;
    FVector PositionSum = FVector::ZeroVector;
    int32 CellCount = 0;
};

struct FHerbalistChunkSummary
{
    // По биому, индекс -- EBiomeType. Массив, а не TMap (ревью этапа 7):
    // сводка строится на каждом шаге графа и не должна аллоцировать.
    FHerbalistBiomeFieldSum Biomes[HerbalistBiomeTypeCount];
    int32 CellCount = 0;
    int32 DegradingCount = 0;
    double DistortionSum = 0.0;
    // Минимум и максимум осмысленны только при CellCount > 0.
    float DistortionMin = 1.0f;
    float DistortionMax = 0.0f;
    int32 LandCellCount = 0;
    double LandDistortionSum = 0.0;
    double DistanceWithHistorySum = 0.0;
};
