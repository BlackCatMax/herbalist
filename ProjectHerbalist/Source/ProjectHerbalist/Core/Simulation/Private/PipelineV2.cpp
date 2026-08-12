// Core/Simulation/Private/PipelineV2.cpp
#include "PipelineV2.h"
#include "ProjectHerbalist.h"
#include "Core/Types/BiomeTypes.h"
#include "Core/HerbalistSettings.h"

namespace Simulation
{
    // ---------------------------------------------------------
    // Вспомогательные функции
    // ---------------------------------------------------------

    static const FInventoryItem* FindItemInSnapshot(const FInventorySnapshot& InvSnap,
                                                    int32 ContainerID,
                                                    FName IngredientID)
    {
        const TArray<FInventoryItem>* Items = InvSnap.ContainerContents.Find(ContainerID);
        if (!Items) return nullptr;

        for (const FInventoryItem& Item : *Items)
        {
            if (Item.IngredientID == IngredientID)
                return &Item;
        }
        return nullptr;
    }

    // Результат сбора = базовые параметры ингредиента, смещённые отклонением
    // текущего состояния клетки от S0 (Алатыря) — та же математика, что в
    // UHarvestService::Harvest, но как чистая функция снапшота (без обращения
    // к UIngredientRegistrySubsystem/UHerbalistSettings через World/Subsystem;
    // BaseState уже резолвлен вне Pipeline, см. FHarvestCommand::BaseState).
    static FInventoryItem GenerateHarvestResult(const FGridCell& Cell,
                                               FName IngredientID,
                                               const FRealState& IngredientBaseState,
                                               FRandomStream& Rng)
    {
        const UHerbalistSettings* Settings = GetHerbalistSettings();
        const float k_biome = Settings ? Settings->HarvestBiomeWeight : 0.6f;

        const FRealState& S0 = FAlatyr::S0;
        const FRealState& BiomeState = Cell.State;

        // Если базовое состояние ингредиента не было резолвлено (нулевое) —
        // деградируем к прежнему поведению: биом как единственный источник.
        const bool bHasBase = IngredientBaseState.Magnitude > KINDA_SMALL_NUMBER || IngredientBaseState.Meta.Distortion > KINDA_SMALL_NUMBER;
        const FRealState& Base = bHasBase ? IngredientBaseState : BiomeState;

        FRealState BiomeDelta;
        BiomeDelta.Direction.Body = BiomeState.Direction.Body - S0.Direction.Body;
        BiomeDelta.Direction.Mind = BiomeState.Direction.Mind - S0.Direction.Mind;
        BiomeDelta.Direction.Spirit = BiomeState.Direction.Spirit - S0.Direction.Spirit;
        BiomeDelta.Direction.Nature = BiomeState.Direction.Nature - S0.Direction.Nature;
        BiomeDelta.Magnitude = BiomeState.Magnitude - S0.Magnitude;
        BiomeDelta.Meta.Distortion = BiomeState.Meta.Distortion - S0.Meta.Distortion;
        BiomeDelta.Meta.Stability = BiomeState.Meta.Stability - S0.Meta.Stability;
        BiomeDelta.Meta.Purity = BiomeState.Meta.Purity - S0.Meta.Purity;
        BiomeDelta.Meta.Potency = BiomeState.Meta.Potency - S0.Meta.Potency;
        BiomeDelta.Meta.Resonance = BiomeState.Meta.Resonance - S0.Meta.Resonance;
        BiomeDelta.Meta.Corruption = BiomeState.Meta.Corruption - S0.Meta.Corruption;

        FRealState State;
        State.Direction.Body = Base.Direction.Body + k_biome * BiomeDelta.Direction.Body;
        State.Direction.Mind = Base.Direction.Mind + k_biome * BiomeDelta.Direction.Mind;
        State.Direction.Spirit = Base.Direction.Spirit + k_biome * BiomeDelta.Direction.Spirit;
        State.Direction.Nature = Base.Direction.Nature + k_biome * BiomeDelta.Direction.Nature;
        State.Magnitude = Base.Magnitude + k_biome * BiomeDelta.Magnitude;
        State.Meta.Stability = Base.Meta.Stability + k_biome * BiomeDelta.Meta.Stability;
        State.Meta.Purity = Base.Meta.Purity + k_biome * BiomeDelta.Meta.Purity;
        State.Meta.Potency = Base.Meta.Potency + k_biome * BiomeDelta.Meta.Potency;
        State.Meta.Resonance = Base.Meta.Resonance + k_biome * BiomeDelta.Meta.Resonance;
        State.Meta.Corruption = Base.Meta.Corruption + k_biome * BiomeDelta.Meta.Corruption;

        const float P_Base = 1.f - Base.Meta.Distortion;
        const float P_Biome = 1.f - BiomeState.Meta.Distortion * k_biome;
        State.Meta.Distortion = 1.f - FMath::Clamp(P_Base * P_Biome, 0.f, 1.f);

        // Небольшой джиттер поверх — условия сбора (FConditionModifier), которые
        // здесь не передаются игроком явно, приближаем случайным шумом.
        State.Magnitude = FMath::Clamp(State.Magnitude + Rng.FRandRange(-0.03f, 0.03f), 0.0f, 1.0f);

        State.Direction.NormalizeSum();
        State.Meta.Distortion = FMath::Clamp(State.Meta.Distortion, 0.f, 1.f);
        State.Meta.Stability = FMath::Clamp(State.Meta.Stability, 0.f, 1.f);
        State.Meta.Purity = FMath::Clamp(State.Meta.Purity, 0.f, 1.f);
        State.Meta.Potency = FMath::Clamp(State.Meta.Potency, 0.f, 1.f);
        State.Meta.Resonance = FMath::Clamp(State.Meta.Resonance, 0.f, 1.f);
        State.Meta.Corruption = FMath::Clamp(State.Meta.Corruption, 0.f, 1.f);

        FInventoryItem Result;
        Result.IngredientID = IngredientID;
        Result.State = State;
        Result.Count = 1;
        Result.CreationTime = 0.0;
        Result.bSubjectToDecay = true;
        return Result;
    }

