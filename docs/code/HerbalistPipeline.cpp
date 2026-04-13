// HerbalistPipeline.cpp
#include "HerbalistPipeline.h"
#include "HerbalistCoreMath.h"
#include <cassert>
#include <algorithm>

namespace HerbalistCore
{
    FRealState Pipeline::Fold(const std::vector<FRealState>& Resources,
                              const std::vector<float>& Weights)
    {
        assert(Resources.size() == Weights.size());

        FRealState Result = FRealState::Zero();
        float TotalWeight = 0.0f;

        for (size_t i = 0; i < Resources.size(); ++i)
        {
            float w = Weights[i];
            TotalWeight += w;

            Result.Direction.Body   += Resources[i].Direction.Body   * w;
            Result.Direction.Mind   += Resources[i].Direction.Mind   * w;
            Result.Direction.Spirit += Resources[i].Direction.Spirit * w;
            Result.Direction.Nature += Resources[i].Direction.Nature * w;

            Result.Magnitude += Resources[i].Magnitude * w;

            Result.Meta.Potency    += Resources[i].Meta.Potency    * w;
            Result.Meta.Purity     += Resources[i].Meta.Purity     * w;
            Result.Meta.Stability  += Resources[i].Meta.Stability  * w;
            Result.Meta.Resonance  += Resources[i].Meta.Resonance  * w;
            Result.Meta.Corruption += Resources[i].Meta.Corruption * w;
            Result.Meta.Distortion += Resources[i].Meta.Distortion * w;
        }

        if (TotalWeight > 1e-6f)
        {
            float inv = 1.0f / TotalWeight;

            Result.Direction.Body   *= inv;
            Result.Direction.Mind   *= inv;
            Result.Direction.Spirit *= inv;
            Result.Direction.Nature *= inv;

            Result.Magnitude *= inv;

            Result.Meta.Potency    *= inv;
            Result.Meta.Purity     *= inv;
            Result.Meta.Stability  *= inv;
            Result.Meta.Resonance  *= inv;
            Result.Meta.Corruption *= inv;
            Result.Meta.Distortion *= inv;
        }

        Math::Normalize(Result.Direction);
        Result.Meta.Clamp();
        Result.Magnitude = Math::Clamp01(Result.Magnitude);

        return Result;
    }

    FRealState Pipeline::ApplyContext(const FRealState& Aggregated,
                                      const FEnvironment& Env,
                                      const FBiomeMemory& Memory)
    {
        FRealState Result = Aggregated;

        // Environment influence
        Result.Meta.Corruption = Math::Clamp01(Result.Meta.Corruption + Env.Toxicity * 0.3f);
        Result.Meta.Purity     = Math::Clamp01(Result.Meta.Purity     - Env.Toxicity * 0.2f);

        Result.Direction.Nature += Env.Fertility * 0.1f;
        Result.Meta.Potency     = Math::Clamp01(Result.Meta.Potency + Env.Fertility * 0.15f);

        Result.Meta.Distortion  = Math::Clamp01(Result.Meta.Distortion + Env.Moisture * 0.2f);
        Result.Meta.Stability   = Math::Clamp01(Result.Meta.Stability  - Env.Moisture * 0.1f);

        // Memory influence
        Result.Meta.Distortion  = Math::Clamp01(Result.Meta.Distortion + Memory.AccumulatedDistortion * 0.5f);
        Result.Meta.Stability   = Math::Clamp01(Result.Meta.Stability  + Memory.StabilityMemory * 0.2f);
        Result.Meta.Purity      = Math::Clamp01(Result.Meta.Purity     + Memory.HistoryPurity * 0.1f);

        Math::Normalize(Result.Direction);
        Result.Meta.Clamp();

        return Result;
    }

