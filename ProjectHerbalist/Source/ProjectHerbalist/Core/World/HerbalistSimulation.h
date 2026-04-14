// HerbalistSimulation.h
#pragma once

#include "Core/Types/HerbalistCoreTypes.h"

struct FRngState;

class PROJECTHERBALIST_API HerbalistSimulation
{
public:
    static FRealState UpdateWorld(FWorldState& World, const FRealState& A, const FRealState& B, FRngState& Rng);
};