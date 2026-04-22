// GridWorldManagerAlchemy.cpp
#include "GridWorldManager.h"
#include "ProjectHerbalist.h"
#include "Core/Pipeline/HerbalistPipeline.h"
#include "Core/BiomeGraph/BiomeGraphSubsystem.h"
#include "Player/HerbalistPlayerController.h"
#include "Core/HerbalistSettings.h"

void AGridWorldManager::ApplyAlchemyResult(int32 X, int32 Y, const TArray<FInventoryItem>& Ingredients, const FIntent& Intent, FRngState& Rng)
{
    FGridCell* Cell = GetCell(X, Y);
    if (!Cell) return;

    // Получаем контекст биома из графа
    float BiomeMorokField = 0.0f;
    float BiomeZaryanaField = 0.0f;
    FVector4 BiomeAxisDrift = FVector4(0.25f, 0.25f, 0.25f, 0.25f);

    if (UBiomeGraphSubsystem* Graph = GetWorld()->GetSubsystem<UBiomeGraphSubsystem>())
    {
        FName BiomeID = FBiomeDefaults::BiomeTypeToName(Cell->Biome);
        if (const FBiomeGraphNode* Node = Graph->GetNode(BiomeID))
        {
            BiomeMorokField = Node->MorokField;
            BiomeZaryanaField = Node->ZaryanaField;
            BiomeAxisDrift = Node->Memory.AxisDrift;
        }
    }

    FRealState OldState = Cell->State;
    FRealState NewState = HerbalistCore::Pipeline::ApplyMorok(
        Ingredients,
        Cell->State,
        Cell->Environment,
        Cell->Memory,
        Intent,
        Rng,
        BiomeMorokField,
        BiomeZaryanaField,
        BiomeAxisDrift
    );

    // Бифуркация (катастрофа / очищение)
    const UHerbalistSettings* Settings = GetHerbalistSettings();
    const float BifurcationThreshold = Settings ? Settings->BifurcationThreshold : 0.85f;
    constexpr float MaxDistortion = 1.0f;

    if (NewState.Meta.Distortion > BifurcationThreshold)
    {
        float Excess = (NewState.Meta.Distortion - BifurcationThreshold) / (MaxDistortion - BifurcationThreshold);
        float BaseChance = FMath::Pow(Excess, 1.5f);
        float Instability = (1.0f - NewState.Meta.Stability) * (1.0f - NewState.Meta.Purity);
        float InstabilityFactor = FMath::Lerp(0.5f, 1.0f, Instability);
        float MemoryFactor = 1.0f - Cell->Memory.StabilityMemory * 0.7f;
        float EventChance = BaseChance * InstabilityFactor * MemoryFactor;
        EventChance = FMath::Clamp(EventChance, 0.0f, 0.95f);

        if (HerbalistCore::Random01(Rng) < EventChance)
        {
            bool bCollapse = HerbalistCore::Random01(Rng) < 0.5f;
            if (bCollapse)
            {
                NewState.Meta.Distortion = 0.2f;
                NewState.Meta.Stability = FMath::Clamp(NewState.Meta.Stability + 0.1f, 0.0f, 1.0f);
                NewState.Direction.Body = FMath::Lerp(NewState.Direction.Body, 0.25f, 0.1f);
                NewState.Direction.Mind = FMath::Lerp(NewState.Direction.Mind, 0.25f, 0.1f);
                NewState.Direction.Spirit = FMath::Lerp(NewState.Direction.Spirit, 0.25f, 0.1f);
                NewState.Direction.Nature = FMath::Lerp(NewState.Direction.Nature, 0.25f, 0.1f);
                NewState.Direction.NormalizeSum();
                UE_LOG(LogHerbalist, Warning, TEXT("[CATASTROPHE] COLLAPSE! Distortion reset to 0.2, Stability+0.1"));
            }
            else
            {
                NewState.Meta.Distortion = 0.4f;
                float Boost = 0.3f * (1.0f - NewState.Meta.Stability);
                NewState.Meta.Stability = FMath::Clamp(NewState.Meta.Stability + Boost, 0.0f, 1.0f);
                NewState.Meta.Purity = FMath::Clamp(NewState.Meta.Purity + Boost * 0.8f, 0.0f, 1.0f);
                NewState.Direction.Body = FMath::Lerp(NewState.Direction.Body, 0.25f, 0.2f);
                NewState.Direction.Mind = FMath::Lerp(NewState.Direction.Mind, 0.25f, 0.2f);
                NewState.Direction.Spirit = FMath::Lerp(NewState.Direction.Spirit, 0.25f, 0.2f);
                NewState.Direction.Nature = FMath::Lerp(NewState.Direction.Nature, 0.25f, 0.2f);
                NewState.Direction.NormalizeSum();
                UE_LOG(LogHerbalist, Warning, TEXT("[CATASTROPHE] PURIFICATION! Distortion reset to 0.4, Stability+%.2f, Purity+%.2f"), Boost, Boost * 0.8f);
            }
        }
    }

    // Вычисляем дельту
    FRealState Delta;
    Delta.Magnitude = NewState.Magnitude - OldState.Magnitude;
    Delta.Direction.Body = NewState.Direction.Body - OldState.Direction.Body;
    Delta.Direction.Mind = NewState.Direction.Mind - OldState.Direction.Mind;
    Delta.Direction.Spirit = NewState.Direction.Spirit - OldState.Direction.Spirit;
    Delta.Direction.Nature = NewState.Direction.Nature - OldState.Direction.Nature;
    Delta.Meta.Distortion = NewState.Meta.Distortion - OldState.Meta.Distortion;
    Delta.Meta.Stability = NewState.Meta.Stability - OldState.Meta.Stability;
    Delta.Meta.Purity = NewState.Meta.Purity - OldState.Meta.Purity;
    Delta.Meta.Potency = NewState.Meta.Potency - OldState.Meta.Potency;
    Delta.Meta.Resonance = NewState.Meta.Resonance - OldState.Meta.Resonance;
    Delta.Meta.Corruption = NewState.Meta.Corruption - OldState.Meta.Corruption;

    if (UBiomeGraphSubsystem* Graph = GetWorld()->GetSubsystem<UBiomeGraphSubsystem>())
    {
        FName BiomeID = FBiomeDefaults::BiomeTypeToName(Cell->Biome);
        float MorokImpact = Delta.Meta.Distortion;
        float ZaryanaImpact = 1.f - Delta.Meta.Distortion;
        FVector4 AxisDelta(Delta.Direction.Body, Delta.Direction.Mind, Delta.Direction.Spirit, Delta.Direction.Nature);
        Graph->RecordFootprint(BiomeID, MorokImpact, ZaryanaImpact, AxisDelta, 1.0f);
    }

    SetTargetState(X, Y, NewState);

    PropagateToNeighbors(X, Y, Delta, 0.5f, PropagationDepth);
}