    FRealState Pipeline::ComputeDelta(const FRealState& Aggregated,
                                      const FIntent& Intent)
    {
        FRealState Delta = Aggregated;

        // Potency scales magnitude (но без взрыва)
        float potencyFactor = 1.0f + Delta.Meta.Potency * 0.5f;
        Delta.Magnitude *= potencyFactor;

        if (Intent.Type == EIntentType::Coherent)
        {
            float k = 1.0f + 0.2f * Intent.Coherence;

            Delta.Direction.Body   *= k;
            Delta.Direction.Mind   *= k;
            Delta.Direction.Spirit *= k;
            Delta.Direction.Nature *= k;

            Delta.Meta.Stability  = Math::Clamp01(Delta.Meta.Stability + 0.1f * Intent.Coherence);
            Delta.Meta.Corruption = Math::Clamp01(Delta.Meta.Corruption - 0.1f * Intent.Coherence);
        }
        else if (Intent.Type == EIntentType::Distorted)
        {
            Delta.Direction.Body   += (Intent.DirectionBias.Body   - 0.5f) * 0.3f;
            Delta.Direction.Mind   += (Intent.DirectionBias.Mind   - 0.5f) * 0.3f;
            Delta.Direction.Spirit += (Intent.DirectionBias.Spirit - 0.5f) * 0.3f;
            Delta.Direction.Nature += (Intent.DirectionBias.Nature - 0.5f) * 0.3f;

            Delta.Meta.Distortion = Math::Clamp01(Delta.Meta.Distortion + 0.2f);
            Delta.Meta.Corruption = Math::Clamp01(Delta.Meta.Corruption + 0.15f);
        }

        ApplyAxisSignModifiers(Delta);
        ApplyInteractionRules(Delta);

        Math::Normalize(Delta.Direction);
        Delta.Magnitude = Math::Clamp01(Delta.Magnitude);
        Delta.Meta.Clamp();

        return Delta;
    }

    FRealState Pipeline::ApplyMorok(const FRealState& Delta,
                                    float MorokIntensity,
                                    float CorruptionLevel,
                                    FRngState& Rng)
    {
        FRealState Result = Delta;

        float effectiveMorok = MorokIntensity * (1.0f + CorruptionLevel);

        auto noise = [&Rng, effectiveMorok]()
        {
            return (Rng.NextFloat() - 0.5f) * 0.3f * effectiveMorok;
        };

        // Direction noise
        Result.Direction.Body   += noise();
        Result.Direction.Mind   += noise();
        Result.Direction.Spirit += noise();
        Result.Direction.Nature += noise();

        // Axis mixing (декорреляция)
        float mix = effectiveMorok * 0.2f;
        Result.Direction.Body   += mix * (Delta.Direction.Mind   - Delta.Direction.Body);
        Result.Direction.Mind   += mix * (Delta.Direction.Spirit - Delta.Direction.Mind);
        Result.Direction.Spirit += mix * (Delta.Direction.Nature - Delta.Direction.Spirit);
        Result.Direction.Nature += mix * (Delta.Direction.Body   - Delta.Direction.Nature);

        // Magnitude distortion (ограниченный)
        float magNoise = noise();
        Result.Magnitude = Math::Clamp01(Result.Magnitude * (1.0f + magNoise));

        // Meta drift
        Result.Meta.Distortion = Math::Clamp01(Result.Meta.Distortion + effectiveMorok * 0.1f);
        Result.Meta.Corruption = Math::Clamp01(Result.Meta.Corruption + effectiveMorok * 0.1f);
        Result.Meta.Purity     = Math::Clamp01(Result.Meta.Purity     - effectiveMorok * 0.1f);

        Math::Normalize(Result.Direction);
        Result.Meta.Clamp();

        return Result;
    }

