// Core/Simulation/Private/TraceReplay.h
#pragma once

#include "CoreMinimal.h"
#include "SnapshotTypes.h"
#include "DeltaTypes.h"
#include "CommandTypes.h"
#include "TraceTypes.h"
#include "Math/RandomStream.h"

namespace Simulation
{
    /**
     * Повторяет выполнение команд из кадра трассировки и сравнивает дельты.
     * @return true, если дельта полностью совпадает.
     */
    // PROJECTHERBALIST_API добавлен 2026-08-31 (полный аудит проекта) —
    // без него функция не экспортировалась из модуля, ProjectHerbalistTests
    // (отдельный модуль) не мог её слинковать (тот же довод, что уже
    // экспортировал FPerceptionService рядом по этой же причине).
    PROJECTHERBALIST_API bool ReplayAndCompare(const FTraceFrame& Frame, FRandomStream& Rng);
}