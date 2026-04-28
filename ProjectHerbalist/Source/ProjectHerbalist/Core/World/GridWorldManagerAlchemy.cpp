// Core/World/GridWorldManagerAlchemy.cpp
#include "Core/World/GridWorldManager.h"
#include "ProjectHerbalist.h"
#include "Core/Pipeline/HerbalistPipeline.h"
#include "Core/Pipeline/AlchemySemantics.h"
#include "Core/Pipeline/AlchemySemanticResolver.h"
#include "Core/Pipeline/AlchemyPhysicsPipeline.h"
#include "Core/Pipeline/AlchemyWorldStateApplier.h"
#include "Core/Pipeline/AlchemyTypes.h"
#include "Core/Pipeline/IntentResolver.h"
#include "Core/Pipeline/PipelineTypes.h"
#include "Core/Pipeline/AlchemyPipelineFacade.h"
#include "Core/Subsystems/IngredientRegistrySubsystem.h"
#include "Core/BiomeGraph/BiomeGraphSubsystem.h"
#include "Player/HerbalistPlayerController.h"
#include "Core/HerbalistSettings.h"
#include "Core/Types/BiomeTypes.h"

// ============================================================================
// ПРИМЕНЕНИЕ АЛХИМИИ
// ============================================================================

void AGridWorldManager::ApplyAlchemyResult(int32 X, int32 Y, const TArray<FInventoryItem>& Ingredients, const FIntent& Intent, FRngState& Rng)
{
    FGridCell* Cell = GetCell(X, Y);
    if (!Cell) return;

    UGameInstance* GameInstance = GetGameInstance();
    UIngredientRegistrySubsystem* IngredientSubsystem = GameInstance ? GameInstance->GetSubsystem<UIngredientRegistrySubsystem>() : nullptr;

    // Биомный контекст
    float BiomeMorokField = 0.0f;
    float BiomeZaryanaField = 0.0f;
    FVector4 BiomeAxisDrift(0.25f, 0.25f, 0.25f, 0.25f);

    if (UBiomeGraphSubsystem* Graph = GetWorld()->GetSubsystem<UBiomeGraphSubsystem>())
    {
        FName BiomeID = FBiomeDefaults::BiomeTypeToName(Cell->Biome);
        if (const FBiomeGraphNode* Node = Graph->GetNode(BiomeID))
        {
            BiomeMorokField   = Node->MorokField;
            BiomeZaryanaField = Node->ZaryanaField;
            BiomeAxisDrift    = Node->Memory.AxisDrift;
        }
    }

    // Сохраняем старое состояние для дельты
    const FRealState OldState = Cell->State;

    // Выполняем алхимию через фасад
    const FAlchemyFacadeResult AlchemyResult = FAlchemyPipelineFacade::Execute(
        Ingredients,
        Cell->State,
        Cell->Environment,
        Cell->Memory,
        Cell->Memory.AccumulatedDistortion,
        IngredientSubsystem,
        BiomeMorokField,
        BiomeZaryanaField,
        BiomeAxisDrift,
        Rng);

    const FRealState& NewState = AlchemyResult.FinalState;

    UE_LOG(LogHerbalist, Log, TEXT("Alchemy outcome: %d"), static_cast<int32>(AlchemyResult.Outcome));

    // Дельта и след в биомном графе
    const FDeltaState Delta = HerbalistCore::ComputeDelta(OldState, NewState);

    if (UBiomeGraphSubsystem* Graph = GetWorld()->GetSubsystem<UBiomeGraphSubsystem>())
    {
        const FName BiomeID = FBiomeDefaults::BiomeTypeToName(Cell->Biome);
        const float MorokImpact   = Delta.MetaDelta.Distortion;
        const float ZaryanaImpact = 1.0f - Delta.MetaDelta.Distortion;
        const FVector4 AxisDelta(
            Delta.DirectionDelta.Body,
            Delta.DirectionDelta.Mind,
            Delta.DirectionDelta.Spirit,
            Delta.DirectionDelta.Nature);

        Graph->RecordFootprint(BiomeID, MorokImpact, ZaryanaImpact, AxisDelta, 1.0f);
    }

    // Прямое применение к клетке (без интерполяции)
    Cell->State       = NewState;
    Cell->TargetState = NewState;

    // Распространение на соседей
    FRealState DeltaForPropagation;
    DeltaForPropagation.Magnitude = Delta.MagnitudeDelta;
    DeltaForPropagation.Direction = Delta.DirectionDelta;
    DeltaForPropagation.Meta      = Delta.MetaDelta;

    PropagateToNeighbors(X, Y, DeltaForPropagation, 0.5f, PropagationDepth);
}

