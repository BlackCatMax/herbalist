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
    bool ReplayAndCompare(const FTraceFrame& Frame, FRandomStream& Rng);
}