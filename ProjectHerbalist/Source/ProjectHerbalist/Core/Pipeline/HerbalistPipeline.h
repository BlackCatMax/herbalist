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
        // Агрегация списка ресурсов (Fold)
        static FRealState Fold(const TArray<FRealState>& Inputs);

        // Вычисление дельты между агрегированным состоянием и текущим состоянием биома
        static FRealState ComputeDelta(const FRealState& Aggregated, const FRealState& CurrentBiomeState);

        // Основной пайплайн: Fold → Delta → Distortion → Morok → Zaryana → NewState
        // Принимает массив FInventoryItem (с типами ресурсов) для корректной обработки воды
        static FRealState ApplyMorok(
            const TArray<FInventoryItem>& Inputs,
            const FRealState& CurrentBiomeState,
            const FEnvironment& Env,
            const FMemoryState& Memory,
            const FIntent& Intent,
            FRngState& Rng
        );
    };
}