void AGridWorldManager::ApplyAlchemyResult(int32 X, int32 Y, const TArray<FRealState>& Ingredients, const FIntent& Intent, FRngState& Rng)
{
    TArray<FInventoryItem> Items;
    for (const FRealState& State : Ingredients)
    {
        FInventoryItem Item;
        Item.Type = EResourceType::None;
        Item.State = State;
        Item.Count = 1;
        Items.Add(Item);
    }
    ApplyAlchemyResult(X, Y, Items, Intent, Rng);
}

void AGridWorldManager::PropagateToNeighbors(int32 X, int32 Y, const FRealState& Delta, float Falloff, int32 Depth)
{
    if (Depth <= 0) return;

    struct FPropagationNode
    {
        int32 X, Y;
        int32 RemainingDepth;
        FRealState CurrentDelta;
    };

    TQueue<FPropagationNode> Queue;
    Queue.Enqueue({ X, Y, Depth, Delta });
    TSet<int32> Visited;

    while (!Queue.IsEmpty())
    {
        FPropagationNode Node;
        Queue.Dequeue(Node);
        if (Node.RemainingDepth <= 0) continue;

        for (int32 dx = -1; dx <= 1; dx++)
        {
            for (int32 dy = -1; dy <= 1; dy++)
            {
                if (dx == 0 && dy == 0) continue;
                int32 NX = Node.X + dx;
                int32 NY = Node.Y + dy;
                int32 Idx = GetCellIndex(NX, NY);
                if (Visited.Contains(Idx)) continue;
                FGridCell* Neighbor = GetCell(NX, NY);
                if (!Neighbor) continue;

                Visited.Add(Idx);

                FRealState WeakDelta = Node.CurrentDelta;
                WeakDelta.Magnitude *= Falloff;
                WeakDelta.Meta.Distortion = FMath::Max(0.0f, WeakDelta.Meta.Distortion * Falloff);
                WeakDelta.Meta.Corruption *= Falloff;
                WeakDelta.Meta.Stability = 0.0f;
                WeakDelta.Meta.Purity = 0.0f;
                WeakDelta.Meta.Potency = 0.0f;
                WeakDelta.Meta.Resonance = 0.0f;
                WeakDelta.Direction.Body = 0.0f;
                WeakDelta.Direction.Mind = 0.0f;
                WeakDelta.Direction.Spirit = 0.0f;
                WeakDelta.Direction.Nature = 0.0f;

                FRealState NewTargetState = Neighbor->TargetState;
                NewTargetState.Magnitude += WeakDelta.Magnitude;
                NewTargetState.Meta.Distortion += WeakDelta.Meta.Distortion;
                NewTargetState.Meta.Corruption += WeakDelta.Meta.Corruption;
                NewTargetState.Magnitude = FMath::Clamp(NewTargetState.Magnitude, 0.0f, 1.0f);
                NewTargetState.Meta.Distortion = FMath::Clamp(NewTargetState.Meta.Distortion, 0.0f, 1.0f);
                NewTargetState.Meta.Corruption = FMath::Clamp(NewTargetState.Meta.Corruption, 0.0f, 1.0f);

                Neighbor->TargetState = NewTargetState;
                MarkDirty(NX, NY);

                if (Node.RemainingDepth - 1 > 0)
                {
                    Queue.Enqueue({ NX, NY, Node.RemainingDepth - 1, WeakDelta });
                }
            }
        }
    }
}

