// PerceptionService.h
#pragma once
#include "PerceivedTypes.h"
#include "SnapshotTypes.h"
#include "Math/RandomStream.h"

namespace Simulation
{
    class PROJECTHERBALIST_API FPerceptionService
    {
    public:
        static FPerceivedWorld ComputePerceivedWorld(const FWorldSnapshot& RealWorld, FRandomStream& Rng, float Clarity = 0.0f);
        static FPerceivedInventory ComputePerceivedInventory(const FInventorySnapshot& RealInventory, FRandomStream& Rng, float Clarity = 0.0f);

        // Общая формула искажения: шум масштабируется собственным Distortion
        // объекта (S_Perceived = Distort(S_real, Morok) — при Distortion≈0 почти
        // не искажает, при высоком Distortion шум заметен), а не фиксированный
        // диапазон независимо от состояния.
        //
        // Clarity ∈ [0,1] (обсуждение в сессии 2026-08-24, "Прогрессия через
        // Заряну", 06_Progression.md) — AGridWorldManager::GlobalPerceptionClarity,
        // растёт с каждым подлинно собранным фрагментом памяти Заряны. Гасит
        // долю шума, который создаёт Distortion (NoiseScale = Distortion *
        // (1 - Clarity)).
        //
        // ЧЕСТНО: это числовое улучшение восприятия, а 06_Progression прямо
        // запрещает "явные численные улучшения". Отличие от рядового фарма,
        // которое сделало пункт 8 Фазы C (сравнение записей Травника, не
        // правку шума) — конечность и нарративная привязка: фрагментов мало
        // (3 в v1, не бесконечный грайнд) и получение каждого — сюжетный
        // бит, не механическая награда за повтор действия. Ближе к "герою
        // передали знание" (обряд знахарки на смертном одре, уже в каноне),
        // чем к скрытому скилл-триггеру. Решение пользователя, явно данное
        // вместе со спекой фрагментов — не моя тихая трактовка канона.
        // Default 0 — старое поведение там, где Clarity не касается (например,
        // презентационный шум AlchemySlotWidget).
        static FRealState PerceiveRealState(const FRealState& Real, FRandomStream& Rng, float Clarity = 0.0f);
    };
}