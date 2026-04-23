// PipelineZaryana.cpp
#include "ProjectHerbalist.h"
#include "HerbalistPipeline.h"
#include "PipelineTypes.h"
#include "Core/HerbalistSettings.h"
#include "Core/Types/HerbalistCoreMath.h"
#include "Math/UnrealMathUtility.h"

namespace HerbalistCore
{
    // =========================================================================
    // L1 Zaryana Structuring (старая версия)
    // =========================================================================

    void Pipeline::ApplyZaryanaStructuring(
        FRealState& InOutDelta,
        float ZaryanaStrength,
        float Distortion)
    {
        const UHerbalistSettings* Settings = GetHerbalistSettings();
        const float BoostFactor = Settings ? Settings->ZaryanaBoostFactor : 0.5f;
        const float SuppressFactor = Settings ? Settings->ZaryanaSuppressFactor : 0.3f;

        float AvgDir = (InOutDelta.Direction.Body + InOutDelta.Direction.Mind +
            InOutDelta.Direction.Spirit + InOutDelta.Direction.Nature) / 4.0f;
        float Boost = 1.0f + ZaryanaStrength * BoostFactor;
        float Suppress = 1.0f - ZaryanaStrength * SuppressFactor;

        InOutDelta.Direction.Body = (InOutDelta.Direction.Body > AvgDir) ? InOutDelta.Direction.Body * Boost : InOutDelta.Direction.Body * Suppress;
        InOutDelta.Direction.Mind = (InOutDelta.Direction.Mind > AvgDir) ? InOutDelta.Direction.Mind * Boost : InOutDelta.Direction.Mind * Suppress;
        InOutDelta.Direction.Spirit = (InOutDelta.Direction.Spirit > AvgDir) ? InOutDelta.Direction.Spirit * Boost : InOutDelta.Direction.Spirit * Suppress;
        InOutDelta.Direction.Nature = (InOutDelta.Direction.Nature > AvgDir) ? InOutDelta.Direction.Nature * Boost : InOutDelta.Direction.Nature * Suppress;

        float StabilityIncrease = ZaryanaStrength * (1.0f - Distortion) * 0.2f;
        float PurityIncrease = ZaryanaStrength * (1.0f - Distortion) * 0.15f;
        InOutDelta.Meta.Stability += StabilityIncrease;
        InOutDelta.Meta.Purity += PurityIncrease;
        InOutDelta.Meta.Stability = FMath::Clamp(InOutDelta.Meta.Stability, -1.0f, 1.0f);
        InOutDelta.Meta.Purity = FMath::Clamp(InOutDelta.Meta.Purity, -1.0f, 1.0f);

        InOutDelta.Direction.NormalizeSum();

        UE_LOG(LogHerbalist, Verbose, TEXT("[ZARYANA L1] Boost: %.2f, Suppress: %.2f, Stability+%.3f, Purity+%.3f"),
            Boost, Suppress, StabilityIncrease, PurityIncrease);
    }

    // =========================================================================
    // L2 Zaryana Structuring (новая версия — с tanh и нормализацией)
    // =========================================================================

    void ApplyZaryanaStructuring(FL2Direction& Dir, float ZaryanaStrength, float Distortion, FRngState& Rng)
    {
        const UHerbalistSettings* Settings = GetHerbalistSettings();
        const float BoostFactor = Settings ? Settings->ZaryanaBoostFactor : 0.5f;
        const float SuppressFactor = Settings ? Settings->ZaryanaSuppressFactor : 0.3f;

        // Усиление доминант и подавление слабых
        float Avg = (Dir.Body + Dir.Mind + Dir.Spirit + Dir.Nature) / 4.0f;
        auto Process = [&](float& val) {
            if (val > Avg)
                val *= (1.0f + ZaryanaStrength * BoostFactor);
            else
                val *= (1.0f - ZaryanaStrength * SuppressFactor);
        };
        Process(Dir.Body);
        Process(Dir.Mind);
        Process(Dir.Spirit);
        Process(Dir.Nature);

        // Мягкая нелинейность (tanh) для подавления экстремальных выбросов
        float Scale = 1.0f + ZaryanaStrength * 0.8f;
        Dir.Body   = FMath::Tanh(Dir.Body * Scale);
        Dir.Mind   = FMath::Tanh(Dir.Mind * Scale);
        Dir.Spirit = FMath::Tanh(Dir.Spirit * Scale);
        Dir.Nature = FMath::Tanh(Dir.Nature * Scale);

        // Нормализация на сферу для сохранения единичной длины
        Dir.NormalizeL2(Rng);

        UE_LOG(LogHerbalist, Log, TEXT("[ZARYANA L2] Strength=%.3f, Scale=%.3f | Dir=(%.3f, %.3f, %.3f, %.3f)"),
            ZaryanaStrength, Scale, Dir.Body, Dir.Mind, Dir.Spirit, Dir.Nature);
    }

} // namespace HerbalistCore