    FRealState Pipeline::ApplyZaryana(const FRealState& State, float ZaryanaPower)
    {
        FRealState Result = State;

        const float eps = 1e-5f;

        float maxVal = std::max({
            Result.Direction.Body,
            Result.Direction.Mind,
            Result.Direction.Spirit,
            Result.Direction.Nature
        });

        float boost = 1.0f + ZaryanaPower * 0.3f;
        float damp  = 1.0f - ZaryanaPower * 0.1f;

        Result.Direction.Body   *= (Result.Direction.Body   >= maxVal - eps) ? boost : damp;
        Result.Direction.Mind   *= (Result.Direction.Mind   >= maxVal - eps) ? boost : damp;
        Result.Direction.Spirit *= (Result.Direction.Spirit >= maxVal - eps) ? boost : damp;
        Result.Direction.Nature *= (Result.Direction.Nature >= maxVal - eps) ? boost : damp;

        Result.Meta.Purity     = Math::Clamp01(Result.Meta.Purity + ZaryanaPower * 0.1f);
        Result.Meta.Stability  = Math::Clamp01(Result.Meta.Stability + ZaryanaPower * 0.1f);
        Result.Meta.Corruption = Math::Clamp01(Result.Meta.Corruption - ZaryanaPower * 0.05f);

        Math::Normalize(Result.Direction);
        Result.Meta.Clamp();

        return Result;
    }

    FPerceivedState Pipeline::DistortPerception(const FRealState& Real,
                                                float MorokIntensity,
                                                float ZaryanaClarity,
                                                FRngState& Rng)
    {
        FPerceivedState Perceived;

        float effectiveDistortion = MorokIntensity * (1.0f - ZaryanaClarity);

        Perceived.Direction = Real.Direction;
        Perceived.Magnitude = Real.Magnitude;
        Perceived.Meta      = Real.Meta;
        Perceived.PerceivedDistortion = effectiveDistortion;

        if (effectiveDistortion > 0.01f)
        {
            auto noise = [&Rng, effectiveDistortion]()
            {
                return (Rng.NextFloat() - 0.5f) * 0.15f * effectiveDistortion;
            };

            Perceived.Direction.Body   += noise();
            Perceived.Direction.Mind   += noise();
            Perceived.Direction.Spirit += noise();
            Perceived.Direction.Nature += noise();

            float magNoise = noise();
            Perceived.Magnitude = Math::Clamp01(Perceived.Magnitude * (1.0f + magNoise));

            Perceived.Meta.Purity     = Math::Clamp01(Perceived.Meta.Purity     - effectiveDistortion * 0.1f);
            Perceived.Meta.Corruption = Math::Clamp01(Perceived.Meta.Corruption + effectiveDistortion * 0.1f);
        }

        Math::Normalize(Perceived.Direction);
        Perceived.Meta.Clamp();

        return Perceived;
    }

    void Pipeline::ApplyAxisSignModifiers(FRealState& State)
    {
        float conflictBS = std::abs(State.Direction.Body - State.Direction.Spirit);
        float conflictMN = std::abs(State.Direction.Mind - State.Direction.Nature);

        float totalConflict = conflictBS + conflictMN;

        State.Meta.Stability  = Math::Clamp01(State.Meta.Stability  - totalConflict * 0.1f);
        State.Meta.Distortion = Math::Clamp01(State.Meta.Distortion + totalConflict * 0.1f);
    }

    void Pipeline::ApplyInteractionRules(FRealState& State)
    {
        if (State.Meta.Corruption > 0.5f)
        {
            State.Meta.Purity     = Math::Clamp01(State.Meta.Purity - 0.05f);
            State.Meta.Distortion = Math::Clamp01(State.Meta.Distortion + 0.05f);
        }

        if (State.Meta.Resonance > 0.7f)
        {
            State.Meta.Potency = Math::Clamp01(State.Meta.Potency + 0.1f);
        }
    }

    FIntent Pipeline::ComputeIntent(const FAction& Action,
                                    const FPerceivedState& Perceived)
    {
        FIntent intent;

        // CORE LOCK: пока deterministic-заглушка
        intent.Type = EIntentType::Neutral;
        intent.Coherence = 0.5f;
        intent.DirectionBias = Perceived.Direction;

        return intent;
    }

} // namespace HerbalistCore