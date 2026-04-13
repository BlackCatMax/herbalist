#pragma once

#include "HerbalistBiomeState.h"
#include <vector>
#include <cstdint>

namespace HerbalistCore
{
    // ---- Хранение Delta + Intent (КРИТИЧЕСКИЙ FIX) ----
    struct FPendingDelta
    {
        FRealState Delta;
        EIntentType Intent = EIntentType::Neutral;
    };

    class Simulation
    {
    public:
        Simulation(uint32_t WorldSizeX, uint32_t WorldSizeY, uint32_t Seed = 0);

        const FBiomeState& GetBiome(uint32_t Index) const;
        const FBiomeState& GetBiomeAt(int32 X, int32 Y) const;

        uint32_t GetBiomeCount() const { return static_cast<uint32_t>(Biomes.size()); }
        uint32_t GetSizeX() const { return SizeX; }
        uint32_t GetSizeY() const { return SizeY; }
        uint32_t GetCurrentTick() const { return CurrentTick; }

        uint32_t GetIndex(int32 X, int32 Y) const { return Y * SizeX + X; }

        float GetGlobalMorok() const { return GlobalMorok; }
        float GetZaryanaClarity() const { return ZaryanaClarity; }
        float GetWorldCoherence() const;

        bool IsBuyanAccessible() const { return GetWorldCoherence() > BuyanThreshold; }

        void AddZaryanaFragment(float Amount)
        {
            ZaryanaClarity = Math::Clamp01(ZaryanaClarity + Amount);
        }

        void Step(float DeltaLogicalTime);
        void AdvanceSimulation(uint32_t Steps);

        void ProcessAction(const FAction& Action, const FPerceivedState& PerceivedContext);
        FPerceivedState GetPerceivedStateForBiome(uint32_t BiomeIndex) const;

        // ---- Метрики ----
        float ComputeDirectionalVariance() const;
        float ComputeAverageMagnitude() const;
        float ComputeAverageAccumulatedDistortion() const;

    private:
        std::vector<FBiomeState> Biomes;
        std::vector<FPendingDelta> PendingDeltas;

        uint32_t SizeX = 0;
        uint32_t SizeY = 0;

        uint32_t CurrentTick = 0;

        mutable FRngState SimulationRng;

        float GlobalMorok = 0.3f;
        float ZaryanaClarity = 0.0f;
        float WorldCoherence = 0.5f;

        const float BuyanThreshold = 0.85f;

        void PropagateChanges();
        void ApplyPendingDeltas();
        void UpdateGlobalMetrics();
    };

} // namespace HerbalistCore