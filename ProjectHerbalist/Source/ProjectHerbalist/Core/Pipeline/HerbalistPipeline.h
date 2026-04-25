// HerbalistPipeline.h
#pragma once

#include "CoreMinimal.h"
#include "Core/Types/HerbalistCoreTypes.h"
#include "Core/Inventory/HerbalistInventoryComponent.h"

// Forward declarations
struct FAggregatedState;
struct FDeltaState;

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

        // Основной пайплайн с матричным Morok и векторной геометрией
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

        // ========== Вспомогательные функции ==========

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
    };

    // ========== Свободные функции пайплайна ==========

    // Агрегация ингредиентов
    FAggregatedState Fold(const TArray<FRealState>& Inputs, FRngState& Rng);

    // Вычисление дельты
    FDeltaState ComputeDelta(const FAggregatedState& Aggregated, const FRealState& CurrentBiomeState, FRngState& Rng);

    // Матричное искажение Morok
    void ApplyMorokDistortion(FL2Direction& Dir, float Distortion, FRngState& Rng);

    // Структурирование Zaryana
    void ApplyZaryanaStructuring(FL2Direction& Dir, float ZaryanaStrength, float Distortion, FRngState& Rng);

    // Внедрение контекста биома
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