void AGridWorldManager::ApplyTest(int32 X, int32 Y)
{
    AHerbalistPlayerController* PC = Cast<AHerbalistPlayerController>(GetWorld()->GetFirstPlayerController());
    if (!PC || !PC->InventoryComponent)
    {
        UE_LOG(LogHerbalist, Warning, TEXT("No player controller or inventory component found"));
        return;
    }

    TArray<FInventoryItem> Inventory = PC->InventoryComponent->GetItems();
    if (Inventory.Num() < 2)
    {
        UE_LOG(LogHerbalist, Warning, TEXT("Need at least 2 resources in inventory"));
        return;
    }

    FInventoryItem Ingredient1 = Inventory[0];
    FInventoryItem Ingredient2 = Inventory[1];
    PC->InventoryComponent->RemoveItem(0, 1);
    PC->InventoryComponent->RemoveItem(1, 1);

    TArray<FInventoryItem> Ingredients = { Ingredient1, Ingredient2 };
    FIntent Intent;
    Intent.Coherence = 0.5f;
    FRngState Rng;
    Rng.Seed = 12345;
    ApplyAlchemyResult(X, Y, Ingredients, Intent, Rng);
    UE_LOG(LogHerbalist, Log, TEXT("Applied alchemy to (%d,%d) and consumed two resources"), X, Y);
}

void AGridWorldManager::ApplyPotionToCell(int32 X, int32 Y, const FRealState& PotionState)
{
    FGridCell* Cell = GetCell(X, Y);
    if (!Cell) return;

    FInventoryItem PotionItem;
    PotionItem.Type = EResourceType::Potion;
    PotionItem.State = PotionState;
    PotionItem.Count = 1;
    TArray<FInventoryItem> Ingredients = { PotionItem };

    FIntent Intent;
    Intent.Coherence = 0.5f;
    FRngState Rng;
    Rng.Seed = FMath::Rand();

    ApplyAlchemyResult(X, Y, Ingredients, Intent, Rng);
}