    // ---------------------------------------------------------
    // Взвешенное накопление FRealState (используется и для Fold ингредиентов,
    // и для усреднения воды)
    // ---------------------------------------------------------
    static void AccumulateWeighted(FRealState& Agg, const FRealState& S, float Weight)
    {
        Agg.Magnitude += S.Magnitude * Weight;
        Agg.Direction.Body += S.Direction.Body * Weight;
        Agg.Direction.Mind += S.Direction.Mind * Weight;
        Agg.Direction.Spirit += S.Direction.Spirit * Weight;
        Agg.Direction.Nature += S.Direction.Nature * Weight;
        Agg.Meta.Distortion += S.Meta.Distortion * Weight;
        Agg.Meta.Stability += S.Meta.Stability * Weight;
        Agg.Meta.Purity += S.Meta.Purity * Weight;
        Agg.Meta.Potency += S.Meta.Potency * Weight;
        Agg.Meta.Resonance += S.Meta.Resonance * Weight;
        Agg.Meta.Corruption += S.Meta.Corruption * Weight;
    }

    static void DivideRealState(FRealState& Agg, float Divisor)
    {
        if (Divisor <= KINDA_SMALL_NUMBER) return;
        Agg.Magnitude /= Divisor;
        Agg.Direction.Body /= Divisor;
        Agg.Direction.Mind /= Divisor;
        Agg.Direction.Spirit /= Divisor;
        Agg.Direction.Nature /= Divisor;
        Agg.Meta.Distortion /= Divisor;
        Agg.Meta.Stability /= Divisor;
        Agg.Meta.Purity /= Divisor;
        Agg.Meta.Potency /= Divisor;
        Agg.Meta.Resonance /= Divisor;
        Agg.Meta.Corruption /= Divisor;
    }

