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
        if (Cell->bIsWater)
        {
            UE_LOG(LogHerbalist, Verbose, TEXT("PipelineV2: Skipped water cell (%d,%d)"), Cmd.TargetCell.X, Cmd.TargetCell.Y);
            return;
        }

        FInventoryItem Harvested = GenerateHarvestResult(*Cell, Cmd.IngredientID, Rng);

        FInventoryOperation Op;
        Op.ContainerID = 0;   // игрок
        Op.Ingredient = Harvested;
        Op.OpType = EInventoryOpType::Add;
        Op.Amount = Cmd.Amount;
        OutDelta.InventoryOps.Add(Op);

        FGridCell Modified = *Cell;
        Modified.HarvestStress = FMath::Clamp(Cell->HarvestStress + 0.1f, 0.0f, 1.0f);
        OutDelta.WorldChanges.Add(Cmd.TargetCell, Modified);
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

        // Remove из источника
        FInventoryOperation RemoveOp;
        RemoveOp.ContainerID = Cmd.SourceContainerID;
        RemoveOp.Ingredient = *SourceItem;
        RemoveOp.OpType = EInventoryOpType::Remove;
        RemoveOp.Amount = Cmd.Amount;
        OutDelta.InventoryOps.Add(RemoveOp);

        // Add в цель
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
		const FGridCell* Cell = WorldSnap.GridState.Find(Cmd.TargetCell);
		if (!Cell)
		{
			UE_LOG(LogHerbalist, Warning, TEXT("PipelineV2: Apply target cell (%d,%d) not found"), Cmd.TargetCell.X, Cmd.TargetCell.Y);
			return;
		}

		FRealState PotionState = ComputeApplyResult(Cmd.Ingredients, Cmd.Intent, Rng);

		// Удаляем использованные ингредиенты из инвентаря
		for (const FInventoryItem& Ing : Cmd.Ingredients)
		{
			FInventoryOperation RemoveOp;
			RemoveOp.ContainerID = 0;   // инвентарь игрока
			RemoveOp.Ingredient = Ing;
			RemoveOp.Ingredient.Count = 1;
			RemoveOp.OpType = EInventoryOpType::Remove;
			RemoveOp.Amount = 1;
			OutDelta.InventoryOps.Add(RemoveOp);
		}

		FGridCell Modified = *Cell;
		Modified.State = PotionState;
		Modified.HarvestStress = FMath::Clamp(Cell->HarvestStress + 0.2f, 0.f, 1.f);

		OutDelta.WorldChanges.Add(Cmd.TargetCell, Modified);
		UE_LOG(LogHerbalist, Log, TEXT("PipelineV2: Apply to cell (%d,%d) - new state M=%.2f, Dist=%.2f"),
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