// ============================================================================
// ПЕРЕГРУЗКА ДЛЯ СЫРЫХ СОСТОЯНИЙ
// ============================================================================

void AGridWorldManager::ApplyAlchemyResult(int32 X, int32 Y, const TArray<FRealState>& Ingredients, const FIntent& Intent, FRngState& Rng)
{
    TArray<FInventoryItem> Items;
    Items.Reserve(Ingredients.Num());
    for (const FRealState& State : Ingredients)
    {
        FInventoryItem Item;
        Item.IngredientID = NAME_None;
        Item.State        = State;
        Item.Count        = 1;
        Items.Add(MoveTemp(Item));
    }
    ApplyAlchemyResult(X, Y, Items, Intent, Rng);
}

// ============================================================================
// РАСПРОСТРАНЕНИЕ НА СОСЕДЕЙ
// ============================================================================

void AGridWorldManager::PropagateToNeighbors(int32 X, int32 Y, const FRealState& Delta, float Falloff, int32 Depth)
{
    if (Depth <= 0) return;

    struct FPropagationNode
    {
        int32 X;
        int32 Y;
        int32 RemainingDepth;
        FRealState CurrentDelta;
    };

    TQueue<FPropagationNode> Queue;
    Queue.Enqueue({ X, Y, Depth, Delta });
    TSet<int32> Visited;
    Visited.Add(GetCellIndex(X, Y));

    while (!Queue.IsEmpty())
    {
        FPropagationNode Node;
        Queue.Dequeue(Node);
        if (Node.RemainingDepth <= 0) continue;

        for (int32 dx = -1; dx <= 1; ++dx)
        {
            for (int32 dy = -1; dy <= 1; ++dy)
            {
                if (dx == 0 && dy == 0) continue;

                const int32 NX = Node.X + dx;
                const int32 NY = Node.Y + dy;
                const int32 Idx = GetCellIndex(NX, NY);

                if (Visited.Contains(Idx)) continue;

                FGridCell* Neighbor = GetCell(NX, NY);
                if (!Neighbor) continue;

                Visited.Add(Idx);

                FRealState WeakDelta = Node.CurrentDelta;
                WeakDelta.Magnitude          *= Falloff;
                WeakDelta.Meta.Distortion     = FMath::Max(0.0f, WeakDelta.Meta.Distortion * Falloff);
                WeakDelta.Meta.Corruption    *= Falloff;
                WeakDelta.Meta.Stability      = 0.0f;
                WeakDelta.Meta.Purity         = 0.0f;
                WeakDelta.Meta.Potency        = 0.0f;
                WeakDelta.Meta.Resonance      = 0.0f;
                WeakDelta.Direction.Body      = 0.0f;
                WeakDelta.Direction.Mind      = 0.0f;
                WeakDelta.Direction.Spirit    = 0.0f;
                WeakDelta.Direction.Nature    = 0.0f;

                FRealState& Target = Neighbor->TargetState;
                Target.Magnitude          = FMath::Clamp(Target.Magnitude       + WeakDelta.Magnitude,       0.0f, 1.0f);
                Target.Meta.Distortion    = FMath::Clamp(Target.Meta.Distortion + WeakDelta.Meta.Distortion, 0.0f, 1.0f);
                Target.Meta.Corruption    = FMath::Clamp(Target.Meta.Corruption + WeakDelta.Meta.Corruption, 0.0f, 1.0f);

                if (Node.RemainingDepth > 1)
                {
                    Queue.Enqueue({ NX, NY, Node.RemainingDepth - 1, WeakDelta });
                }
            }
        }
    }
}

// ============================================================================
// ТЕСТОВЫЕ КОМАНДЫ
// ============================================================================

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
    PotionItem.IngredientID = FName(TEXT("Potion"));
    PotionItem.State        = PotionState;
    PotionItem.Count        = 1;

    TArray<FInventoryItem> Ingredients = { PotionItem };
    FIntent Intent;
    Intent.Coherence = 0.5f;

    FRngState Rng;
    Rng.Seed = (X * 7919) ^ (Y * 7901) ^ static_cast<int32>(Cell->Memory.AccumulatedDistortion * 10000.0f);

    ApplyAlchemyResult(X, Y, Ingredients, Intent, Rng);
}