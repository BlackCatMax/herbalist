// HerbalistPipeline.cpp
#include "HerbalistPipeline.h"
#include "PipelineTypes.h"
#include "ProjectHerbalist.h"
#include "Core/Types/HerbalistCoreTypes.h"
#include "Core/Inventory/HerbalistInventoryComponent.h"
#include "Core/HerbalistSettings.h"
#include "Core/Harvest/HarvestService.h"
#include "Math/UnrealMathUtility.h"
#include "Math/Vector4.h"

namespace HerbalistCore
{
    // =========================================================================
    // Основной пайплайн ApplyMorok
    // =========================================================================
    FRealState Pipeline::ApplyMorok(
        const TArray<FInventoryItem>& Inputs,
        const FRealState& CurrentBiomeState,
        const FEnvironment& Env,
        const FMemoryState& Memory,
        const FIntent& Intent,
        FRngState& Rng,
        float BiomeMorokField,
        float BiomeZaryanaField,
        const FVector4& BiomeAxisDrift)
    {
        // 1. Разделение ингредиентов
        TArray<FRealState> NonWater, Water;
        SeparateWaterAndIngredients(Inputs, NonWater, Water);

        // 2. Специальные случаи
        if (NonWater.Num() == 0 && Water.Num() > 0)
        {
            FRealState Result = ProcessWaterOnly(Water);
            UE_LOG(LogHerbalist, Log, TEXT("ALCHEMY_RESULT|WaterOnly|Mag=%.3f|Body=%.2f|Mind=%.2f|Spirit=%.2f|Nature=%.2f|Dist=%.3f|Stab=%.3f|Pur=%.3f|Pot=%.3f|Res=%.3f|Cor=%.3f"),
                Result.Magnitude, Result.Direction.Body, Result.Direction.Mind, Result.Direction.Spirit, Result.Direction.Nature,
                Result.Meta.Distortion, Result.Meta.Stability, Result.Meta.Purity, Result.Meta.Potency, Result.Meta.Resonance, Result.Meta.Corruption);
            return Result;
        }
        if (Water.Num() == 0)
        {
            FRealState Result = ProcessNoWater();
            UE_LOG(LogHerbalist, Log, TEXT("ALCHEMY_RESULT|Ash|Mag=%.3f|Body=%.2f|Mind=%.2f|Spirit=%.2f|Nature=%.2f|Dist=%.3f|Stab=%.3f|Pur=%.3f|Pot=%.3f|Res=%.3f|Cor=%.3f"),
                Result.Magnitude, Result.Direction.Body, Result.Direction.Mind, Result.Direction.Spirit, Result.Direction.Nature,
                Result.Meta.Distortion, Result.Meta.Stability, Result.Meta.Purity, Result.Meta.Potency, Result.Meta.Resonance, Result.Meta.Corruption);
            return Result;
        }

        // 3. Агрегация не-воды
        FAggregatedState NonWaterAgg = Fold(NonWater, Rng);

        // 4. Агрегация воды
        FRealState WaterAggregated = AggregateWater(Water);

        // 5. Смешивание воды и не-воды (разбавление)
        FRealState NonWaterL1;
        NonWaterL1.Direction = NonWaterAgg.Dir.ToL1();
        NonWaterL1.Magnitude = NonWaterAgg.Magnitude;
        NonWaterL1.Meta = NonWaterAgg.Meta;

        FRealState AggregatedL1 = BlendWaterAndNonWater(NonWaterL1, WaterAggregated, NonWater.Num(), Water.Num());

        // 6. Переход в рабочую геометрию
        FL2Direction AggregatedDir = ToL2(AggregatedL1.Direction, Rng);
        float AggregatedMag = AggregatedL1.Magnitude;
        FMeta AggregatedMeta = AggregatedL1.Meta;

        FL2Direction CurrentDir = ToL2(CurrentBiomeState.Direction, Rng);

        // 7. Вычисление Delta
        FL2Direction DeltaDir;
        DeltaDir.Body = AggregatedDir.Body - CurrentDir.Body;
        DeltaDir.Mind = AggregatedDir.Mind - CurrentDir.Mind;
        DeltaDir.Spirit = AggregatedDir.Spirit - CurrentDir.Spirit;
        DeltaDir.Nature = AggregatedDir.Nature - CurrentDir.Nature;

        float MagnitudeDelta = AggregatedMag - CurrentBiomeState.Magnitude;

        // 8. Применение Potency, Resonance, Corruption
        float PotencyScale = 1.0f + (AggregatedMeta.Potency - 0.5f) * 0.5f;
        DeltaDir.Body *= PotencyScale;
        DeltaDir.Mind *= PotencyScale;
        DeltaDir.Spirit *= PotencyScale;
        DeltaDir.Nature *= PotencyScale;
        MagnitudeDelta *= PotencyScale;

        // 9. Базовый Distortion и ZaryanaStrength
        float Distortion = ComputeBaseDistortion(Env, Memory);
        float ZaryanaStrength = Intent.Coherence * (1.0f - Distortion);

        // 10. Внедрение контекста биома
        ApplyBiomeContext(Distortion, ZaryanaStrength, DeltaDir, MagnitudeDelta,
            BiomeMorokField, BiomeZaryanaField, BiomeAxisDrift);

        // 11. Влияние Intent
        float IntentFactor = 0.5f + Intent.Coherence;
        DeltaDir.Body *= IntentFactor;
        DeltaDir.Mind *= IntentFactor;
        DeltaDir.Spirit *= IntentFactor;
        DeltaDir.Nature *= IntentFactor;
        MagnitudeDelta *= IntentFactor;

        // 12. Morok (матричное искажение)
        ApplyMorokDistortion(DeltaDir, Distortion, Rng);

        // 13. Zaryana (структурирование)
        ApplyZaryanaStructuring(DeltaDir, ZaryanaStrength, Distortion, Rng);

        // 14. Применение Delta к текущему состоянию
        FL2Direction NewDir;
        NewDir.Body = CurrentDir.Body + DeltaDir.Body;
        NewDir.Mind = CurrentDir.Mind + DeltaDir.Mind;
        NewDir.Spirit = CurrentDir.Spirit + DeltaDir.Spirit;
        NewDir.Nature = CurrentDir.Nature + DeltaDir.Nature;
        NewDir.NormalizeL2(Rng);

        float NewMagnitude = CurrentBiomeState.Magnitude + MagnitudeDelta;
        NewMagnitude = FMath::Clamp(NewMagnitude, 0.0f, 1.0f);

        // 15. Конвертация обратно в FRealState
        FRealState NewState;
        NewState.Direction = NewDir.ToL1();
        NewState.Magnitude = NewMagnitude;
        NewState.Meta = AggregatedMeta;

        // Финальный клиппинг
        NewState.Direction.NormalizeSum();
        NewState.Meta.Distortion = FMath::Clamp(NewState.Meta.Distortion, 0.0f, 1.0f);
        NewState.Meta.Stability = FMath::Clamp(NewState.Meta.Stability, 0.0f, 1.0f);
        NewState.Meta.Purity = FMath::Clamp(NewState.Meta.Purity, 0.0f, 1.0f);
        NewState.Meta.Potency = FMath::Clamp(NewState.Meta.Potency, 0.0f, 1.0f);
        NewState.Meta.Resonance = FMath::Clamp(NewState.Meta.Resonance, 0.0f, 1.0f);
        NewState.Meta.Corruption = FMath::Clamp(NewState.Meta.Corruption, 0.0f, 1.0f);

        UE_LOG(LogHerbalist, Log, TEXT("ALCHEMY_RESULT|Mag=%.3f|Body=%.2f|Mind=%.2f|Spirit=%.2f|Nature=%.2f|Dist=%.3f|Stab=%.3f|Pur=%.3f|Pot=%.3f|Res=%.3f|Cor=%.3f"),
            NewState.Magnitude,
            NewState.Direction.Body, NewState.Direction.Mind,
            NewState.Direction.Spirit, NewState.Direction.Nature,
            NewState.Meta.Distortion, NewState.Meta.Stability, NewState.Meta.Purity,
            NewState.Meta.Potency, NewState.Meta.Resonance, NewState.Meta.Corruption);

        return NewState;
    }

} // namespace HerbalistCore