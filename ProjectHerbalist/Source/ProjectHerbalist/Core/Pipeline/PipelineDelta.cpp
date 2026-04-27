// Core/Pipeline/PipelineDelta.cpp
#include "PipelineTypes.h"
#include "HerbalistPipeline.h"
#include "Core/Types/HerbalistCoreTypes.h"

namespace HerbalistCore
{
    FDeltaState ComputeDelta(const FRealState& Before, const FRealState& After)
    {
        FDeltaState Result;
        Result.DirectionDelta.Body    = After.Direction.Body    - Before.Direction.Body;
        Result.DirectionDelta.Mind    = After.Direction.Mind    - Before.Direction.Mind;
        Result.DirectionDelta.Spirit  = After.Direction.Spirit  - Before.Direction.Spirit;
        Result.DirectionDelta.Nature  = After.Direction.Nature  - Before.Direction.Nature;

        Result.MagnitudeDelta = After.Magnitude - Before.Magnitude;

        Result.MetaDelta.Distortion   = After.Meta.Distortion   - Before.Meta.Distortion;
        Result.MetaDelta.Stability    = After.Meta.Stability    - Before.Meta.Stability;
        Result.MetaDelta.Purity       = After.Meta.Purity       - Before.Meta.Purity;
        Result.MetaDelta.Potency      = After.Meta.Potency      - Before.Meta.Potency;
        Result.MetaDelta.Resonance    = After.Meta.Resonance    - Before.Meta.Resonance;
        Result.MetaDelta.Corruption   = After.Meta.Corruption   - Before.Meta.Corruption;

        return Result;
    }
}