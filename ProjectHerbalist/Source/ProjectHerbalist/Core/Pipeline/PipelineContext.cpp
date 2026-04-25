// PipelineContext.cpp
#include "ProjectHerbalist.h"
#include "HerbalistPipeline.h"
#include "PipelineTypes.h"
#include "Core/HerbalistSettings.h"
#include "Math/UnrealMathUtility.h"

namespace HerbalistCore
{
    // =========================================================================
    // ComputeBaseDistortion
    // =========================================================================

    float Pipeline::ComputeBaseDistortion(const FEnvironment& Env, const FMemoryState& Memory)
    {
        const UHerbalistSettings* Settings = GetHerbalistSettings();
        const float ToxWeight = Settings ? Settings->EnvironmentToxicityWeight : 0.5f;

        float EnvDist = Env.Toxicity * ToxWeight;
        float MemoryDist = Memory.AccumulatedDistortion;
        float Distortion = 1.0f - (1.0f - EnvDist) * (1.0f - MemoryDist);
        Distortion = FMath::Clamp(Distortion, 0.0f, 0.95f);
        return Distortion;
    }

    // =========================================================================
    // ApplyBiomeContext
    // =========================================================================

    void ApplyBiomeContext(
        float& InOutDistortion,
        float& InOutZaryanaStrength,
        FL2Direction& InOutDeltaDir,
        float& InOutDeltaMagnitude,
        float BiomeMorokField,
        float BiomeZaryanaField,
        const FVector4& BiomeAxisDrift)
    {
        const UHerbalistSettings* Settings = GetHerbalistSettings();
        const float MorokInfluence = Settings ? Settings->BiomeMorokInfluence : 0.3f;
        const float ZaryanaInfluence = Settings ? Settings->BiomeZaryanaInfluence : 0.3f;
        const float DriftWeight = Settings ? Settings->BiomeAxisDriftWeight : 0.1f;

        InOutDistortion = FMath::Clamp(InOutDistortion + BiomeMorokField * MorokInfluence, 0.0f, 0.95f);
        UE_LOG(LogHerbalist, Log, TEXT("[BIOME CONTEXT] MorokField=%.3f -> Distortion=%.3f"), BiomeMorokField, InOutDistortion);

        InOutZaryanaStrength = FMath::Clamp(InOutZaryanaStrength + BiomeZaryanaField * ZaryanaInfluence, 0.0f, 1.0f);
        UE_LOG(LogHerbalist, Log, TEXT("[BIOME CONTEXT] ZaryanaField=%.3f -> ZaryanaStrength=%.3f"), BiomeZaryanaField, InOutZaryanaStrength);

        InOutDeltaDir.Body += BiomeAxisDrift.X * DriftWeight;
        InOutDeltaDir.Mind += BiomeAxisDrift.Y * DriftWeight;
        InOutDeltaDir.Spirit += BiomeAxisDrift.Z * DriftWeight;
        InOutDeltaDir.Nature += BiomeAxisDrift.W * DriftWeight;

        InOutDeltaMagnitude += (BiomeAxisDrift.X + BiomeAxisDrift.Y + BiomeAxisDrift.Z + BiomeAxisDrift.W) * 0.25f * DriftWeight;
        InOutDeltaMagnitude = FMath::Clamp(InOutDeltaMagnitude, -1.0f, 1.0f);

        UE_LOG(LogHerbalist, Verbose, TEXT("[BIOME AXIS DRIFT] Dir:(%.3f, %.3f, %.3f, %.3f) | MagDelta=%.3f"),
            BiomeAxisDrift.X, BiomeAxisDrift.Y, BiomeAxisDrift.Z, BiomeAxisDrift.W, InOutDeltaMagnitude);
    }

} // namespace HerbalistCore
