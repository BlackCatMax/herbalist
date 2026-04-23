// HerbalistPipeline.h
#pragma once

#include "CoreMinimal.h"
#include "Core/Types/HerbalistCoreTypes.h"
#include "Core/Inventory/HerbalistInventoryComponent.h"

// Forward declarations
struct FAggregatedL2;
struct FDeltaL2;

namespace HerbalistCore
{
    // Случайные числа
    inline float Random01(FRngState& Rng)
    {
        Rng.Seed = (Rng.Seed * 196314165) + 907633515;
        return (Rng.Seed & 0x00FFFFFF) / float(0x01000000);
    }

    inline float RandomRange(FRngState& Rng, float Min, float Max)
    {
        return Min + (Max - Min) * Random01(Rng);
    }

    class Pipeline
    {
    public:
        // ========== Основной публичный API ==========

        // Агрегация списка ресурсов (Fold) - L1 версия
        static FRealState Fold(const TArray<FRealState>& Inputs);

        // Вычисление дельты между агрегированным состоянием и текущим состоянием биома - L1 версия
        static FRealState ComputeDelta(const FRealState& Aggregated, const FRealState& CurrentBiomeState);

        // Основной пайплайн - L1 версия (старая)
        static FRealState ApplyMorok(
            const TArray<FInventoryItem>& Inputs,
            const FRealState& CurrentBiomeState,
            const FEnvironment& Env,
            const FMemoryState& Memory,
            const FIntent& Intent,
            FRngState& Rng,
            float BiomeMorokField = 0.0f,
            float BiomeZaryanaField = 0.0f,
            const FVector4& BiomeAxisDrift = FVector4(0.25f, 0.25f, 0.25f, 0.25f)
        );

        // Основной пайплайн - L2 версия (новая)
        static FRealState ApplyMorokL2(
            const TArray<FInventoryItem>& Inputs,
            const FRealState& CurrentBiomeState,
            const FEnvironment& Env,
            const FMemoryState& Memory,
            const FIntent& Intent,
            FRngState& Rng,
            float BiomeMorokField = 0.0f,
            float BiomeZaryanaField = 0.0f,
            const FVector4& BiomeAxisDrift = FVector4(0.25f, 0.25f, 0.25f, 0.25f)
        );

        // ========== Вспомогательные функции (публичные для тестирования) ==========

        // Разделение ингредиентов на воду и не-воду
        static void SeparateWaterAndIngredients(
            const TArray<FInventoryItem>& Inputs,
            TArray<FRealState>& OutNonWater,
            TArray<FRealState>& OutWater
        );

        // Обработка случая "только вода" -> варёная вода
        static FRealState ProcessWaterOnly(const TArray<FRealState>& WaterStates);

        // Обработка случая "нет воды" -> зола
        static FRealState ProcessNoWater();

        // Агрегация нескольких порций воды в одну
        static FRealState AggregateWater(const TArray<FRealState>& WaterStates);

        // Смешивание агрегированной не-воды и воды с учётом разбавления
        static FRealState BlendWaterAndNonWater(
            const FRealState& NonWaterAggregated,
            const FRealState& WaterAggregated,
            int32 NonWaterCount,
            int32 WaterCount
        );

        // Вычисление базового Distortion из среды и памяти клетки
        static float ComputeBaseDistortion(const FEnvironment& Env, const FMemoryState& Memory);

        // Внедрение контекста биома (поля графа) в Distortion, ZaryanaStrength и Delta (L1 версия)
        static void ApplyBiomeContext(
            float& InOutDistortion,
            float& InOutZaryanaStrength,
            FRealState& InOutDelta,
            float BiomeMorokField,
            float BiomeZaryanaField,
            const FVector4& BiomeAxisDrift
        );

        // Применение Potency, Resonance, Corruption к Delta (L1 версия)
        static void ApplyPotencyResonanceCorruption(
            FRealState& InOutDelta,
            const FMeta& Meta
        );

        // Нелинейное искажение Morok (L1 версия)
        static void ApplyMorokDistortion(
            FRealState& InOutDelta,
            float Distortion,
            FRngState& Rng
        );

        // Структурирование Zaryana (L1 версия)
        static void ApplyZaryanaStructuring(
            FRealState& InOutDelta,
            float ZaryanaStrength,
            float Distortion
        );

        // Финальное применение дельты к состоянию с интерполяцией и клиппингом (L1 версия)
        static FRealState FinalizeState(
            const FRealState& CurrentBiomeState,
            const FRealState& Delta
        );
    };

    // ========== Свободные функции для L2 пайплайна ==========

    // Агрегация в L2
    FAggregatedL2 FoldL2(const TArray<FRealState>& Inputs, FRngState& Rng);

    // Вычисление дельты в L2
    FDeltaL2 ComputeDeltaL2(const FAggregatedL2& Aggregated, const FRealState& CurrentBiomeState, FRngState& Rng);

    // Применение Morok матрицы к L2 направлению
    void ApplyMorokMatrix(FL2Direction& Dir, float Distortion, FRngState& Rng);

    // Применение Zaryana структурирования к L2 направлению
    void ApplyZaryanaStructuring(FL2Direction& Dir, float ZaryanaStrength, float Distortion, FRngState& Rng);

    // Внедрение контекста биома для L2
    void ApplyBiomeContext(
        float& InOutDistortion,
        float& InOutZaryanaStrength,
        FL2Direction& InOutDeltaDir,
        float& InOutDeltaMagnitude,
        float BiomeMorokField,
        float BiomeZaryanaField,
        const FVector4& BiomeAxisDrift
    );

} // namespace HerbalistCore