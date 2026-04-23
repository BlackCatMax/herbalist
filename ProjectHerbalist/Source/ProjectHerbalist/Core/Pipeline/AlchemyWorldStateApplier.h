// AlchemyWorldStateApplier.h
#pragma once
#include "CoreMinimal.h"
#include "Core/Types/HerbalistCoreTypes.h"
#include "AlchemySemanticResolver.h"
#include "AlchemyPhysicsPipeline.h"

class FAlchemyWorldStateApplier
{
public:
    static FRealState Apply(
        const FAlchemySemanticResult& Semantic,
        const FAlchemyPhysicsResult& Physics,
        FRngState& Rng);
};