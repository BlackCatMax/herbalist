// AlchemySemantics.h
#pragma once
#include "CoreMinimal.h"
#include "Core/Types/HerbalistCoreTypes.h"

namespace HerbalistCore
{
    EAlchemyOutcome ClassifyOutcome(const TArray<FInventoryItem>& Inputs);

    FRealState ApplyAshTransform(const FMeta& CoreMeta);
    FRealState ApplyBoiledWaterTransform(const TArray<FRealState>& WaterStates);
    FRealState ApplyCatastropheTransform(FRealState& InState, bool bCollapse, FRngState& Rng);
}