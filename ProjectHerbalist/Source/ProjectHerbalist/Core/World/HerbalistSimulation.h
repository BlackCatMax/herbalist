#pragma once

#include "Core/Types/HerbalistCoreTypes.h"

namespace HerbalistCore
{
    class Simulation
    {
    public:
        static void Tick(FRealState& State, float DeltaTime);
    };
}