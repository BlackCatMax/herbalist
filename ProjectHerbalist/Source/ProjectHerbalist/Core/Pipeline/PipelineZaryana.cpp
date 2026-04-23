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
    // Zaryana — структурирование и стабилизация
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

        // Мягкая нелинейность для подавления экстремальных выбросов
        float Scale = 1.0f + ZaryanaStrength * 0.8f;
        Dir.Body = FMath::Tanh(Dir.Body * Scale);
        Dir.Mind = FMath::Tanh(Dir.Mind * Scale);
        Dir.Spirit = FMath::Tanh(Dir.Spirit * Scale);
        Dir.Nature = FMath::Tanh(Dir.Nature * Scale);

        // Нормализация на сферу для сохранения единичной длины
        Dir.NormalizeL2(Rng);

        UE_LOG(LogHerbalist, Log, TEXT("[ZARYANA] Strength=%.3f, Scale=%.3f | Dir=(%.3f, %.3f, %.3f, %.3f)"),
            ZaryanaStrength, Scale, Dir.Body, Dir.Mind, Dir.Spirit, Dir.Nature);
    }

} // namespace HerbalistCore