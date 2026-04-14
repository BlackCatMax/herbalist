#pragma once

#include "HerbalistPipeline.h"
#include "Core/Types/HerbalistCoreMath.h"

namespace HerbalistCore
{
    struct FBiomeState
    {
        FRealState Core;
        FEnvironment Environment;
        FBiomeMemory Memory;

        uint32_t BiomeTypeId = 0;
        int32 GridX = 0;
        int32 GridY = 0;

        // --- Perception ---
        FPerceivedState GetPerceived(float GlobalMorok,
                                     float ZaryanaClarity,
                                     FRngState& PerceptionRng) const
        {
            return Pipeline::DistortPerception(
                Core,
                GlobalMorok + Memory.AccumulatedDistortion * 0.3f,
                ZaryanaClarity,
                PerceptionRng
            );
        }

        // --- Core state update ---
        void ApplyDelta(const FRealState& Delta, EIntentType Intent)
        {
            const float ApplyRate = 0.3f;
            const float MaxStep   = 0.5f;

            // --- Direction (с ограничением влияния) ---
            FDirection deltaDir = Delta.Direction * ApplyRate;

            deltaDir.Body   = Math::Clamp(deltaDir.Body,   -MaxStep, MaxStep);
            deltaDir.Mind   = Math::Clamp(deltaDir.Mind,   -MaxStep, MaxStep);
            deltaDir.Spirit = Math::Clamp(deltaDir.Spirit, -MaxStep, MaxStep);
            deltaDir.Nature = Math::Clamp(deltaDir.Nature, -MaxStep, MaxStep);

            Core.Direction += deltaDir;
            Math::Normalize(Core.Direction);

            // --- Magnitude (симметричное насыщение + ограничение шага) ---
            float rawDeltaMag = Delta.Magnitude * ApplyRate;
            float deltaMag = Math::Clamp(rawDeltaMag, -MaxStep, MaxStep);

            if (deltaMag >= 0.0f)
                Core.Magnitude += deltaMag * (1.0f - Core.Magnitude);
            else
                Core.Magnitude += deltaMag * Core.Magnitude;

            Core.Magnitude = Math::Clamp01(Core.Magnitude);

            // --- Meta ---
            Core.Meta.Potency    = Math::Clamp01(Core.Meta.Potency    + Delta.Meta.Potency    * ApplyRate);
            Core.Meta.Purity     = Math::Clamp01(Core.Meta.Purity     + Delta.Meta.Purity     * ApplyRate);
            Core.Meta.Stability  = Math::Clamp01(Core.Meta.Stability  + Delta.Meta.Stability  * ApplyRate);
            Core.Meta.Resonance  = Math::Clamp01(Core.Meta.Resonance  + Delta.Meta.Resonance  * ApplyRate);
            Core.Meta.Corruption = Math::Clamp01(Core.Meta.Corruption + Delta.Meta.Corruption * ApplyRate);
            Core.Meta.Distortion = Math::Clamp01(Core.Meta.Distortion + Delta.Meta.Distortion * ApplyRate);

            Core.Meta.Clamp(); // страховка инварианта

            // --- Memory ---
            if (Intent == EIntentType::Coherent)
            {
                Memory.StabilityMemory       = Math::Clamp01(Memory.StabilityMemory + 0.05f);
                Memory.HistoryPurity         = Math::Clamp01(Memory.HistoryPurity   + 0.03f);
                Memory.AccumulatedDistortion = Math::Clamp01(Memory.AccumulatedDistortion - 0.02f);
            }
            else if (Intent == EIntentType::Distorted)
            {
                Memory.AccumulatedDistortion = Math::Clamp01(Memory.AccumulatedDistortion + 0.1f);
                Memory.StabilityMemory       = Math::Clamp01(Memory.StabilityMemory - 0.05f);
            }

            // Neutral — намеренно ничего не делает (инвариант)
        }
    };

} // namespace HerbalistCore