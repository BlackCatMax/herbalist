#pragma once

#include "Core/Types/HerbalistCoreTypes.h"

class PROJECTHERBALIST_API HerbalistSimulation
{
public:
    static FRealState UpdateWorld(FWorldState& World, const TArray<FRealState>& Inputs, FRngState& Rng);
};