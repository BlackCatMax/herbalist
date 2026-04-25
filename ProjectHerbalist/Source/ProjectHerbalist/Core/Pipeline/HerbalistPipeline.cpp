// HerbalistPipeline.cpp
#include "HerbalistPipeline.h"
#include "PipelineTypes.h"
#include "ProjectHerbalist.h"
#include "Core/Types/HerbalistCoreTypes.h"
#include "Core/Inventory/HerbalistInventoryComponent.h"
#include "Core/HerbalistSettings.h"
#include "Math/UnrealMathUtility.h"
#include "Math/Vector4.h"
#include "PipelineMeta.h"

namespace HerbalistCore
{
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
        // 1. Разделение ингредиентов на воду и не-воду
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

        // ============================================================
        // ФАЗА 1: CoreMeta (только ингредиенты)
        // ============================================================
        FMeta CoreMeta = AggregatedL1.Meta;

        // ============================================================
        // ПРЕДВАРИТЕЛЬНЫЙ РАСЧЁТ ДЕЛЬТ ДЛЯ КОНТЕКСТА БИОМА
        // ============================================================
        FL2Direction AggregatedDir = ToL2(AggregatedL1.Direction, Rng);
        FL2Direction CurrentDir = ToL2(CurrentBiomeState.Direction, Rng);

        FL2Direction DeltaDir;
        DeltaDir.Body = AggregatedDir.Body - CurrentDir.Body;
        DeltaDir.Mind = AggregatedDir.Mind - CurrentDir.Mind;
        DeltaDir.Spirit = AggregatedDir.Spirit - CurrentDir.Spirit;
        DeltaDir.Nature = AggregatedDir.Nature - CurrentDir.Nature;

        float MagnitudeDelta = AggregatedL1.Magnitude - CurrentBiomeState.Magnitude;

        // ============================================================
        // ФАЗА 2: Контекст биома (изменяет дельты и Distortion/Zaryana)
        // ============================================================
        float Distortion = ComputeBaseDistortion(Env, Memory);
        float ZaryanaStrength = Intent.Coherence * (1.0f - Distortion);

        ApplyBiomeContext(Distortion, ZaryanaStrength, DeltaDir, MagnitudeDelta,
            BiomeMorokField, BiomeZaryanaField, BiomeAxisDrift);

        // ============================================================
        // ФАЗА 3: Morok Transform (насыщающая формула)
        // ============================================================
        const UHerbalistSettings* Settings = GetHerbalistSettings();
        float MorokFactor = Distortion * (Settings ? Settings->MorokMixStrengthFactor : 0.5f);
        float BaseDist = CoreMeta.Distortion;
        float Noise = MorokFactor * (1.0f - CoreMeta.Stability);

        FMeta MorokMeta = CoreMeta;
        MorokMeta.Distortion = BaseDist + Noise * (1.0f - BaseDist);
        MorokMeta.Distortion = FMath::Clamp(MorokMeta.Distortion, 0.0f, 1.0f);

        MorokMeta.Corruption = FMath::Clamp(
            CoreMeta.Corruption + MorokFactor * (1.0f - CoreMeta.Stability),
            0.0f, 1.0f);

        // ============================================================
        // ФАЗА 4: Environment Influence (мягкие оси)
        // ============================================================
        FMeta EnvApplied = MorokMeta;

        EnvApplied.Purity = FMath::Clamp(
            MorokMeta.Purity - Env.Toxicity * 0.3f,
            0.0f, 1.0f);

        float EnvStabilityBias = 1.0f - Env.Toxicity;
        EnvApplied.Stability = FMath::Clamp(
            FMath::Lerp(MorokMeta.Stability, EnvStabilityBias, 0.4f),
            0.0f, 1.0f);

        EnvApplied.Potency = FMath::Clamp(
            MorokMeta.Potency + (Env.Fertility * 0.1f + Env.Moisture * 0.05f),
            0.0f, 1.0f);