    // ---------------------------------------------------------
    // Пайплайн варки зелья — 9 шагов из 05_Systems.md:
    // 1-2. Сбор параметров + Агрегация (Fold, с затуханием по порядку)
    // 3. Biome Context Injection (MorokField/ZaryanaField/Affinity/AxisDrift)
    // 4. Применение воды (обязательность, "только вода", разбавление, штраф >80%)
    // 5. Нормализация осей
    // 6. Morok (нелинейное искажение + обмен осями)
    // 7. Zaryana (усиление доминирующей оси, стабильность/чистота)
    // 8. Bifurcation (Collapse/Purification)
    // ---------------------------------------------------------
    static FRealState ComputeApplyResult(const TArray<FInventoryItem>& Ingredients,
                                        const FIntent& Intent,
                                        const FBiomeFieldContext* BiomeCtx,
                                        float CollapseThreshold,
                                        FRandomStream& Rng,
                                        EAlchemyOutcome& OutOutcome,
                                        FVector4& OutAxisDeltaForFootprint)
    {
        OutOutcome = EAlchemyOutcome::Valid;
        OutAxisDeltaForFootprint = FVector4(0.f, 0.f, 0.f, 0.f);

        if (Ingredients.Num() == 0) return FRealState();

        const UHerbalistSettings* Settings = GetHerbalistSettings();

        // --- 1-2. Fold: ингредиенты и вода агрегируются раздельно; у обычных
        // ингредиентов вес затухает с позицией (первый — максимальный вклад) ---
        const float OrderDecay = Settings ? Settings->FoldWeightDecay : 0.8f;

        FRealState NonWaterAgg;
        float NonWaterWeight = 0.f;
        int32 NonWaterOrderIndex = 0;
        int32 NonWaterCount = 0;

        FRealState WaterAgg;
        float WaterWeightSum = 0.f;
        int32 WaterCount = 0;

        for (const FInventoryItem& Item : Ingredients)
        {
            if (Item.bIsWater)
            {
                const float Weight = static_cast<float>(Item.Count);
                AccumulateWeighted(WaterAgg, Item.State, Weight);
                WaterWeightSum += Weight;
                WaterCount += Item.Count;
            }
            else
            {
                const float Weight = Item.State.Magnitude * Item.Count * FMath::Pow(OrderDecay, static_cast<float>(NonWaterOrderIndex));
                AccumulateWeighted(NonWaterAgg, Item.State, Weight);
                NonWaterWeight += Weight;
                NonWaterCount += Item.Count;
                ++NonWaterOrderIndex;
            }
        }

        DivideRealState(NonWaterAgg, NonWaterWeight);
        DivideRealState(WaterAgg, WaterWeightSum);

        // --- 4a. Обязательность воды: ингредиенты без единой капли воды дают золу ---
        if (NonWaterCount > 0 && WaterCount == 0)
        {
            OutOutcome = EAlchemyOutcome::Ash;
            FRealState Ash;
            Ash.Magnitude = 0.05f;
            Ash.Meta.Distortion = 0.9f;
            Ash.Meta.Corruption = 0.8f;
            Ash.Meta.Purity = 0.05f;
            Ash.Meta.Stability = 0.1f;
            Ash.Direction = NonWaterAgg.Direction;
            Ash.Direction.NormalizeSum();
            return Ash;
        }

        // --- 4b. Только вода — варёная вода: высокая Purity, нулевой Distortion ---
        if (NonWaterCount == 0 && WaterCount > 0)
        {
            OutOutcome = EAlchemyOutcome::BoiledWater;
            FRealState Boiled = WaterAgg;
            Boiled.Meta.Purity = FMath::Clamp(Boiled.Meta.Purity + 0.3f, 0.f, 1.f);
            Boiled.Meta.Distortion = 0.f;
            Boiled.Magnitude = FMath::Min(Boiled.Magnitude, 0.2f);
            Boiled.Direction.NormalizeSum();
            return Boiled;
        }

        // --- Смесь: Fold ингредиентов + разбавление водой ---
        FRealState Result = NonWaterAgg;

        const int32 TotalCount = NonWaterCount + WaterCount;
        const float WaterFraction = TotalCount > 0 ? static_cast<float>(WaterCount) / static_cast<float>(TotalCount) : 0.f;

        // 4c. Разбавление: Magnitude снижается пропорционально доле воды
        Result.Magnitude = FMath::Clamp(Result.Magnitude * (1.f - WaterFraction), 0.f, 1.f);

        // 4d. Избыток воды: дополнительный штраф — "водянистое" зелье
        const float MaxWaterRatio = Settings ? Settings->MaxWaterRatio : 0.8f;
        const float WaterDilutionPenalty = Settings ? Settings->WaterDilutionPenalty : 0.2f;
        if (WaterFraction > MaxWaterRatio)
        {
            const float Excess = (WaterFraction - MaxWaterRatio) / FMath::Max(1.f - MaxWaterRatio, KINDA_SMALL_NUMBER);
            Result.Magnitude = FMath::Clamp(Result.Magnitude * (1.f - Excess * WaterDilutionPenalty), 0.f, 1.f);
        }

        // Вода как растворитель слегка подтягивает Purity к своей
        Result.Meta.Purity = FMath::Clamp(FMath::Lerp(Result.Meta.Purity, WaterAgg.Meta.Purity, WaterFraction * 0.5f), 0.f, 1.f);

        // --- 3. Biome Context Injection: сдвиг осей от Memory.AxisDrift,
        // эффективная сила Morok/Zaryana = поле узла * аффинити биома ---
        float EffectiveMorok = 0.f;
        float EffectiveZaryana = 0.f;
        if (BiomeCtx)
        {
            EffectiveMorok = FMath::Clamp(BiomeCtx->MorokField * BiomeCtx->MorokAffinity, 0.f, 1.f);
            EffectiveZaryana = FMath::Clamp(BiomeCtx->ZaryanaField * BiomeCtx->ZaryanaAffinity, 0.f, 1.f);

            const float DriftStrength = Settings ? Settings->BiomeAxisDriftWeight : 0.1f;
            Result.Direction.Body   = FMath::Max(0.f, Result.Direction.Body   + (BiomeCtx->AxisDrift.X - 0.25f) * DriftStrength);
            Result.Direction.Mind   = FMath::Max(0.f, Result.Direction.Mind   + (BiomeCtx->AxisDrift.Y - 0.25f) * DriftStrength);
            Result.Direction.Spirit = FMath::Max(0.f, Result.Direction.Spirit + (BiomeCtx->AxisDrift.Z - 0.25f) * DriftStrength);
            Result.Direction.Nature = FMath::Max(0.f, Result.Direction.Nature + (BiomeCtx->AxisDrift.W - 0.25f) * DriftStrength);
        }

        // --- 5. Нормализация осей ---
        Result.Direction.NormalizeSum();

        // --- 6. Morok: нелинейное искажение дельты + обмен осями ---
        const float Coherence = FMath::Clamp(Intent.Coherence, 0.f, 1.f);
        const float BiomeMorokInfluence = Settings ? Settings->BiomeMorokInfluence : 0.3f;
        const float MorokMixStrengthFactor = Settings ? Settings->MorokMixStrengthFactor : 0.5f;
        const float MorokPush = EffectiveMorok * BiomeMorokInfluence;
        Result.Meta.Distortion = FMath::Clamp(
            Result.Meta.Distortion * (1.f - Coherence) + Rng.FRandRange(0.f, 0.2f) * Coherence + MorokPush,
            0.f, 1.f);

        if (Rng.FRand() < EffectiveMorok * MorokMixStrengthFactor)
        {
            float* Axes[4] = { &Result.Direction.Body, &Result.Direction.Mind, &Result.Direction.Spirit, &Result.Direction.Nature };
            const int32 A = Rng.RandRange(0, 3);
            const int32 B = Rng.RandRange(0, 3);
            if (A != B)
            {
                Swap(*Axes[A], *Axes[B]);
            }
        }

        // --- 7. Zaryana: усиление доминирующей оси + стабильность/чистота.
        // ZaryanaBoostFactor — вклад согласованности (Coherence) в стабилизацию,
        // BiomeZaryanaInfluence — вклад поля биома, ZaryanaSuppressFactor — то,
        // насколько Заряна подавляет уже накопленное искажение. ---
        const float ZaryanaBoostFactor = Settings ? Settings->ZaryanaBoostFactor : 0.5f;
        const float BiomeZaryanaInfluence = Settings ? Settings->BiomeZaryanaInfluence : 0.3f;
        const float ZaryanaSuppressFactor = Settings ? Settings->ZaryanaSuppressFactor : 0.3f;

        Result.Meta.Stability = FMath::Clamp(Result.Meta.Stability * (1.f + Coherence * ZaryanaBoostFactor + EffectiveZaryana * BiomeZaryanaInfluence), 0.f, 1.f);
        Result.Meta.Purity = FMath::Clamp(Result.Meta.Purity + EffectiveZaryana * BiomeZaryanaInfluence * 0.5f, 0.f, 1.f);
        Result.Meta.Distortion = FMath::Clamp(Result.Meta.Distortion - EffectiveZaryana * ZaryanaSuppressFactor, 0.f, 1.f);

        if (EffectiveZaryana > KINDA_SMALL_NUMBER)
        {
            float* Axes[4] = { &Result.Direction.Body, &Result.Direction.Mind, &Result.Direction.Spirit, &Result.Direction.Nature };
            int32 Dominant = 0;
            for (int32 i = 1; i < 4; ++i)
            {
                if (*Axes[i] > *Axes[Dominant]) Dominant = i;
            }
            *Axes[Dominant] = FMath::Min(1.f, *Axes[Dominant] * (1.f + EffectiveZaryana * BiomeZaryanaInfluence));
        }

        Result.Direction.NormalizeSum();

        // --- 8. Bifurcation: при критическом Distortion — Collapse или Purification.
        // Чем выше текущая Stability, тем вероятнее очищение, а не схлопывание. ---
        if (Result.Meta.Distortion >= CollapseThreshold)
        {
            const bool bPurify = Rng.FRand() < Result.Meta.Stability;
            if (bPurify)
            {
                Result.Meta.Distortion = 0.4f;
                Result.Meta.Purity = FMath::Clamp(Result.Meta.Purity + 0.2f, 0.f, 1.f);
                Result.Meta.Stability = FMath::Clamp(Result.Meta.Stability + 0.2f, 0.f, 1.f);
                OutOutcome = EAlchemyOutcome::Valid;
            }
            else
            {
                Result.Meta.Distortion = 0.2f;
                Result.Meta.Stability = FMath::Clamp(Result.Meta.Stability - 0.3f, 0.f, 1.f);
                Result.Meta.Corruption = FMath::Clamp(Result.Meta.Corruption + 0.2f, 0.f, 1.f);
                OutOutcome = EAlchemyOutcome::Catastrophe;
            }
        }

        OutAxisDeltaForFootprint = FVector4(Result.Direction.Body, Result.Direction.Mind, Result.Direction.Spirit, Result.Direction.Nature)
            - FVector4(0.25f, 0.25f, 0.25f, 0.25f);

        return Result;
    }

