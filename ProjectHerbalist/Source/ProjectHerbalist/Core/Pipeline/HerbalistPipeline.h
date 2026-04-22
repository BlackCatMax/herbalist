// HerbalistPipeline.h
#pragma once

#include "CoreMinimal.h"
#include "Core/Types/HerbalistCoreTypes.h"
#include "Core/Inventory/HerbalistInventoryComponent.h" // для FInventoryItem

namespace HerbalistCore
{
    float Random01(FRngState& Rng);

    class Pipeline
    {
    public:
        // ========== Основной публичный API ==========

        // Агрегация списка ресурсов (Fold)
        static FRealState Fold(const TArray<FRealState>& Inputs);

        // Вычисление дельты между агрегированным состоянием и текущим состоянием биома
        static FRealState ComputeDelta(const FRealState& Aggregated, const FRealState& CurrentBiomeState);

        // Основной пайплайн
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

    private:
        // ========== Вспомогательные функции этапов пайплайна ==========

        // Разделение ингредиентов на воду и не-воду
        static void SeparateWaterAndIngredients(
            const TArray<FInventoryItem>& Inputs,
            TArray<FRealState>& OutNonWater,
            TArray<FRealState>& OutWater
        );

        // Обработка случая "только вода" -> кипяченая вода
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

        // Внедрение контекста биома (поля графа) в Distortion, ZaryanaStrength и Delta
        static void ApplyBiomeContext(
            float& InOutDistortion,
            float& InOutZaryanaStrength,
            FRealState& InOutDelta,
            float BiomeMorokField,
            float BiomeZaryanaField,
            const FVector4& BiomeAxisDrift
        );

        // Применение Potency, Resonance, Corruption к Delta
        static void ApplyPotencyResonanceCorruption(
            FRealState& InOutDelta,
            const FMeta& Meta
        );

        // Нелинейное искажение Morok
        static void ApplyMorokDistortion(
            FRealState& InOutDelta,
            float Distortion,
            FRngState& Rng
        );

        // Структурирование Zaryana
        static void ApplyZaryanaStructuring(
            FRealState& InOutDelta,
            float ZaryanaStrength,
            float Distortion
        );

        // Финальное применение дельты к состоянию с интерполяцией и клиппингом
        static FRealState FinalizeState(
            const FRealState& CurrentBiomeState,
            const FRealState& Delta
        );
    };
}