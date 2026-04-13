#include "HerbalistSimulation.h"
#include <cassert>

namespace HerbalistCore
{
    // ---- Новый тип для хранения Delta + Intent ----
    struct FPendingDelta
    {
        FRealState Delta;
        EIntentType Intent = EIntentType::Neutral;
    };

    Simulation::Simulation(uint32_t WorldSizeX, uint32_t WorldSizeY, uint32_t Seed)
        : SizeX(WorldSizeX), SizeY(WorldSizeY), SimulationRng(Seed)
    {
        Biomes.resize(SizeX * SizeY);

        PendingDeltas.resize(Biomes.size());
        for (auto& pd : PendingDeltas)
        {
            pd.Delta = FRealState::Zero();
            pd.Intent = EIntentType::Neutral;
        }

        for (uint32_t y = 0; y < SizeY; ++y)
        {
            for (uint32_t x = 0; x < SizeX; ++x)
            {
                uint32_t idx = y * SizeX + x;
                Biomes[idx].Core = GetS0();
                Biomes[idx].GridX = x;
                Biomes[idx].GridY = y;
            }
        }
    }

    const FBiomeState& Simulation::GetBiome(uint32_t Index) const
    {
        assert(Index < Biomes.size());
        return Biomes[Index];
    }

    const FBiomeState& Simulation::GetBiomeAt(int32 X, int32 Y) const
    {
        X = std::clamp(X, 0, (int32)SizeX - 1);
        Y = std::clamp(Y, 0, (int32)SizeY - 1);
        return Biomes[Y * SizeX + X];
    }

    float Simulation::GetWorldCoherence() const
    {
        return WorldCoherence;
    }

    void Simulation::Step(float DeltaLogicalTime)
    {
        // Естественное затухание
        for (auto& biome : Biomes)
        {
            biome.Memory.AccumulatedDistortion *= (1.0f - 0.01f * DeltaLogicalTime);
            biome.Core.Meta.Distortion = Math::Clamp01(
                biome.Core.Meta.Distortion * (1.0f - 0.005f * DeltaLogicalTime)
            );
        }

        PropagateChanges();
        ApplyPendingDeltas();
        UpdateGlobalMetrics();

        ++CurrentTick;
    }

    void Simulation::AdvanceSimulation(uint32_t Steps)
    {
        for (uint32_t i = 0; i < Steps; ++i)
        {
            Step(1.0f);
        }
    }

    void Simulation::ProcessAction(const FAction& Action, const FPerceivedState& PerceivedContext)
    {
        uint32_t uniqueId = HashCombine(Action.BiomeIndex, Action.LogicalTick);
        FRngState localRng = BranchRng(SimulationRng, uniqueId);

        FIntent intent = Pipeline::ComputeIntent(Action, PerceivedContext);

        // Заглушка ресурсов (будет заменено системой ингредиентов)
        std::vector<FRealState> resources = { GetS0() };
        std::vector<float> weights = { 1.0f };

        FRealState aggregated = Pipeline::Fold(resources, weights);

        const FBiomeState& targetBiome = GetBiome(Action.BiomeIndex);

        FRealState contextualized = Pipeline::ApplyContext(
            aggregated,
            targetBiome.Environment,
            targetBiome.Memory
        );

        FRealState delta = Pipeline::ComputeDelta(contextualized, intent);

        delta = Pipeline::ApplyMorok(
            delta,
            GlobalMorok,
            targetBiome.Memory.AccumulatedDistortion,
            localRng
        );

        delta = Pipeline::ApplyZaryana(delta, ZaryanaClarity);

        // ---- КРИТИЧЕСКИЙ ФИКС: сохраняем Intent ----
        PendingDeltas[Action.BiomeIndex].Delta += delta;
        PendingDeltas[Action.BiomeIndex].Intent = intent.Type;
    }

    FPerceivedState Simulation::GetPerceivedStateForBiome(uint32_t BiomeIndex) const
    {
        const FBiomeState& biome = GetBiome(BiomeIndex);

        uint32_t perceptionId = HashCombine(BiomeIndex, 0xDEADBEEF);
        FRngState perceptionRng = BranchRng(SimulationRng, perceptionId);

        return biome.GetPerceived(GlobalMorok, ZaryanaClarity, perceptionRng);
    }