    // ---------------------------------------------------------
    // Обработчики команд
    // ---------------------------------------------------------

    static void ProcessHarvestCommand(const FHarvestCommand& Cmd,
                                     const FWorldSnapshot& WorldSnap,
                                     FRandomStream& Rng,
                                     FStateDelta& OutDelta)
    {
        const FGridCell* Cell = WorldSnap.GridState.Find(Cmd.TargetCell);
        if (!Cell)
        {
            UE_LOG(LogHerbalist, Warning, TEXT("PipelineV2: Harvest cell (%d,%d) not found"), Cmd.TargetCell.X, Cmd.TargetCell.Y);
            return;
        }
        
        // ====================================================================
        // ВОДА: не деградирует – только добавляем предмет в инвентарь
        // ====================================================================
        if (Cell->bIsWater)
        {
            FInventoryItem WaterItem;
            WaterItem.IngredientID = Cell->WaterTypeID.IsNone() ? FName(TEXT("Water")) : Cell->WaterTypeID;
            WaterItem.State = Cell->State;                 // копируем состояние воды
            WaterItem.Count = 1;
            WaterItem.CreationTime = 0.0;
            WaterItem.bSubjectToDecay = true;
            WaterItem.bIsWater = true;

            FInventoryOperation Op;
            Op.ContainerID = 0;
            Op.Ingredient = WaterItem;
            Op.OpType = EInventoryOpType::Add;
            Op.Amount = 1;
            OutDelta.InventoryOps.Add(Op);

            // Не добавляем WorldChanges – клетка не меняется
            UE_LOG(LogHerbalist, Verbose, TEXT("Water harvested at (%d,%d)"), Cmd.TargetCell.X, Cmd.TargetCell.Y);
            return;
        }

        // ====================================================================
        // РАСТЕНИЯ: деградация (медленная)
        // ====================================================================
        const float DegradationStep = 0.002f;
        const float StressStep = 0.001f;
        
        FGridCell Modified = *Cell;
        Modified.HarvestStress = FMath::Clamp(Cell->HarvestStress + StressStep, 0.0f, 1.0f);
        Modified.State.Meta.Distortion = FMath::Clamp(Cell->State.Meta.Distortion + DegradationStep, 0.0f, 1.0f);
        Modified.State.Meta.Purity      = FMath::Clamp(Cell->State.Meta.Purity      - DegradationStep, 0.0f, 1.0f);
        Modified.State.Meta.Stability   = FMath::Clamp(Cell->State.Meta.Stability   - DegradationStep, 0.0f, 1.0f);
        Modified.State.Magnitude        = FMath::Clamp(Cell->State.Magnitude        - DegradationStep * 0.5f, 0.0f, 1.0f);
        
        Modified.State.Direction.Nature = FMath::Clamp(Cell->State.Direction.Nature - 0.001f, 0.0f, 1.0f);
        Modified.State.Direction.Body   = FMath::Clamp(Cell->State.Direction.Body   + 0.001f, 0.0f, 1.0f);
        Modified.State.Direction.NormalizeSum();
        Modified.Memory.AccumulatedDistortion = Modified.State.Meta.Distortion;
        
        FInventoryItem Harvested = GenerateHarvestResult(*Cell, Cmd.IngredientID, Cmd.BaseState, Rng);
        
        FInventoryOperation Op;
        Op.ContainerID = 0;
        Op.Ingredient = Harvested;
        Op.OpType = EInventoryOpType::Add;
        Op.Amount = Cmd.Amount;
        OutDelta.InventoryOps.Add(Op);
        OutDelta.WorldChanges.Add(Cmd.TargetCell, Modified);
        
        UE_LOG(LogHerbalist, Verbose, TEXT("Harvest: cell (%d,%d) Dist=%.3f"),
            Cmd.TargetCell.X, Cmd.TargetCell.Y, Modified.State.Meta.Distortion);
    }

