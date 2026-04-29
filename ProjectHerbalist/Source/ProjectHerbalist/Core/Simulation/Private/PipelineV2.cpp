// Core/Simulation/Private/PipelineV2.cpp
#include "PipelineV2.h"
#include "ProjectHerbalist.h"
#include "Core/Subsystems/IngredientRegistrySubsystem.h"

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
            if (Item.IngredientID == IngredientID) return &Item;
        return nullptr;
    }

    static FInventoryItem GenerateHarvestResult(const FGridCell& Cell, FName IngredientID, FRandomStream& Rng)
    {
        FInventoryItem Result;
        Result.IngredientID = IngredientID;
        Result.State = Cell.State;
        Result.State.Magnitude = FMath::Clamp(Result.State.Magnitude + Rng.FRandRange(-0.05f, 0.05f), 0.0f, 1.0f);
        Result.State.Meta.Distortion = FMath::Clamp(Result.State.Meta.Distortion + Rng.FRandRange(-0.02f, 0.02f), 0.0f, 1.0f);
        Result.Count = 1;
        Result.CreationTime = 0.0;
        Result.bSubjectToDecay = true;
        return Result;
    }

    // Вспомогательная: случайное число 0..1 (детерминировано)
    static float Random01(FRandomStream& Rng)
    {
        return Rng.FRand();
    }

    // Нормализация L1
    static void NormalizeDirection(FDirection& Dir)
    {
        float Sum = Dir.Body + Dir.Mind + Dir.Spirit + Dir.Nature;
        if (Sum > KINDA_SMALL_NUMBER)
        {
            Dir.Body /= Sum;
            Dir.Mind /= Sum;
            Dir.Spirit /= Sum;
            Dir.Nature /= Sum;
        }
        else
        {
            Dir.Body = Dir.Mind = Dir.Spirit = Dir.Nature = 0.25f;
        }
    }

    // ---------------------------------------------------------
    // Расширенная алхимия (крафт)
    // ---------------------------------------------------------
    static FRealState ComputeAdvancedApplyResult(const TArray<FInventoryItem>& Ingredients,
                                                 const FIntent& Intent,
                                                 const FWorldSnapshot& WorldSnap,
                                                 const FIntPoint& TargetCell,
                                                 float BiomeMorokField,
                                                 float BiomeZaryanaField,
                                                 const FVector4& BiomeAxisDrift,
                                                 FRandomStream& Rng)
    {
        // 1. Разделение на воду и не-воду (упрощённо – по IngredientID)
        TArray<FRealState> WaterStates;
        TArray<FRealState> NonWaterStates;
        for (const FInventoryItem& Item : Ingredients)
        {
            bool bIsWater = (Item.IngredientID == FName(TEXT("Water")) || Item.IngredientID == FName(TEXT("BoiledWater")));
            if (bIsWater)
                WaterStates.Add(Item.State);
            else
                NonWaterStates.Add(Item.State);
        }

        // 2. Нет воды -> зола
        if (WaterStates.Num() == 0)
        {
            FRealState Ash;
            Ash.Magnitude = 0.1f;
            Ash.Meta.Distortion = 0.9f;
            Ash.Meta.Stability = 0.0f;
            Ash.Meta.Purity = 0.0f;
            Ash.Meta.Potency = 0.0f;
            Ash.Meta.Resonance = 0.0f;
            Ash.Meta.Corruption = 0.9f;
            Ash.Direction.Body = Ash.Direction.Mind = Ash.Direction.Spirit = Ash.Direction.Nature = 0.25f;
            return Ash;
        }

        // 3. Только вода -> варёная вода
        if (NonWaterStates.Num() == 0)
        {
            FRealState Avg;
            for (const FRealState& W : WaterStates)
            {
                Avg.Magnitude += W.Magnitude;
                Avg.Direction.Body += W.Direction.Body;
                Avg.Direction.Mind += W.Direction.Mind;
                Avg.Direction.Spirit += W.Direction.Spirit;
                Avg.Direction.Nature += W.Direction.Nature;
                Avg.Meta.Purity += W.Meta.Purity;
                Avg.Meta.Stability += W.Meta.Stability;
                Avg.Meta.Corruption += W.Meta.Corruption;
                Avg.Meta.Distortion += W.Meta.Distortion;
            }
            int32 C = WaterStates.Num();
            Avg.Magnitude /= C;
            Avg.Direction.Body /= C; Avg.Direction.Mind /= C; Avg.Direction.Spirit /= C; Avg.Direction.Nature /= C;
            Avg.Meta.Purity /= C; Avg.Meta.Stability /= C; Avg.Meta.Corruption /= C; Avg.Meta.Distortion /= C;

            Avg.Magnitude = FMath::Clamp(Avg.Magnitude * 0.8f, 0.f, 1.f);
            Avg.Meta.Purity = FMath::Clamp(Avg.Meta.Purity + 0.2f, 0.f, 1.f);
            Avg.Meta.Stability = FMath::Clamp(Avg.Meta.Stability + 0.1f, 0.f, 1.f);
            Avg.Meta.Distortion = FMath::Clamp(Avg.Meta.Distortion - 0.2f, 0.f, 1.f);
            Avg.Meta.Corruption = FMath::Clamp(Avg.Meta.Corruption - 0.1f, 0.f, 1.f);
            NormalizeDirection(Avg.Direction);
            return Avg;
        }

        // 4. Агрегация не-воды с затуханием веса
        const float WeightDecay = 0.8f;
        float CurrentWeight = 1.0f;
        float TotalNonWaterWeight = 0.0f;
        FRealState AggregatedNonWater;
        for (const FRealState& S : NonWaterStates)
        {
            float w = CurrentWeight;
            TotalNonWaterWeight += w;
            AggregatedNonWater.Magnitude += S.Magnitude * w;
            AggregatedNonWater.Direction.Body += S.Direction.Body * w;
            AggregatedNonWater.Direction.Mind += S.Direction.Mind * w;
            AggregatedNonWater.Direction.Spirit += S.Direction.Spirit * w;
            AggregatedNonWater.Direction.Nature += S.Direction.Nature * w;
            AggregatedNonWater.Meta.Distortion += S.Meta.Distortion * w;
            AggregatedNonWater.Meta.Stability += S.Meta.Stability * w;
            AggregatedNonWater.Meta.Purity += S.Meta.Purity * w;
            AggregatedNonWater.Meta.Potency += S.Meta.Potency * w;
            AggregatedNonWater.Meta.Resonance += S.Meta.Resonance * w;
            AggregatedNonWater.Meta.Corruption += S.Meta.Corruption * w;
            CurrentWeight *= WeightDecay;
        }
        if (TotalNonWaterWeight > KINDA_SMALL_NUMBER)
        {
            AggregatedNonWater.Magnitude /= TotalNonWaterWeight;
            AggregatedNonWater.Direction.Body /= TotalNonWaterWeight;
            AggregatedNonWater.Direction.Mind /= TotalNonWaterWeight;
            AggregatedNonWater.Direction.Spirit /= TotalNonWaterWeight;
            AggregatedNonWater.Direction.Nature /= TotalNonWaterWeight;
            AggregatedNonWater.Meta.Distortion /= TotalNonWaterWeight;
            AggregatedNonWater.Meta.Stability /= TotalNonWaterWeight;
            AggregatedNonWater.Meta.Purity /= TotalNonWaterWeight;
            AggregatedNonWater.Meta.Potency /= TotalNonWaterWeight;
            AggregatedNonWater.Meta.Resonance /= TotalNonWaterWeight;
            AggregatedNonWater.Meta.Corruption /= TotalNonWaterWeight;
        }
        NormalizeDirection(AggregatedNonWater.Direction);

        // 5. Агрегация воды (среднее)
        FRealState AggregatedWater;
        for (const FRealState& W : WaterStates)
        {
            AggregatedWater.Magnitude += W.Magnitude;
            AggregatedWater.Direction.Body += W.Direction.Body;
            AggregatedWater.Direction.Mind += W.Direction.Mind;
            AggregatedWater.Direction.Spirit += W.Direction.Spirit;
            AggregatedWater.Direction.Nature += W.Direction.Nature;
            AggregatedWater.Meta.Purity += W.Meta.Purity;
            AggregatedWater.Meta.Stability += W.Meta.Stability;
            AggregatedWater.Meta.Corruption += W.Meta.Corruption;
            AggregatedWater.Meta.Distortion += W.Meta.Distortion;
        }
        int32 WaterCount = WaterStates.Num();
        AggregatedWater.Magnitude /= WaterCount;
        AggregatedWater.Direction.Body /= WaterCount;
        AggregatedWater.Direction.Mind /= WaterCount;
        AggregatedWater.Direction.Spirit /= WaterCount;
        AggregatedWater.Direction.Nature /= WaterCount;
        AggregatedWater.Meta.Purity /= WaterCount;
        AggregatedWater.Meta.Stability /= WaterCount;
        AggregatedWater.Meta.Corruption /= WaterCount;
        AggregatedWater.Meta.Distortion /= WaterCount;
        NormalizeDirection(AggregatedWater.Direction);

        // 6. Смешивание воды и не-воды (разбавление)
        float NonWaterVolume = (float)NonWaterStates.Num();
        float WaterVolume = (float)WaterCount;
        float WaterRatio = WaterVolume / (NonWaterVolume + WaterVolume);
        const float MaxWaterRatio = 0.8f;
        const float WaterDilutionPenalty = 0.2f;
        float EffectiveWaterRatio = FMath::Min(WaterRatio, MaxWaterRatio);
        float DilutionPenalty = (WaterRatio > MaxWaterRatio) ? WaterDilutionPenalty : 1.0f;

        const float NonWaterW = 1.0f - EffectiveWaterRatio;
        const float WaterW = EffectiveWaterRatio;

        FRealState Blended;
        Blended.Magnitude = AggregatedNonWater.Magnitude * (1.0f - WaterRatio * 0.8f) * DilutionPenalty;
        Blended.Direction.Body = AggregatedNonWater.Direction.Body * NonWaterW + AggregatedWater.Direction.Body * WaterW;
        Blended.Direction.Mind = AggregatedNonWater.Direction.Mind * NonWaterW + AggregatedWater.Direction.Mind * WaterW;
        Blended.Direction.Spirit = AggregatedNonWater.Direction.Spirit * NonWaterW + AggregatedWater.Direction.Spirit * WaterW;
        Blended.Direction.Nature = AggregatedNonWater.Direction.Nature * NonWaterW + AggregatedWater.Direction.Nature * WaterW;
        NormalizeDirection(Blended.Direction);

        auto BlendMeta = [&](float a, float b) { return a * NonWaterW + b * WaterW; };
        Blended.Meta.Distortion = BlendMeta(AggregatedNonWater.Meta.Distortion, AggregatedWater.Meta.Distortion);
        Blended.Meta.Stability   = BlendMeta(AggregatedNonWater.Meta.Stability,   AggregatedWater.Meta.Stability);
        Blended.Meta.Purity      = BlendMeta(AggregatedNonWater.Meta.Purity,      AggregatedWater.Meta.Purity);
        Blended.Meta.Potency     = BlendMeta(AggregatedNonWater.Meta.Potency,     AggregatedWater.Meta.Potency);
        Blended.Meta.Resonance   = BlendMeta(AggregatedNonWater.Meta.Resonance,   AggregatedWater.Meta.Resonance);
        Blended.Meta.Corruption  = BlendMeta(AggregatedNonWater.Meta.Corruption,  AggregatedWater.Meta.Corruption);

        // 7. Coherence
        float Coherence = FMath::Clamp(Intent.Coherence, 0.f, 1.f);
        Blended.Meta.Distortion = Blended.Meta.Distortion * (1.f - Coherence) + Random01(Rng) * 0.2f * Coherence;
        Blended.Meta.Stability   = Blended.Meta.Stability   * (1.f + Coherence * 0.5f);
        Blended.Meta.Purity      = Blended.Meta.Purity      * (1.f + Coherence * 0.3f);

        // 8. Влияние контекста биома (поля графа)
        Blended.Meta.Distortion = FMath::Clamp(Blended.Meta.Distortion + BiomeMorokField * 0.1f, 0.f, 1.f);
        Blended.Meta.Stability  = FMath::Clamp(Blended.Meta.Stability  + BiomeZaryanaField * 0.05f, 0.f, 1.f);
        Blended.Meta.Purity     = FMath::Clamp(Blended.Meta.Purity     + BiomeZaryanaField * 0.05f, 0.f, 1.f);
        Blended.Direction.Body   = FMath::Clamp(Blended.Direction.Body   + BiomeAxisDrift.X * 0.1f, 0.f, 1.f);
        Blended.Direction.Mind   = FMath::Clamp(Blended.Direction.Mind   + BiomeAxisDrift.Y * 0.1f, 0.f, 1.f);
        Blended.Direction.Spirit = FMath::Clamp(Blended.Direction.Spirit + BiomeAxisDrift.Z * 0.1f, 0.f, 1.f);
        Blended.Direction.Nature = FMath::Clamp(Blended.Direction.Nature + BiomeAxisDrift.W * 0.1f, 0.f, 1.f);
        NormalizeDirection(Blended.Direction);

        // 9. Нелинейное искажение Morok
        float MorokPower = FMath::Clamp(Blended.Meta.Distortion, 0.f, 1.f);
        if (MorokPower > 0.05f)
        {
            float mix = MorokPower * 0.7f;
            float k = (Random01(Rng) * 2.0f - 1.0f) * MorokPower * 0.5f;
            float B = Blended.Direction.Body;
            float M = Blended.Direction.Mind;
            float S = Blended.Direction.Spirit;
            float N = Blended.Direction.Nature;
            Blended.Direction.Body = (1.0f - mix) * B + k * M + mix * S;
            Blended.Direction.Mind = -k * B + (1.0f - mix) * M + mix * N;
            Blended.Direction.Spirit = mix * B + (1.0f - mix) * S + k * N;
            Blended.Direction.Nature = mix * M - k * S + (1.0f - mix) * N;
            float scale = 1.0f + MorokPower * 0.5f;
            Blended.Direction.Body *= scale;
            Blended.Direction.Mind *= scale;
            Blended.Direction.Spirit *= scale;
            Blended.Direction.Nature *= scale;
            NormalizeDirection(Blended.Direction);
        }

        // 10. Структурирование Zaryana
        float ZaryanaStrength = FMath::Clamp(Blended.Meta.Stability, 0.f, 1.f);
        if (ZaryanaStrength > 0.05f)
        {
            float avg = (Blended.Direction.Body + Blended.Direction.Mind + Blended.Direction.Spirit + Blended.Direction.Nature) / 4.0f;
            auto enhance = [&](float& val) {
                if (val > avg) val *= (1.0f + ZaryanaStrength * 0.5f);
                else val *= (1.0f - ZaryanaStrength * 0.3f);
            };
            enhance(Blended.Direction.Body);
            enhance(Blended.Direction.Mind);
            enhance(Blended.Direction.Spirit);
            enhance(Blended.Direction.Nature);
            float tanhScale = 1.0f + ZaryanaStrength * 0.8f;
            Blended.Direction.Body = FMath::Tanh(Blended.Direction.Body * tanhScale);
            Blended.Direction.Mind = FMath::Tanh(Blended.Direction.Mind * tanhScale);
            Blended.Direction.Spirit = FMath::Tanh(Blended.Direction.Spirit * tanhScale);
            Blended.Direction.Nature = FMath::Tanh(Blended.Direction.Nature * tanhScale);
            NormalizeDirection(Blended.Direction);
        }

        // 11. Бифуркация (коллапс/очищение)
        const float BifurcationThreshold = 0.92f;
        if (Blended.Meta.Distortion > BifurcationThreshold)
        {
            float excess = (Blended.Meta.Distortion - BifurcationThreshold) / (1.0f - BifurcationThreshold);
            float chance = FMath::Clamp(FMath::Pow(excess, 1.5f) * 0.8f, 0.f, 0.95f);
            if (Random01(Rng) < chance)
            {
                bool bCollapse = (Random01(Rng) < 0.5f);
                if (bCollapse)
                {
                    Blended.Meta.Distortion = FMath::Clamp(Blended.Meta.Distortion * 0.3f, 0.1f, 0.4f);
                    Blended.Meta.Stability = FMath::Clamp(Blended.Meta.Stability - 0.2f, 0.f, 1.f);
                    Blended.Meta.Purity = FMath::Clamp(Blended.Meta.Purity - 0.2f, 0.f, 1.f);
                }
                else
                {
                    Blended.Meta.Distortion = FMath::Clamp(Blended.Meta.Distortion * 0.6f, 0.3f, 0.5f);
                    Blended.Meta.Stability = FMath::Clamp(Blended.Meta.Stability + 0.2f, 0.f, 1.f);
                    Blended.Meta.Purity = FMath::Clamp(Blended.Meta.Purity + 0.2f, 0.f, 1.f);
                }
            }
        }

        Blended.Magnitude = FMath::Clamp(Blended.Magnitude, 0.f, 1.f);
        Blended.Meta.Distortion = FMath::Clamp(Blended.Meta.Distortion, 0.f, 1.f);
        Blended.Meta.Stability = FMath::Clamp(Blended.Meta.Stability, 0.f, 1.f);
        Blended.Meta.Purity = FMath::Clamp(Blended.Meta.Purity, 0.f, 1.f);
        Blended.Meta.Potency = FMath::Clamp(Blended.Meta.Potency, 0.f, 1.f);
        Blended.Meta.Resonance = FMath::Clamp(Blended.Meta.Resonance, 0.f, 1.f);
        Blended.Meta.Corruption = FMath::Clamp(Blended.Meta.Corruption, 0.f, 1.f);

        return Blended;
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

        // ВОДА: не деградирует
        if (Cell->bIsWater)
        {
            FInventoryItem WaterItem;
            WaterItem.IngredientID = Cell->WaterTypeID.IsNone() ? FName(TEXT("Water")) : Cell->WaterTypeID;
            WaterItem.State = Cell->State;
            WaterItem.Count = 1;
            WaterItem.CreationTime = 0.0;
            WaterItem.bSubjectToDecay = true;

            FInventoryOperation Op;
            Op.ContainerID = 0;
            Op.Ingredient = WaterItem;
            Op.OpType = EInventoryOpType::Add;
            Op.Amount = 1;
            OutDelta.InventoryOps.Add(Op);
            UE_LOG(LogHerbalist, Verbose, TEXT("Water harvested at (%d,%d)"), Cmd.TargetCell.X, Cmd.TargetCell.Y);
            return;
        }

        // РАСТЕНИЯ: деградация
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

        FInventoryItem Harvested = GenerateHarvestResult(*Cell, Cmd.IngredientID, Rng);

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
                                   FRandomStream& Rng,
                                   FStateDelta& OutDelta)
    {
        // Вычисляем результат с использованием расширенной алхимии
        FRealState PotionState = ComputeAdvancedApplyResult(Cmd.Ingredients, Cmd.Intent,
                                                           WorldSnap, Cmd.TargetCell,
                                                           Cmd.BiomeMorokField, Cmd.BiomeZaryanaField, Cmd.BiomeAxisDrift,
                                                           Rng);

        // Удаляем ингредиенты
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

        if (Cmd.bIsCrafting)
        {
            // Крафт: добавляем зелье в инвентарь
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
            UE_LOG(LogHerbalist, Log, TEXT("Crafted potion: M=%.2f, Dist=%.2f, Purity=%.2f"),
                   PotionState.Magnitude, PotionState.Meta.Distortion, PotionState.Meta.Purity);
            return;
        }

        // Применение на клетку
        if (Cmd.Ingredients.Num() != 1 || Cmd.Ingredients[0].IngredientID != FName(TEXT("Potion")))
        {
            UE_LOG(LogHerbalist, Warning, TEXT("Apply on cell requires exactly one Potion item"));
            return;
        }

        const FGridCell* Cell = WorldSnap.GridState.Find(Cmd.TargetCell);
        if (!Cell)
        {
            UE_LOG(LogHerbalist, Warning, TEXT("Apply target cell not found"));
            return;
        }

        FGridCell Modified = *Cell;
        Modified.State = PotionState;
        Modified.HarvestStress = FMath::Clamp(Cell->HarvestStress + 0.2f, 0.f, 1.f);
        OutDelta.WorldChanges.Add(Cmd.TargetCell, Modified);
        OutDelta.bIsPotionEffect = true;
        UE_LOG(LogHerbalist, Log, TEXT("Applied potion to cell (%d,%d)"),
               Cmd.TargetCell.X, Cmd.TargetCell.Y);
    }

    // ---------------------------------------------------------
    // Главная точка входа
    // ---------------------------------------------------------
    FStateDelta ExecutePipeline(const FWorldSnapshot& WorldSnapshot,
                                const FInventorySnapshot& InventorySnapshot,
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
                ProcessApplyCommand(Entry.Apply, WorldSnapshot, Rng, Delta);
                break;
            default:
                break;
            }
        }

        return Delta;
    }
}