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
    // L1: ComputeDelta
    // =========================================================================
    FRealState Pipeline::ComputeDelta(const FRealState& Aggregated, const FRealState& CurrentBiomeState)
    {
        FRealState Delta;
        Delta.Direction.Body = Aggregated.Direction.Body - CurrentBiomeState.Direction.Body;
        Delta.Direction.Mind = Aggregated.Direction.Mind - CurrentBiomeState.Direction.Mind;
        Delta.Direction.Spirit = Aggregated.Direction.Spirit - CurrentBiomeState.Direction.Spirit;
        Delta.Direction.Nature = Aggregated.Direction.Nature - CurrentBiomeState.Direction.Nature;

        Delta.Magnitude = Aggregated.Magnitude - CurrentBiomeState.Magnitude;
        Delta.Meta.Distortion = Aggregated.Meta.Distortion - CurrentBiomeState.Meta.Distortion;
        Delta.Meta.Stability = Aggregated.Meta.Stability - CurrentBiomeState.Meta.Stability;
        Delta.Meta.Purity = Aggregated.Meta.Purity - CurrentBiomeState.Meta.Purity;
        Delta.Meta.Potency = Aggregated.Meta.Potency - CurrentBiomeState.Meta.Potency;
        Delta.Meta.Resonance = Aggregated.Meta.Resonance - CurrentBiomeState.Meta.Resonance;
        Delta.Meta.Corruption = Aggregated.Meta.Corruption - CurrentBiomeState.Meta.Corruption;

        return Delta;
    }

    // =========================================================================
    // L2: ComputeDeltaL2
    // =========================================================================
    FDeltaL2 ComputeDeltaL2(const FAggregatedL2& Aggregated, const FRealState& CurrentBiomeState, FRngState& Rng)
    {
        FL2Direction CurrentDir = ToL2(CurrentBiomeState.Direction, Rng);

        FDeltaL2 Delta;
        Delta.Dir.Body   = Aggregated.Dir.Body   - CurrentDir.Body;
        Delta.Dir.Mind   = Aggregated.Dir.Mind   - CurrentDir.Mind;
        Delta.Dir.Spirit = Aggregated.Dir.Spirit - CurrentDir.Spirit;
        Delta.Dir.Nature = Aggregated.Dir.Nature - CurrentDir.Nature;

        Delta.Magnitude = Aggregated.Magnitude - CurrentBiomeState.Magnitude;
        Delta.Meta.Distortion = Aggregated.Meta.Distortion - CurrentBiomeState.Meta.Distortion;
        Delta.Meta.Stability  = Aggregated.Meta.Stability  - CurrentBiomeState.Meta.Stability;
        Delta.Meta.Purity     = Aggregated.Meta.Purity     - CurrentBiomeState.Meta.Purity;
        Delta.Meta.Potency    = Aggregated.Meta.Potency    - CurrentBiomeState.Meta.Potency;
        Delta.Meta.Resonance  = Aggregated.Meta.Resonance  - CurrentBiomeState.Meta.Resonance;
        Delta.Meta.Corruption = Aggregated.Meta.Corruption - CurrentBiomeState.Meta.Corruption;

        return Delta;
    }

    // =========================================================================
    // L1: ApplyPotencyResonanceCorruption
    // =========================================================================
    void Pipeline::ApplyPotencyResonanceCorruption(FRealState& InOutDelta, const FMeta& Meta)
    {
        float PotencyScale = 1.0f + (Meta.Potency - 0.5f) * 0.5f;
        InOutDelta.Magnitude *= PotencyScale;
        InOutDelta.Meta.Distortion *= PotencyScale;
        InOutDelta.Meta.Stability *= PotencyScale;
        InOutDelta.Meta.Purity *= PotencyScale;
        InOutDelta.Meta.Potency *= PotencyScale;
        InOutDelta.Meta.Resonance *= PotencyScale;
        InOutDelta.Meta.Corruption *= PotencyScale;

        float ResonanceFactor = 1.0f + Meta.Resonance * 0.5f;
        InOutDelta.Direction.Body = InOutDelta.Direction.Body > 0 ? InOutDelta.Direction.Body * ResonanceFactor : InOutDelta.Direction.Body;
        InOutDelta.Direction.Mind = InOutDelta.Direction.Mind > 0 ? InOutDelta.Direction.Mind * ResonanceFactor : InOutDelta.Direction.Mind;
        InOutDelta.Direction.Spirit = InOutDelta.Direction.Spirit > 0 ? InOutDelta.Direction.Spirit * ResonanceFactor : InOutDelta.Direction.Spirit;
        InOutDelta.Direction.Nature = InOutDelta.Direction.Nature > 0 ? InOutDelta.Direction.Nature * ResonanceFactor : InOutDelta.Direction.Nature;

        InOutDelta.Meta.Distortion += Meta.Corruption * 0.1f;
        InOutDelta.Meta.Stability -= Meta.Corruption * 0.05f;
        InOutDelta.Meta.Purity -= Meta.Corruption * 0.05f;
        InOutDelta.Meta.Corruption += Meta.Corruption * 0.05f;

        UE_LOG(LogHerbalist, Verbose, TEXT("[POTENCY L1] Scale=%.3f | [RESONANCE] Factor=%.3f | [CORRUPTION] AddDist=%.3f"),
            PotencyScale, ResonanceFactor, Meta.Corruption * 0.1f);
    }

    // =========================================================================
    // L1: FinalizeState
    // =========================================================================
    FRealState Pipeline::FinalizeState(const FRealState& CurrentBiomeState, const FRealState& Delta)
    {
        FRealState NewState = CurrentBiomeState;

        FDirection NewDir;
        NewDir.Body = NewState.Direction.Body + Delta.Direction.Body;
        NewDir.Mind = NewState.Direction.Mind + Delta.Direction.Mind;
        NewDir.Spirit = NewState.Direction.Spirit + Delta.Direction.Spirit;
        NewDir.Nature = NewState.Direction.Nature + Delta.Direction.Nature;
        NewDir.NormalizeSum();

        float InterpAlpha = FMath::Clamp(Delta.Magnitude * 2.0f, 0.0f, 1.0f);
        NewState.Direction.Body = FMath::Lerp(NewState.Direction.Body, NewDir.Body, InterpAlpha);
        NewState.Direction.Mind = FMath::Lerp(NewState.Direction.Mind, NewDir.Mind, InterpAlpha);
        NewState.Direction.Spirit = FMath::Lerp(NewState.Direction.Spirit, NewDir.Spirit, InterpAlpha);
        NewState.Direction.Nature = FMath::Lerp(NewState.Direction.Nature, NewDir.Nature, InterpAlpha);
        NewState.Direction.NormalizeSum();

        NewState.Magnitude += Delta.Magnitude;
        NewState.Meta.Distortion += Delta.Meta.Distortion;
        NewState.Meta.Stability += Delta.Meta.Stability;
        NewState.Meta.Purity += Delta.Meta.Purity;
        NewState.Meta.Potency += Delta.Meta.Potency;
        NewState.Meta.Resonance += Delta.Meta.Resonance;
        NewState.Meta.Corruption += Delta.Meta.Corruption;

        NewState.Direction.NormalizeSum();

        NewState.Magnitude = FMath::Clamp(NewState.Magnitude, 0.0f, 1.0f);
        NewState.Meta.Distortion = FMath::Clamp(NewState.Meta.Distortion, 0.0f, 1.0f);
        NewState.Meta.Stability = FMath::Clamp(NewState.Meta.Stability, 0.0f, 1.0f);
        NewState.Meta.Purity = FMath::Clamp(NewState.Meta.Purity, 0.0f, 1.0f);
        NewState.Meta.Potency = FMath::Clamp(NewState.Meta.Potency, 0.0f, 1.0f);
        NewState.Meta.Resonance = FMath::Clamp(NewState.Meta.Resonance, 0.0f, 1.0f);
        NewState.Meta.Corruption = FMath::Clamp(NewState.Meta.Corruption, 0.0f, 1.0f);

        float Integrity = (1.0f - NewState.Meta.Distortion * 0.7f) * (1.0f + NewState.Meta.Stability);
        float StructureFactor = FMath::Clamp(Integrity, 0.0f, 1.0f);
        NewState.Magnitude *= StructureFactor;

        UE_LOG(LogHerbalist, Verbose, TEXT("[NEW_STATE L1] Mag: %.3f | Dir: (%.2f, %.2f, %.2f, %.2f) | Dist:%.3f Stab:%.3f Pur:%.3f Pot:%.3f Res:%.3f Cor:%.3f"),
            NewState.Magnitude,
            NewState.Direction.Body, NewState.Direction.Mind,
            NewState.Direction.Spirit, NewState.Direction.Nature,
            NewState.Meta.Distortion, NewState.Meta.Stability, NewState.Meta.Purity,
            NewState.Meta.Potency, NewState.Meta.Resonance, NewState.Meta.Corruption);

        return NewState;
    }

    // =========================================================================
    // L1: ApplyMorok (старая версия)
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
            UE_LOG(LogHerbalist, Log, TEXT("ALCHEMY_RESULT|Pipeline=L1|WaterOnly|Mag=%.3f|Body=%.2f|Mind=%.2f|Spirit=%.2f|Nature=%.2f|Dist=%.3f|Stab=%.3f|Pur=%.3f|Pot=%.3f|Res=%.3f|Cor=%.3f"),
                Result.Magnitude, Result.Direction.Body, Result.Direction.Mind, Result.Direction.Spirit, Result.Direction.Nature,
                Result.Meta.Distortion, Result.Meta.Stability, Result.Meta.Purity, Result.Meta.Potency, Result.Meta.Resonance, Result.Meta.Corruption);
            return Result;
        }
        if (Water.Num() == 0)
        {
            FRealState Result = ProcessNoWater();
            UE_LOG(LogHerbalist, Log, TEXT("ALCHEMY_RESULT|Pipeline=L1|Ash|Mag=%.3f|Body=%.2f|Mind=%.2f|Spirit=%.2f|Nature=%.2f|Dist=%.3f|Stab=%.3f|Pur=%.3f|Pot=%.3f|Res=%.3f|Cor=%.3f"),
                Result.Magnitude, Result.Direction.Body, Result.Direction.Mind, Result.Direction.Spirit, Result.Direction.Nature,
                Result.Meta.Distortion, Result.Meta.Stability, Result.Meta.Purity, Result.Meta.Potency, Result.Meta.Resonance, Result.Meta.Corruption);
            return Result;
        }

        // 3. Агрегация не-воды и воды
        FRealState NonWaterAggregated = Fold(NonWater);
        FRealState WaterAggregated = AggregateWater(Water);

        // 4. Смешивание воды и не-воды
        FRealState Aggregated = BlendWaterAndNonWater(NonWaterAggregated, WaterAggregated, NonWater.Num(), Water.Num());

        UE_LOG(LogHerbalist, Verbose, TEXT("[FOLD L1] Mag: %.3f | Dir: (%.2f, %.2f, %.2f, %.2f) | Dist:%.3f Stab:%.3f Pur:%.3f Pot:%.3f Res:%.3f Cor:%.3f"),
            Aggregated.Magnitude,
            Aggregated.Direction.Body, Aggregated.Direction.Mind,
            Aggregated.Direction.Spirit, Aggregated.Direction.Nature,
            Aggregated.Meta.Distortion, Aggregated.Meta.Stability, Aggregated.Meta.Purity,
            Aggregated.Meta.Potency, Aggregated.Meta.Resonance, Aggregated.Meta.Corruption);

        // 5. Дельта
        FRealState Delta = ComputeDelta(Aggregated, CurrentBiomeState);
        UE_LOG(LogHerbalist, Verbose, TEXT("[DELTA L1] Mag: %.3f | Dir: (%.2f, %.2f, %.2f, %.2f) | Dist:%.3f Stab:%.3f Pur:%.3f Pot:%.3f Res:%.3f Cor:%.3f"),
            Delta.Magnitude,
            Delta.Direction.Body, Delta.Direction.Mind,
            Delta.Direction.Spirit, Delta.Direction.Nature,
            Delta.Meta.Distortion, Delta.Meta.Stability, Delta.Meta.Purity,
            Delta.Meta.Potency, Delta.Meta.Resonance, Delta.Meta.Corruption);

        // 6. Базовый Distortion и ZaryanaStrength
        float Distortion = ComputeBaseDistortion(Env, Memory);
        float ZaryanaStrength = Intent.Coherence * (1.0f - Distortion);

        // 7. Внедрение контекста биома
        ApplyBiomeContext(Distortion, ZaryanaStrength, Delta, BiomeMorokField, BiomeZaryanaField, BiomeAxisDrift);

        // 8. Влияние Intent на дельту
        float IntentFactor = 0.5f + Intent.Coherence;
        Delta.Magnitude *= IntentFactor;
        Delta.Meta.Stability *= IntentFactor;
        Delta.Meta.Purity *= IntentFactor;
        UE_LOG(LogHerbalist, Verbose, TEXT("[INTENT L1] Factor=%.3f"), IntentFactor);

        // 9. Модификаторы от мета-параметров
        ApplyPotencyResonanceCorruption(Delta, Aggregated.Meta);

        // 10. Morok (нелинейное искажение)
        ApplyMorokDistortion(Delta, Distortion, Rng);

        // 11. Zaryana (структурирование)
        ApplyZaryanaStructuring(Delta, ZaryanaStrength, Distortion);

        // 12. Финальное состояние
        FRealState NewState = FinalizeState(CurrentBiomeState, Delta);
        UE_LOG(LogHerbalist, Log, TEXT("ALCHEMY_RESULT|Pipeline=L1|Mag=%.3f|Body=%.2f|Mind=%.2f|Spirit=%.2f|Nature=%.2f|Dist=%.3f|Stab=%.3f|Pur=%.3f|Pot=%.3f|Res=%.3f|Cor=%.3f"),
            NewState.Magnitude,
            NewState.Direction.Body, NewState.Direction.Mind,
            NewState.Direction.Spirit, NewState.Direction.Nature,
            NewState.Meta.Distortion, NewState.Meta.Stability, NewState.Meta.Purity,
            NewState.Meta.Potency, NewState.Meta.Resonance, NewState.Meta.Corruption);
        return NewState;
    }

    // =========================================================================
    // L2: ApplyMorokL2 (новая версия)
    // =========================================================================
    FRealState Pipeline::ApplyMorokL2(
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
            UE_LOG(LogHerbalist, Log, TEXT("ALCHEMY_RESULT|Pipeline=L2|WaterOnly|Mag=%.3f|Body=%.2f|Mind=%.2f|Spirit=%.2f|Nature=%.2f|Dist=%.3f|Stab=%.3f|Pur=%.3f|Pot=%.3f|Res=%.3f|Cor=%.3f"),
                Result.Magnitude, Result.Direction.Body, Result.Direction.Mind, Result.Direction.Spirit, Result.Direction.Nature,
                Result.Meta.Distortion, Result.Meta.Stability, Result.Meta.Purity, Result.Meta.Potency, Result.Meta.Resonance, Result.Meta.Corruption);
            return Result;
        }
        if (Water.Num() == 0)
        {
            FRealState Result = ProcessNoWater();
            UE_LOG(LogHerbalist, Log, TEXT("ALCHEMY_RESULT|Pipeline=L2|Ash|Mag=%.3f|Body=%.2f|Mind=%.2f|Spirit=%.2f|Nature=%.2f|Dist=%.3f|Stab=%.3f|Pur=%.3f|Pot=%.3f|Res=%.3f|Cor=%.3f"),
                Result.Magnitude, Result.Direction.Body, Result.Direction.Mind, Result.Direction.Spirit, Result.Direction.Nature,
                Result.Meta.Distortion, Result.Meta.Stability, Result.Meta.Purity, Result.Meta.Potency, Result.Meta.Resonance, Result.Meta.Corruption);
            return Result;
        }

        // 3. Агрегация не-воды в L2
        FAggregatedL2 NonWaterAggL2 = FoldL2(NonWater, Rng);

        // 4. Агрегация воды в L1 (вода пока остаётся в L1)
        FRealState WaterAggregated_L1 = AggregateWater(Water);

        // 5. Смешивание воды и не-воды в L1 (разбавление)
        FRealState NonWaterAgg_L1;
        NonWaterAgg_L1.Direction = NonWaterAggL2.Dir.ToL1();
        NonWaterAgg_L1.Magnitude = NonWaterAggL2.Magnitude;
        NonWaterAgg_L1.Meta = NonWaterAggL2.Meta;

        FRealState Aggregated_L1 = BlendWaterAndNonWater(NonWaterAgg_L1, WaterAggregated_L1, NonWater.Num(), Water.Num());

        // 6. Переход в L2 для основной математики
        FL2Direction AggregatedDir = ToL2(Aggregated_L1.Direction, Rng);
        float AggregatedMag = Aggregated_L1.Magnitude;
        FMeta AggregatedMeta = Aggregated_L1.Meta;

        FL2Direction CurrentDir = ToL2(CurrentBiomeState.Direction, Rng);

        // 7. Вычисление Delta в L2
        FL2Direction DeltaDir;
        DeltaDir.Body   = AggregatedDir.Body   - CurrentDir.Body;
        DeltaDir.Mind   = AggregatedDir.Mind   - CurrentDir.Mind;
        DeltaDir.Spirit = AggregatedDir.Spirit - CurrentDir.Spirit;
        DeltaDir.Nature = AggregatedDir.Nature - CurrentDir.Nature;

        float MagnitudeDelta = AggregatedMag - CurrentBiomeState.Magnitude;

        // 8. Применение Potency, Resonance, Corruption (линейное масштабирование)
        float PotencyScale = 1.0f + (AggregatedMeta.Potency - 0.5f) * 0.5f;
        DeltaDir.Body   *= PotencyScale;
        DeltaDir.Mind   *= PotencyScale;
        DeltaDir.Spirit *= PotencyScale;
        DeltaDir.Nature *= PotencyScale;
        MagnitudeDelta *= PotencyScale;

        // 9. Базовый Distortion и ZaryanaStrength
        float Distortion = ComputeBaseDistortion(Env, Memory);
        float ZaryanaStrength = Intent.Coherence * (1.0f - Distortion);

        // 10. Внедрение контекста биома
        HerbalistCore::ApplyBiomeContext(Distortion, ZaryanaStrength, DeltaDir, MagnitudeDelta,
            BiomeMorokField, BiomeZaryanaField, BiomeAxisDrift);

        // 11. Влияние Intent
        float IntentFactor = 0.5f + Intent.Coherence;
        DeltaDir.Body   *= IntentFactor;
        DeltaDir.Mind   *= IntentFactor;
        DeltaDir.Spirit *= IntentFactor;
        DeltaDir.Nature *= IntentFactor;
        MagnitudeDelta *= IntentFactor;

        // 12. Morok (матричное искажение)
        ApplyMorokMatrix(DeltaDir, Distortion, Rng);

        // 13. Zaryana (структурирование)
        HerbalistCore::ApplyZaryanaStructuring(DeltaDir, ZaryanaStrength, Distortion, Rng);

        // 14. Применение Delta к текущему состоянию в L2
        FL2Direction NewDir;
        NewDir.Body   = CurrentDir.Body   + DeltaDir.Body;
        NewDir.Mind   = CurrentDir.Mind   + DeltaDir.Mind;
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
        NewState.Meta.Stability  = FMath::Clamp(NewState.Meta.Stability,  0.0f, 1.0f);
        NewState.Meta.Purity     = FMath::Clamp(NewState.Meta.Purity,     0.0f, 1.0f);
        NewState.Meta.Potency    = FMath::Clamp(NewState.Meta.Potency,    0.0f, 1.0f);
        NewState.Meta.Resonance  = FMath::Clamp(NewState.Meta.Resonance,  0.0f, 1.0f);
        NewState.Meta.Corruption = FMath::Clamp(NewState.Meta.Corruption, 0.0f, 1.0f);

        UE_LOG(LogHerbalist, Log, TEXT("ALCHEMY_RESULT|Pipeline=L2|Mag=%.3f|Body=%.2f|Mind=%.2f|Spirit=%.2f|Nature=%.2f|Dist=%.3f|Stab=%.3f|Pur=%.3f|Pot=%.3f|Res=%.3f|Cor=%.3f"),
            NewState.Magnitude,
            NewState.Direction.Body, NewState.Direction.Mind,
            NewState.Direction.Spirit, NewState.Direction.Nature,
            NewState.Meta.Distortion, NewState.Meta.Stability, NewState.Meta.Purity,
            NewState.Meta.Potency, NewState.Meta.Resonance, NewState.Meta.Corruption);

        return NewState;
    }

} // namespace HerbalistCore