    static void ProcessTransferCommand(const FTransferCommand& Cmd,
                                      const FInventorySnapshot& InvSnap,
                                      FStateDelta& OutDelta)
    {
        const FInventoryItem* SourceItem = FindItemInSnapshot(InvSnap, Cmd.SourceContainerID, Cmd.IngredientID);
        if (!SourceItem)
        {
            UE_LOG(LogHerbalist, Warning, TEXT("PipelineV2: Transfer source item %s not found in container %d"),
                *Cmd.IngredientID.ToString(), Cmd.SourceContainerID);
            return;
        }

        FInventoryOperation RemoveOp;
        RemoveOp.ContainerID = Cmd.SourceContainerID;
        RemoveOp.Ingredient = *SourceItem;
        RemoveOp.OpType = EInventoryOpType::Remove;
        RemoveOp.Amount = Cmd.Amount;
        OutDelta.InventoryOps.Add(RemoveOp);

        FInventoryOperation AddOp;
        AddOp.ContainerID = Cmd.TargetContainerID;
        AddOp.Ingredient = *SourceItem;
        AddOp.Ingredient.Count = Cmd.Amount;
        AddOp.OpType = EInventoryOpType::Add;
        AddOp.Amount = Cmd.Amount;
        OutDelta.InventoryOps.Add(AddOp);
    }

