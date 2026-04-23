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
    // ApplyBiomeContext (L1 версия — модифицирует FRealState)
    // =========================================================================

    void Pipeline::ApplyBiomeContext(
        float& InOutDistortion,
        float& InOutZaryanaStrength,
        FRealState& InOutDelta,
        float BiomeMorokField,
        float BiomeZaryanaField,
        const FVector4& BiomeAxisDrift)
    {
        const UHerbalistSettings* Settings = GetHerbalistSettings();
        const float MorokInfluence = Settings ? Settings->BiomeMorokInfluence : 0.3f;
        const float ZaryanaInfluence = Settings ? Settings->BiomeZaryanaInfluence : 0.3f;
        const float DriftWeight = Settings ? Settings->BiomeAxisDriftWeight : 0.1f;

        InOutDistortion = FMath::Clamp(InOutDistortion + BiomeMorokField * MorokInfluence, 0.0f, 0.95f);
        UE_LOG(LogHerbalist, Verbose, TEXT("[BIOME CONTEXT L1] MorokField=%.3f -> Distortion=%.3f"), BiomeMorokField, InOutDistortion);

        InOutZaryanaStrength = FMath::Clamp(InOutZaryanaStrength + BiomeZaryanaField * ZaryanaInfluence, 0.0f, 1.0f);
        UE_LOG(LogHerbalist, Verbose, TEXT("[BIOME CONTEXT L1] ZaryanaField=%.3f -> ZaryanaStrength=%.3f"), BiomeZaryanaField, InOutZaryanaStrength);

        InOutDelta.Direction.Body   += BiomeAxisDrift.X * DriftWeight;
        InOutDelta.Direction.Mind   += BiomeAxisDrift.Y * DriftWeight;
        InOutDelta.Direction.Spirit += BiomeAxisDrift.Z * DriftWeight;
        InOutDelta.Direction.Nature += BiomeAxisDrift.W * DriftWeight;
        UE_LOG(LogHerbalist, Verbose, TEXT("[BIOME AXIS DRIFT L1] Applied: (%.3f, %.3f, %.3f, %.3f)"),
            BiomeAxisDrift.X, BiomeAxisDrift.Y, BiomeAxisDrift.Z, BiomeAxisDrift.W);
    }

    // =========================================================================
    // ApplyBiomeContext (L2 версия — модифицирует FL2Direction и Magnitude)
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
        UE_LOG(LogHerbalist, Log, TEXT("[BIOME CONTEXT L2] MorokField=%.3f -> Distortion=%.3f"), BiomeMorokField, InOutDistortion);

        InOutZaryanaStrength = FMath::Clamp(InOutZaryanaStrength + BiomeZaryanaField * ZaryanaInfluence, 0.0f, 1.0f);
        UE_LOG(LogHerbalist, Log, TEXT("[BIOME CONTEXT L2] ZaryanaField=%.3f -> ZaryanaStrength=%.3f"), BiomeZaryanaField, InOutZaryanaStrength);

        InOutDeltaDir.Body   += BiomeAxisDrift.X * DriftWeight;
        InOutDeltaDir.Mind   += BiomeAxisDrift.Y * DriftWeight;
        InOutDeltaDir.Spirit += BiomeAxisDrift.Z * DriftWeight;
        InOutDeltaDir.Nature += BiomeAxisDrift.W * DriftWeight;

        InOutDeltaMagnitude += (BiomeAxisDrift.X + BiomeAxisDrift.Y + BiomeAxisDrift.Z + BiomeAxisDrift.W) * 0.25f * DriftWeight;
        InOutDeltaMagnitude = FMath::Clamp(InOutDeltaMagnitude, -1.0f, 1.0f);

        UE_LOG(LogHerbalist, Verbose, TEXT("[BIOME AXIS DRIFT L2] Dir: (%.3f, %.3f, %.3f, %.3f) | MagDelta=%.3f"),
            BiomeAxisDrift.X, BiomeAxisDrift.Y, BiomeAxisDrift.Z, BiomeAxisDrift.W, InOutDeltaMagnitude);
    }

} // namespace HerbalistCore