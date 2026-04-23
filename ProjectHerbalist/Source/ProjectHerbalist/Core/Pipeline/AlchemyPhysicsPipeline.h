// AlchemyPhysicsPipeline.h
#pragma once
#include "CoreMinimal.h"
#include "Core/Types/HerbalistCoreTypes.h"
#include "AlchemySemantics.h"
#include "AlchemyTypes.h"

struct FAlchemyPhysicsResult
{
    FRealState State;
    bool bCatastropheTriggered = false;
    bool bCollapse = false;
};

class FAlchemyPhysicsPipeline
{
public:
    static FAlchemyPhysicsResult Run(
        const TArray<FRealState>& IngredientStates,
        const TArray<FRealState>& WaterStates,
        const FRealState& CellState,
        const FEnvironment& Env,
        const FMemoryState& Memory,
        float Coherence,
        FRngState& Rng,
        float MorokField,
        float ZaryanaField,
        const FVector4& AxisDrift);

private:
    static bool ShouldTriggerCatastrophe(const FRealState& State, const FMemoryState& Memory, FRngState& Rng);
};