    static void ProcessApplyCommand(const FApplyCommand& Cmd,
                                   const FWorldSnapshot& WorldSnap,
                                   const FBiomeSnapshot& BiomeSnap,
                                   FRandomStream& Rng,
                                   FStateDelta& OutDelta)
    {
        // 0. Контекст биома клетки-цели. При крафте (вне мира, TargetCell = (-1,-1))
        // контекста нет — Biome Context Injection применяется только при варке
        // непосредственно в мире, как описано в 05_Systems.md/14_Biome_Graph.md.
        const FGridCell* TargetCell = Cmd.bIsCrafting ? nullptr : WorldSnap.GridState.Find(Cmd.TargetCell);
        const FBiomeFieldContext* BiomeCtx = nullptr;
        FName TargetBiomeID;
        if (TargetCell)
        {
            TargetBiomeID = FBiomeDefaults::BiomeTypeToName(TargetCell->Biome);
            BiomeCtx = BiomeSnap.Contexts.Find(TargetBiomeID);
        }

        // 1. Вычисляем результирующее состояние зелья
        EAlchemyOutcome Outcome = EAlchemyOutcome::Valid;
        FVector4 AxisDeltaForFootprint;
        FRealState PotionState = ComputeApplyResult(Cmd.Ingredients, Cmd.Intent, BiomeCtx, BiomeSnap.CollapseThreshold, Rng, Outcome, AxisDeltaForFootprint);

        // 2. Удаляем использованные ингредиенты из инвентаря
        for (const FInventoryItem& Ing : Cmd.Ingredients)
        {
            FInventoryOperation RemoveOp;
            RemoveOp.ContainerID = 0;
            RemoveOp.Ingredient = Ing;
            RemoveOp.Ingredient.Count = 1;
            RemoveOp.OpType = EInventoryOpType::Remove;
            RemoveOp.Amount = 1;
            OutDelta.InventoryOps.Add(RemoveOp);
        }

        // Footprint (14_Biome_Graph.md) — только при варке/применении непосредственно
        // в мире, привязанной к конкретному биому.
        if (TargetCell && !TargetBiomeID.IsNone())
        {
            FBiomeFootprintEntry Footprint;
            Footprint.BiomeID = TargetBiomeID;
            Footprint.MorokImpact = PotionState.Meta.Distortion;
            Footprint.ZaryanaImpact = 1.f - PotionState.Meta.Distortion;
            Footprint.AxisDelta = AxisDeltaForFootprint;
            OutDelta.Footprints.Add(Footprint);
        }

        // 3. Если крафт – создаём зелье в инвентаре и выходим
        if (Cmd.bIsCrafting)
        {
            FInventoryItem PotionItem;
            PotionItem.IngredientID = FName(TEXT("Potion"));
            PotionItem.State = PotionState;
            PotionItem.Count = 1;
            PotionItem.CreationTime = 0.0;
            PotionItem.bSubjectToDecay = false;

            FInventoryOperation AddOp;
            AddOp.ContainerID = 0;
            AddOp.Ingredient = PotionItem;
            AddOp.OpType = EInventoryOpType::Add;
            AddOp.Amount = 1;
            OutDelta.InventoryOps.Add(AddOp);

            UE_LOG(LogHerbalist, Log, TEXT("Crafted potion: Outcome=%d M=%.2f, Dist=%.2f, Purity=%.2f"),
                (int32)Outcome, PotionState.Magnitude, PotionState.Meta.Distortion, PotionState.Meta.Purity);
            return;
        }

        // 4. Иначе – применение на клетку
        if (!TargetCell)
        {
            UE_LOG(LogHerbalist, Warning, TEXT("Apply target cell (%d,%d) not found"), Cmd.TargetCell.X, Cmd.TargetCell.Y);
            return;
        }

        FGridCell Modified = *TargetCell;
        Modified.State = PotionState;
        Modified.HarvestStress = FMath::Clamp(TargetCell->HarvestStress + 0.2f, 0.f, 1.f);
        OutDelta.WorldChanges.Add(Cmd.TargetCell, Modified);

        UE_LOG(LogHerbalist, Log, TEXT("Applied potion to cell (%d,%d): Outcome=%d M=%.2f, Dist=%.2f"),
            Cmd.TargetCell.X, Cmd.TargetCell.Y, (int32)Outcome, PotionState.Magnitude, PotionState.Meta.Distortion);
    }

    // ---------------------------------------------------------
    // Главная точка входа
    // ---------------------------------------------------------
    FStateDelta ExecutePipeline(const FWorldSnapshot& WorldSnapshot,
                                const FInventorySnapshot& InventorySnapshot,
                                const FBiomeSnapshot& BiomeSnapshot,
                                const FCommandGraph& Commands,
                                FRandomStream& Rng)
    {
        FStateDelta Delta;

        for (const FCommandEntry& Entry : Commands.Commands)
        {
            if (Entry.bCancelled) continue;

            switch (Entry.Primitive)
            {
            case ECommandPrimitive::Harvest:
                ProcessHarvestCommand(Entry.Harvest, WorldSnapshot, Rng, Delta);
                break;
            case ECommandPrimitive::Transfer:
                ProcessTransferCommand(Entry.Transfer, InventorySnapshot, Delta);
                break;
            case ECommandPrimitive::Apply:
                ProcessApplyCommand(Entry.Apply, WorldSnapshot, BiomeSnapshot, Rng, Delta);
                break;
            default:
                break;
            }
        }

        return Delta;
    }
}