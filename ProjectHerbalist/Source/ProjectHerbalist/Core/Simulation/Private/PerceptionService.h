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

        // Без внешнего Rng (аудит 2026-09-05, решение пользователя): у
        // предмета в инвентаре нет позиции/радиуса, которым можно было бы
        // мерить "новое наблюдение", поэтому шум каждого предмета — свой
        // локальный FRandomStream, засеянный от identity+текущего State
        // этого предмета (см. ComputeInventoryPerceptionSeed в .cpp), а не
        // от общего тикового сида мира. Один и тот же предмет с одним и тем
        // же реальным состоянием ВСЕГДА виден одинаково, сколько раз тултип
        // ни открывай — усреднять уже нечего; шум меняется САМ, когда
        // меняется State (порча/сушка/отстой) — та же семантика, что "новое
        // наблюдение", без надобности отслеживать открытие/закрытие UI.
        static FPerceivedInventory ComputePerceivedInventory(const FInventorySnapshot& RealInventory, float Clarity = 0.0f);

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