    void Simulation::PropagateChanges()
    {
        const float propagationFactor = 0.1f;

        std::vector<FPendingDelta> propagationBuffer(Biomes.size());

        for (size_t i = 0; i < Biomes.size(); ++i)
        {
            propagationBuffer[i].Delta = PendingDeltas[i].Delta * propagationFactor;
            propagationBuffer[i].Intent = PendingDeltas[i].Intent;

            PendingDeltas[i].Delta = PendingDeltas[i].Delta * (1.0f - propagationFactor);
        }

        for (uint32_t y = 0; y < SizeY; ++y)
        {
            for (uint32_t x = 0; x < SizeX; ++x)
            {
                uint32_t idx = y * SizeX + x;

                if (x > 0)
                {
                    PendingDeltas[idx - 1].Delta += propagationBuffer[idx].Delta;
                }
                if (x < SizeX - 1)
                {
                    PendingDeltas[idx + 1].Delta += propagationBuffer[idx].Delta;
                }
                if (y > 0)
                {
                    PendingDeltas[idx - SizeX].Delta += propagationBuffer[idx].Delta;
                }
                if (y < SizeY - 1)
                {
                    PendingDeltas[idx + SizeX].Delta += propagationBuffer[idx].Delta;
                }
            }
        }
    }

    void Simulation::ApplyPendingDeltas()
    {
        for (size_t i = 0; i < Biomes.size(); ++i)
        {
            Biomes[i].ApplyDelta(
                PendingDeltas[i].Delta,
                PendingDeltas[i].Intent
            );

            PendingDeltas[i].Delta = FRealState::Zero();
            PendingDeltas[i].Intent = EIntentType::Neutral;
        }
    }

    void Simulation::UpdateGlobalMetrics()
    {
        float totalDistance = 0.0f;

        for (const auto& biome : Biomes)
        {
            totalDistance += Math::DistanceToS0(biome.Core);
        }

        float avgDistance = totalDistance / Biomes.size();
        WorldCoherence = 1.0f - avgDistance;

        float avgDistortion = 0.0f;
        float avgAccumulated = 0.0f;

        for (const auto& biome : Biomes)
        {
            avgDistortion += biome.Core.Meta.Distortion;
            avgAccumulated += biome.Memory.AccumulatedDistortion;
        }

        avgDistortion /= Biomes.size();
        avgAccumulated /= Biomes.size();

        GlobalMorok = Math::Clamp01(
            0.2f +
            avgDistortion * 0.5f +
            avgAccumulated * 0.3f
        );
    }

    float Simulation::ComputeDirectionalVariance() const
    {
        if (Biomes.empty()) return 0.0f;

        FDirection mean = { 0,0,0,0 };

        for (const auto& biome : Biomes)
        {
            mean.Body   += biome.Core.Direction.Body;
            mean.Mind   += biome.Core.Direction.Mind;
            mean.Spirit += biome.Core.Direction.Spirit;
            mean.Nature += biome.Core.Direction.Nature;
        }

        float invN = 1.0f / Biomes.size();

        mean.Body   *= invN;
        mean.Mind   *= invN;
        mean.Spirit *= invN;
        mean.Nature *= invN;

        float variance = 0.0f;

        for (const auto& biome : Biomes)
        {
            variance += Math::Distance(biome.Core.Direction, mean);
        }

        return variance * invN;
    }

    float Simulation::ComputeAverageMagnitude() const
    {
        float sum = 0.0f;
        for (const auto& biome : Biomes)
        {
            sum += biome.Core.Magnitude;
        }
        return sum / Biomes.size();
    }

    float Simulation::ComputeAverageAccumulatedDistortion() const
    {
        float sum = 0.0f;
        for (const auto& biome : Biomes)
        {
            sum += biome.Memory.AccumulatedDistortion;
        }
        return sum / Biomes.size();
    }

} // namespace HerbalistCore