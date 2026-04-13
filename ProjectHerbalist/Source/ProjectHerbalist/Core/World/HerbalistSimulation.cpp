#include "Core/World/HerbalistSimulation.h"
#include "Core/Types/HerbalistCoreMath.h"

using namespace HerbalistCore;

void Simulation::Tick(FRealState& State, float DeltaTime)
{
    State.Meta.Stability = Math::Clamp01(State.Meta.Stability - DeltaTime * 0.01f);
}