        // ============================================================
        // ФАЗА 5: Zaryana Projection (нормализация)
        // ============================================================
        FMeta Out = EnvApplied;

        Out.Distortion = FMath::Clamp(Out.Distortion * (1.0f - ZaryanaStrength * 0.25f), 0.0f, 1.0f);
        Out.Corruption = FMath::Clamp(Out.Corruption * (1.0f - ZaryanaStrength * 0.20f), 0.0f, 1.0f);
        Out.Stability = FMath::Lerp(Out.Stability, 1.0f, ZaryanaStrength * 0.15f);
        Out.Purity = FMath::Clamp(
            Out.Purity + ZaryanaStrength * (Out.Stability * 0.2f),
            0.0f, 1.0f);

        // ============================================================
        // ФАЗА 6: Direction и Magnitude
        // ============================================================
        ApplyMorokDistortion(DeltaDir, Distortion, Rng);
        ApplyZaryanaStructuring(DeltaDir, ZaryanaStrength, Distortion, Rng);

        FL2Direction EnvDirBias;
        EnvDirBias.Body = BiomeAxisDrift.X * 0.2f;
        EnvDirBias.Mind = BiomeAxisDrift.Y * 0.2f;
        EnvDirBias.Spirit = BiomeAxisDrift.Z * 0.2f;
        EnvDirBias.Nature = BiomeAxisDrift.W * 0.2f;

        FL2Direction NewDir;
        NewDir.Body = CurrentDir.Body + DeltaDir.Body + EnvDirBias.Body;
        NewDir.Mind = CurrentDir.Mind + DeltaDir.Mind + EnvDirBias.Mind;
        NewDir.Spirit = CurrentDir.Spirit + DeltaDir.Spirit + EnvDirBias.Spirit;
        NewDir.Nature = CurrentDir.Nature + DeltaDir.Nature + EnvDirBias.Nature;
        NewDir.NormalizeL2(Rng);

        float NewMagnitude = CurrentBiomeState.Magnitude + MagnitudeDelta;
        NewMagnitude = FMath::Clamp(NewMagnitude, 0.0f, 1.0f);

        // ============================================================
        // СБОРКА
        // ============================================================
        FRealState NewState;
        NewState.Direction = NewDir.ToL1();
        NewState.Magnitude = NewMagnitude;
        NewState.Meta = Out;

        NewState.Direction.NormalizeSum();
        NewState.Meta.Distortion = FMath::Clamp(NewState.Meta.Distortion, 0.0f, 1.0f);
        NewState.Meta.Stability   = FMath::Clamp(NewState.Meta.Stability,   0.0f, 1.0f);
        NewState.Meta.Purity      = FMath::Clamp(NewState.Meta.Purity,      0.0f, 1.0f);
        NewState.Meta.Potency     = FMath::Clamp(NewState.Meta.Potency,     0.0f, 1.0f);
        NewState.Meta.Resonance   = FMath::Clamp(NewState.Meta.Resonance,   0.0f, 1.0f);
        NewState.Meta.Corruption  = FMath::Clamp(NewState.Meta.Corruption,  0.0f, 1.0f);

        UE_LOG(LogHerbalist, Log, TEXT("ALCHEMY_RESULT|Mag=%.3f|Body=%.2f|Mind=%.2f|Spirit=%.2f|Nature=%.2f|Dist=%.3f|Stab=%.3f|Pur=%.3f|Pot=%.3f|Res=%.3f|Cor=%.3f"),
            NewState.Magnitude,
            NewState.Direction.Body, NewState.Direction.Mind,
            NewState.Direction.Spirit, NewState.Direction.Nature,
            NewState.Meta.Distortion, NewState.Meta.Stability, NewState.Meta.Purity,
            NewState.Meta.Potency, NewState.Meta.Resonance, NewState.Meta.Corruption);

        return NewState;
    }

} // namespace HerbalistCore
