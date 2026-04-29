// Core/Simulation/Private/PipelineV2.cpp
#include "PipelineV2.h"
#include "ProjectHerbalist.h"

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

    static FInventoryItem GenerateHarvestResult(const FGridCell& Cell,
                                               FName IngredientID,
                                               FRandomStream& Rng)
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

    static FRealState ComputeApplyResult(const TArray<FInventoryItem>& Ingredients, const FIntent& Intent, FRandomStream& Rng)
    {
        if (Ingredients.Num() == 0) return FRealState();

        FRealState Result;
        float TotalWeight = 0.f;
        for (const FInventoryItem& Item : Ingredients)
        {
            float Weight = Item.State.Magnitude * Item.Count;
            Result.Magnitude += Item.State.Magnitude * Weight;
            Result.Direction.Body += Item.State.Direction.Body * Weight;
            Result.Direction.Mind += Item.State.Direction.Mind * Weight;
            Result.Direction.Spirit += Item.State.Direction.Spirit * Weight;
            Result.Direction.Nature += Item.State.Direction.Nature * Weight;
            Result.Meta.Distortion += Item.State.Meta.Distortion * Weight;
            Result.Meta.Stability += Item.State.Meta.Stability * Weight;
            Result.Meta.Purity += Item.State.Meta.Purity * Weight;
            Result.Meta.Potency += Item.State.Meta.Potency * Weight;
            Result.Meta.Resonance += Item.State.Meta.Resonance * Weight;
            Result.Meta.Corruption += Item.State.Meta.Corruption * Weight;
            TotalWeight += Weight;
        }
        if (TotalWeight > KINDA_SMALL_NUMBER)
        {
            Result.Magnitude /= TotalWeight;
            Result.Direction.Body /= TotalWeight;
            Result.Direction.Mind /= TotalWeight;
            Result.Direction.Spirit /= TotalWeight;
            Result.Direction.Nature /= TotalWeight;
            Result.Meta.Distortion /= TotalWeight;
            Result.Meta.Stability /= TotalWeight;
            Result.Meta.Purity /= TotalWeight;
            Result.Meta.Potency /= TotalWeight;
            Result.Meta.Resonance /= TotalWeight;
            Result.Meta.Corruption /= TotalWeight;
        }

        float Coherence = FMath::Clamp(Intent.Coherence, 0.f, 1.f);
        Result.Meta.Distortion = FMath::Clamp(Result.Meta.Distortion * (1.f - Coherence) + Rng.FRandRange(0.f, 0.2f) * Coherence, 0.f, 1.f);
        Result.Meta.Stability = FMath::Clamp(Result.Meta.Stability * (1.f + Coherence * 0.5f), 0.f, 1.f);
        Result.Direction.NormalizeSum();
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
        
        // Обработка воды (как в Этапе 3)
        if (Cell->bIsWater)
        {
            const float WaterDegradationStep = 0.001f;
            const float WaterStressStep = 0.0005f;
            
            FGridCell Modified = *Cell;
            Modified.HarvestStress = FMath::Clamp(Cell->HarvestStress + WaterStressStep, 0.0f, 1.0f);
            Modified.State.Meta.Distortion = FMath::Clamp(Cell->State.Meta.Distortion + WaterDegradationStep, 0.0f, 1.0f);
            Modified.State.Meta.Purity = FMath::Clamp(Cell->State.Meta.Purity - WaterDegradationStep, 0.0f, 1.0f);
            Modified.State.Meta.Stability = FMath::Clamp(Cell->State.Meta.Stability - WaterDegradationStep, 0.0f, 1.0f);
            Modified.State.Magnitude = FMath::Clamp(Cell->State.Magnitude - WaterDegradationStep * 0.5f, 0.0f, 1.0f);
            Modified.Memory.AccumulatedDistortion = Modified.State.Meta.Distortion;
            
            FInventoryItem WaterItem;
            WaterItem.IngredientID = Cell->WaterTypeID.IsNone() ? FName(TEXT("Water")) : Cell->WaterTypeID;
            WaterItem.State = Cell->State;
            WaterItem.State.Magnitude = FMath::Clamp(Cell->State.Magnitude - 0.001f, 0.0f, 1.0f);
            WaterItem.Count = 1;
            WaterItem.CreationTime = 0.0;
            WaterItem.bSubjectToDecay = true;
            
            FInventoryOperation Op;
            Op.ContainerID = 0;
            Op.Ingredient = WaterItem;
            Op.OpType = EInventoryOpType::Add;
            Op.Amount = 1;
            OutDelta.InventoryOps.Add(Op);
            OutDelta.WorldChanges.Add(Cmd.TargetCell, Modified);
            
            UE_LOG(LogHerbalist, Verbose, TEXT("Water harvested at (%d,%d)"), Cmd.TargetCell.X, Cmd.TargetCell.Y);
            return;
        }

        // Обработка растений (деградация)
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
        // 1. Вычисляем результирующее состояние зелья
        FRealState PotionState = ComputeApplyResult(Cmd.Ingredients, Cmd.Intent, Rng);
        
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
        
        // 3. Если крафт – создаём зелье в инвентаре и выходим
        if (Cmd.bIsCrafting)
        {
            FInventoryItem PotionItem;
            PotionItem.IngredientID = FName(TEXT("Potion"));
            PotionItem.State = PotionState;
            PotionItem.Count = 1;
            PotionItem.CreationTime = 0.0;
            PotionItem.bSubjectToDecay = false;   // зелья не портятся
            
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
        
        // 4. Иначе – применение на клетку (старая логика)
        const FGridCell* Cell = WorldSnap.GridState.Find(Cmd.TargetCell);
        if (!Cell)
        {
            UE_LOG(LogHerbalist, Warning, TEXT("Apply target cell (%d,%d) not found"), Cmd.TargetCell.X, Cmd.TargetCell.Y);
            return;
        }
        
        FGridCell Modified = *Cell;
        Modified.State = PotionState;
        Modified.HarvestStress = FMath::Clamp(Cell->HarvestStress + 0.2f, 0.f, 1.f);
        OutDelta.WorldChanges.Add(Cmd.TargetCell, Modified);
        
        UE_LOG(LogHerbalist, Log, TEXT("Applied potion to cell (%d,%d): M=%.2f, Dist=%.2f"),
            Cmd.TargetCell.X, Cmd.TargetCell.Y, PotionState.Magnitude, PotionState.Meta.Distortion);
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